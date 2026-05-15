# ARK — Windows 内核 Anti‑Rootkit / 系统取证工具

PCHunter 风格的 Windows x64 内核取证工具，由一个 KMDF/Mini‑Filter 驱动（`MyDriver64.sys`）和一个 MFC 上位机（`MyPCHunter64.exe`）组成。

## 功能模块

| 分类 | 模块 |
| --- | --- |
| 进程 | 进程列表、线程、模块、句柄、VAD、内存读写、进程监控 |
| 内核 | 驱动模块、Kernel CallBack、Object CallBack、MiniFilter CallBack、SSDT / Shadow SSDT、IDT / GDT / HAL Table、DPC、Worker Thread、WDF、内核中断表 |
| 钩子 | **内核 Inline Hook 检测**（按 PDB 函数入口 vs 磁盘 PE `.text` 字节比对）+ 选中行**反汇编窗口** |
| 文件 | 文件枚举、解除占用、强删 |
| 注册表 | 注册表枚举 |
| 设置 | 调试标志开关 |

## 项目架构

```
┌──────────────────────── R3 (Ring 3) ────────────────────────┐
│  MyPCHunter64.exe (MFC, x64)                                │
│   ├─ MyPCHunter64Dlg          主对话框 + Tab 容器           │
│   ├─ Dlg* (Process/Driver/…)  各功能页                      │
│   ├─ CLoadDriver              加载/卸载驱动 + IPC 派发      │
│   ├─ CThreadPool              UI 线程池（避免阻塞 UI）      │
│   ├─ KernelHookDetect         内核 Inline Hook 检测核心    │
│   ├─ PdbResolver              异步下载 + 解析 PDB          │
│   │                           （MS 公共符号服务器 + Capstone）│
│   ├─ Pdb/SymLoader            基于 DbgHelp 的 PDB 包装     │
│   └─ Watchdog (--watchdog)    自身 spawn 子进程，主进程被任 │
│                               务管理器杀也能 ControlService │
│                               STOP + DeleteService          │
│                                                            │
│      ↕ FilterSendMessage (MiniFilter 通信 Port)            │
│      Port: \58DF4FB5-D464-4DF9-B14E-3565FDE02AED            │
│                                                            │
├──────────────────────── R0 (Ring 0) ────────────────────────┤
│  MyDriver64.sys (KMDF + FltMgr)                            │
│   ├─ MainDriver / Interface   命令派发 (g_CmdFun[])         │
│   ├─ FindModuleData           手工定位未导出的内核变量      │
│   │  (PspCreateProcess..., KiServiceTable, 等)              │
│   ├─ OffsetDefaults_*.inc     各内核版本结构体偏移          │
│   │  (7600/7601/9200/9600/10240..26200)                     │
│   ├─ Ssdt / Filter / File…    业务模块                      │
│   ├─ Hook/                    SSDT hook 框架                │
│   └─ Communication            MiniFilter Port + 消息队列   │
└─────────────────────────────────────────────────────────────┘
```

### 用到的关键知识点

- **MiniFilter 通信 Port**：`FltCreateCommunicationPort` / `FltSendMessage` 派发 R3 命令并在请求线程上下文同步执行（可以直接 `ProbeForWrite` 用户缓冲区）。
- **Designated initializer 派发表**：`g_CmdFun[cmd] = {cmd, handler}` 用稀疏初始化器代替开 switch，方便插命令。
- **OS 版本偏移表**：根据 `RtlGetVersion()` 在 `OffsetDefaults_*.inc` 中切换内核结构体偏移（`EPROCESS`、`ETHREAD`、`KTHREAD`、`KPCR` 等）。
- **未导出符号定位**：在 `FindModuleData.c` 中按指令模式扫描 `ntoskrnl` 的 `.text` 拿到 `PspCreateProcessNotifyRoutine` / `PspCreateThreadNotifyRoutine` / `KeServiceDescriptorTableShadow` 等内部变量地址。
- **PDB 解析**：上位机内嵌 MS DbgHelp + `SymSrv`，从 `https://msdl.microsoft.com/download/symbols` 异步下载 PDB，缓存到 `C:\Symbols\`；通过 `SymEnumSymbolsW` 枚举 `ntoskrnl` 全部函数入口。
- **内核 Inline Hook 检测**：
  - R3 把 `ntoskrnl.exe` 以 `SEC_IMAGE_NO_EXECUTE` 风格 `MapViewOfFile` 进来当"原始字节参考"
  - 调 `PdbResolver_EnumKernelFunctions` 拿所有 `.text` 内函数入口（含 RVA 与 KVA）
  - 通过 `um_Cmd_Probe_KernelMemory_info` 一次性批量读 256 个函数前 16 字节
  - 与磁盘 `.text` 对应偏移 `memcmp`，差异即为 Hook 候选；`ClassifyHook` 根据 `0xE9 / 0xFF25 / 48B8…FFE0 / CC / C3 / 68…C3` 等模式判定类型；Capstone 反汇编出可读字符串
- **反汇编**：Capstone 5.0 (`CS_ARCH_X86 / CS_MODE_64`)，对未识别字节开 `CS_OPT_SKIPDATA` 避免一遇到非法 opcode 就 `cnt=0`。
- **看门狗**：主进程加载驱动成功后 `CreateProcess(self, "--watchdog <pid>")` spawn 一个隐藏副本，子进程 `WaitForSingleObject(主进程, INFINITE)` 后调用 SCM `ControlService(STOP)+DeleteService`。`TerminateProcess` 也能兜底卸载。
- **崩溃兜底**：`SetUnhandledExceptionFilter` + `signal` + `std::set_terminate` + `_set_invalid_parameter_handler` + `atexit`，所有路径汇集到 `EmergencyShutdown` 卸载驱动。
- **进程级日志**：纯 Win32 (`CreateFileW`/`WriteFile + FILE_FLAG_WRITE_THROUGH`)，在全局对象构造期就工作，方便诊断 InitInstance 之前的崩溃。

## 编译环境

- **OS**：Windows 10/11 x64
- **Visual Studio 2022 17.x 或 2026 (VS18) Preview**，带"C++ MFC for v143"工作负载
- **Windows SDK / WDK 10.0.26100.0**（KMDF 1.33）
- **MSBuild** 路径示例：
  - VS 2022：`C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe`
  - VS 2026 Preview：`C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`
- **第三方依赖**（已随仓库提供）：
  - `MyPCHunter64\include\capstone-5.0-Release\`：Capstone x64 头文件 + 静态 lib
  - `capstone-6.0.0-Alpha6\include\`：备用 Capstone 头（驱动暂未使用）
  - `MyPCHunter64\include\Pdb\`：基于 DbgHelp 的轻量 PDB 加载封装

## 已验证的运行环境

驱动 OS 偏移已覆盖以下版本（见 `MyDriver64/OffsetDefaults_*.inc`）：

| 版本 | Build | 说明 |
| --- | ---: | --- |
| Win 7 RTM | 7600 | ✔ |
| Win 7 SP1 | 7601 | ✔ |
| Win 8 | 9200 | ✔ |
| Win 8.1 | 9600 | ✔ |
| Win 10 TH1 | 10240 | ✔ |
| Win 10 RS1 | 14393 | ✔ |
| Win 10 RS2 | 15063 | ✔ |
| Win 10 RS3 | 16299 | ✔ |
| Win 10 RS4 | 17134 | ✔ |
| Win 10 RS5 | 17763 | ✔ |
| Win 10 19H1 | 18362 | ✔ |
| Win 10 19H2 | 18363 | ✔ |
| Win 10 20H1 | 19041 | ✔ |
| Win 10 20H2 | 19042 | ✔ |
| Win 10 21H2 | 19044 | ✔ |
| Win 10 22H2 | 19045 | ✔（开发主测） |
| Win 11 21H2 | 22000 | ✔ |
| Win 11 22H2 | 22621 | ✔ |
| Win 11 23H2 | 22631 | ✔ |
| Win 11 24H2 | 26100 | ✔ |
| Win 11 25H2 | 26200 | ✔ |

> 实际运行需要把目标机器设置为 **测试签名模式**（`bcdedit /set testsigning on` 后重启），或自行用企业证书签名 `MyDriver64.sys`。

## 编译步骤

### 1. 克隆仓库

```powershell
git clone https://github.com/18325984858/ARK.git
cd ARK
```

### 2. 编译驱动 `MyDriver64.sys`

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
    .\MyDriver64\MyDriver64.vcxproj `
    /p:Configuration=Release /p:Platform=x64 /nologo /v:m
```

产物：`MyDriver64\x64\Release\MyDriver64.sys`，仓库根目录提供了一份预编译的 `MyDriver64.sys`。

### 3. 编译上位机 `MyPCHunter64.exe`

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
    .\MyPCHunter64\MyPCHunter64.vcxproj `
    /p:Configuration=Release /p:Platform=x64 /nologo /v:m
```

`Post-Build` 会把 EXE 拷贝到仓库根目录的 `MyPCHunter64.exe`。

### 4. 运行

将 `MyPCHunter64.exe` 与 `MyDriver64.sys` 放在同一目录，**以管理员身份**运行 EXE。EXE 会通过 SCM 创建并启动驱动服务，正常退出/崩溃/被任务管理器杀都会自动卸载驱动。

首次进入"内核钩子"页时，PDB 会从 MS 公共符号服务器异步下载到 `C:\Symbols\`（首次大约 30‑60 秒），后续启动复用本地缓存。

## 目录结构

```
ARK/
├─ MyDriver64/              内核驱动工程 (KMDF + MiniFilter)
│   ├─ OffsetDefaults_*.inc 各 OS 版本结构体偏移
│   ├─ Hook/                SSDT hook 框架
│   ├─ DataStruct/          内核侧链表/哈希等
│   └─ Struct.h             R3↔R0 共享结构体（IPC 协议）
├─ MyPCHunter64/            上位机 MFC 工程
│   ├─ Dlg*.{cpp,h}         各功能 Tab
│   ├─ CLoadDriver.cpp      驱动加载/IPC 派发
│   ├─ KernelHookDetect.cpp 内核 Hook 扫描核心
│   ├─ PdbResolver.cpp      PDB 异步加载
│   └─ include/             Capstone / DbgHelp PDB 封装
├─ capstone-6.0.0-Alpha6/   备用 Capstone 头
├─ tools/                   辅助脚本
├─ MyDriver64.inf           驱动 INF
├─ MyDriver64.sys           预编译驱动
└─ MyPCHunter64.exe         预编译 EXE
```

## License & 免责声明

仅用于学习与系统取证研究。请勿用于非法用途；使用本项目造成的任何后果由使用者自行承担。
