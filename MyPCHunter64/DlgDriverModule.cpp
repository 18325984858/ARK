// DlgDriverModule.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgDriverModule.h"
#include "Thread.h"
#include "SvcUtil.h"
#include "CLoadDriver.h"
#include "../MyDriver64/Struct.h"

extern _LoadDriver g_LoadDriver;

// DlgDriverModule 对话框

IMPLEMENT_DYNAMIC(DlgDriverModule, CDialogEx)

DlgDriverModule::DlgDriverModule(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_DRIVERMODE, pParent)
{

}

DlgDriverModule::~DlgDriverModule()
{
}

void DlgDriverModule::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_CONTROL_DRIVERMODULE_LIST, m_CListCtrl);
}

BEGIN_MESSAGE_MAP(DlgDriverModule, CDialogEx)
	ON_WM_SIZE()
	ON_COMMAND(ID_DRIVER_MENU_COPY_NAME, &DlgDriverModule::OnDriverMenuCopyName)
	ON_COMMAND(ID_DRIVER_MENU_COPY_BASEADDR, &DlgDriverModule::OnDriverMenuCopyBaseaddr)
	ON_COMMAND(ID_DRIVER_MENU_COPY_SIZE, &DlgDriverModule::OnDriverMenuCopySize)
	ON_COMMAND(ID_DRIVER_MENU_COPY_LOADORAD, &DlgDriverModule::OnDriverMenuCopyLoadorad)
	ON_COMMAND(ID_DRIVER_MENU_COPY_OBJECEADDR, &DlgDriverModule::OnDriverMenuCopyObjeceaddr)
	ON_COMMAND(ID_DRIVER_MENU_COPY_OBJECTNAME, &DlgDriverModule::OnDriverMenuCopyObjectname)
	ON_COMMAND(ID_DRIVER_MENU_COPY_SERVERNAME, &DlgDriverModule::OnDriverMenuCopyServername)
	ON_COMMAND(ID_DRIVER_MENU_COPY_PATH, &DlgDriverModule::OnDriverMenuCopyPath)
	ON_COMMAND(ID_DRIVER_MENU_COPY_COMPANY, &DlgDriverModule::OnDriverMenuCopyCompany)
	ON_COMMAND(ID_DRIVER_REFRESH, &DlgDriverModule::OnDriverRefresh)
	ON_NOTIFY(NM_RCLICK, ID_CONTROL_DRIVERMODULE_LIST, &DlgDriverModule::OnNMRClickControlDrivermoduleList)
	ON_COMMAND(ID_DRIVER_MENU_COPY_SING, &DlgDriverModule::OnDriverMenuCopySing)
	ON_COMMAND(ID_DRIVER_LOAD_START,        &DlgDriverModule::OnDriverLoadStart)
	ON_COMMAND(ID_DRIVER_MMAP_LOAD,         &DlgDriverModule::OnDriverMMapLoad)
	ON_COMMAND(ID_DRIVER_GRACEFUL_UNLOAD,   &DlgDriverModule::OnDriverGracefulUnload)
	ON_COMMAND(ID_DRIVER_FORCE_UNLOAD,      &DlgDriverModule::OnDriverForceUnload)
	ON_COMMAND(ID_DRIVER_START_BOOT,        &DlgDriverModule::OnDriverStartBoot)
	ON_COMMAND(ID_DRIVER_START_SYSTEM,      &DlgDriverModule::OnDriverStartSystem)
	ON_COMMAND(ID_DRIVER_START_AUTO,        &DlgDriverModule::OnDriverStartAuto)
	ON_COMMAND(ID_DRIVER_START_DEMAND,      &DlgDriverModule::OnDriverStartDemand)
	ON_COMMAND(ID_DRIVER_START_DISABLED,    &DlgDriverModule::OnDriverStartDisabled)
END_MESSAGE_MAP()

// DlgDriverModule 消息处理程序

BOOL DlgDriverModule::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_CListCtrl.InsertColumn(um_Driver_Name, _T("驱动名"), LVCFMT_LEFT, 130);
	m_CListCtrl.InsertColumn(um_Driver_BaseAddr, _T("基地址"), LVCFMT_LEFT, 120);
	m_CListCtrl.InsertColumn(um_Driver_Size, _T("大小"), LVCFMT_LEFT, 100);
	m_CListCtrl.InsertColumn(um_Driver_LoadOrder, _T("加载顺序"), LVCFMT_LEFT, 75);
	m_CListCtrl.InsertColumn(um_Driver_Object, _T("驱动对象"), LVCFMT_LEFT, 120);
	m_CListCtrl.InsertColumn(um_Driver_ObjectName, _T("对象名称"), LVCFMT_LEFT, 120);
	m_CListCtrl.InsertColumn(um_Driver_ServerName, _T("服务名称"), LVCFMT_LEFT, 100);
	m_CListCtrl.InsertColumn(um_Driver_DigitalSignature, _T("数字签名"), LVCFMT_LEFT, 100);
	m_CListCtrl.InsertColumn(um_Driver_FilePath, _T("路径"), LVCFMT_LEFT, 500);
	m_CListCtrl.InsertColumn(um_Driver_FileName, _T("公司名"), LVCFMT_LEFT, 250);
	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	return TRUE;
}

void DlgDriverModule::InsertCtrlListControl(PCDriverInfo pInfo)
{
	if (pInfo == NULL)
	{
		return;
	}

	PCLIST_ENTRY pList = &pInfo->List.List;
	do
	{
		int i = m_CListCtrl.GetItemCount();

		PCDriverInfo pDriverInfo = (PCDriverInfo)pList;

		m_CListCtrl.InsertItem(i, pDriverInfo->ImageBaseName);

		WCHAR str_DllBase[256] = { 0 };
		wsprintf(str_DllBase, L"%I64X", pDriverInfo->ImageBaseAddr);
		m_CListCtrl.SetItemText(i, um_Driver_BaseAddr, str_DllBase);

		WCHAR str_Size[256] = { 0 };
		wsprintf(str_Size, L"0x%08X", pDriverInfo->Size);
		m_CListCtrl.SetItemText(i, um_Driver_Size, str_Size);

		WCHAR str_LoadOrder[256] = { 0 };
		wsprintf(str_LoadOrder, L"%d", i);
		m_CListCtrl.SetItemText(i, um_Driver_LoadOrder, str_LoadOrder);

		WCHAR str_DriverObject[256] = { 0 };
		wsprintf(str_DriverObject, L"%I64X", pDriverInfo->DriverObject);
		m_CListCtrl.SetItemText(i, um_Driver_Object, pDriverInfo->DriverObject ? str_DriverObject : TEXT("--"));

		m_CListCtrl.SetItemText(i, um_Driver_ObjectName, pDriverInfo->DriverObject ? pDriverInfo->ServerName : TEXT("--"));

		m_CListCtrl.SetItemText(i, um_Driver_ServerName, pDriverInfo->DriverObject ? pDriverInfo->DriverName : TEXT("--"));

		CString FilePath = PathTransForm(pDriverInfo->ImageFullBaseName);

		m_CListCtrl.SetItemText(i, um_Driver_FilePath, FilePath.GetBuffer());

		TCHAR szSoftSignBuf[MAXBYTE] = { 0 };
		if (GetSoftSign(FilePath.GetBuffer(), szSoftSignBuf, MAXBYTE) == 0)
		{
			m_CListCtrl.SetItemText(i, um_Driver_DigitalSignature, szSoftSignBuf);
		}
		else
		{
			m_CListCtrl.SetItemText(i, um_Driver_DigitalSignature, TEXT("--"));
		}

		CString szDstFileName;
		m_CListCtrl.SetItemText(i, um_Driver_FileName, TEXT("--"));
		if (this->GetCompanyName(FilePath, szDstFileName))
		{
			m_CListCtrl.SetItemText(i, um_Driver_FileName, (LPWSTR)szDstFileName.GetString());
		}

		//获取下一个节点
		pList = pList->Blink;

		SIZE_T FreeSize = 0;
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pDriverInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}

	} while (pList != &pInfo->List.List);

}

void DlgDriverModule::OnNMRClickControlDrivermoduleList(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;

	CMenu menu;
	POINT point = { 0 };

	GetCursorPos(&point);//获取当前的游标
	menu.LoadMenuW(ID_MENU_DRIVER);//加载菜单资源
	CMenu* pPopup = menu.GetSubMenu(0);//

	POSITION FristIndex = m_CListCtrl.GetFirstSelectedItemPosition();//获取选中行的行数  pos = 行数 - 1
	int TempIndex = (int)FristIndex - 1;//存储第一次的索引位置

	if (this->m_ThreadFlags == 1)
	{
		menu.EnableMenuItem(ID_DRIVER_REFRESH, MF_GRAYED | MF_BYCOMMAND);
	}

	// 没有 DriverObject（ntoskrnl / win32k / hal 等系统镜像）禁用强卸 / 优雅卸 / 改启动类型
	CString driverObj = GetSelText(um_Driver_Object);
	CString svcName   = GetSelText(um_Driver_ServerName);
	BOOL hasDrvObj = (!driverObj.IsEmpty() && driverObj != L"--");
	BOOL hasSvc    = (!svcName.IsEmpty() && svcName != L"--");
	if (!hasDrvObj)
	{
		menu.EnableMenuItem(ID_DRIVER_FORCE_UNLOAD, MF_GRAYED | MF_BYCOMMAND);
	}
	if (!hasSvc)
	{
		menu.EnableMenuItem(ID_DRIVER_GRACEFUL_UNLOAD, MF_GRAYED | MF_BYCOMMAND);
		// 改启动类型这一整个 popup 灰掉（用 BYPOSITION 难定位，直接按 ID 单项灰）
		menu.EnableMenuItem(ID_DRIVER_START_BOOT,     MF_GRAYED | MF_BYCOMMAND);
		menu.EnableMenuItem(ID_DRIVER_START_SYSTEM,   MF_GRAYED | MF_BYCOMMAND);
		menu.EnableMenuItem(ID_DRIVER_START_AUTO,     MF_GRAYED | MF_BYCOMMAND);
		menu.EnableMenuItem(ID_DRIVER_START_DEMAND,   MF_GRAYED | MF_BYCOMMAND);
		menu.EnableMenuItem(ID_DRIVER_START_DISABLED, MF_GRAYED | MF_BYCOMMAND);
	}

	CString explorerPath;
	UINT explorerCmd = AppendOpenInExplorerItem(*pPopup, &m_CListCtrl, explorerPath);

	UINT cmd = pPopup->TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD, point.x, point.y, this);
	if (HandleOpenInExplorerCmd(cmd, explorerCmd, explorerPath))
	{
		return;
	}
	if (cmd != 0)
	{
		PostMessage(WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
	}
}

void DlgDriverModule::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	RECT rect = { 0 };
	rect.bottom = cy;
	rect.right = cx;
	m_CListCtrl.MoveWindow(&rect, TRUE);
}

void DlgDriverModule::OnDriverRefresh()
{
	// TODO: 在此添加命令处理程序代码
	m_CListCtrl.DeleteAllItems();

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumDriverInfo, this });

}

void DlgDriverModule::OnDriverMenuCopyName()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Driver_Name);
}

void DlgDriverModule::OnDriverMenuCopyBaseaddr()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Driver_BaseAddr);
}

void DlgDriverModule::OnDriverMenuCopySize()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Driver_Size);
}

void DlgDriverModule::OnDriverMenuCopyLoadorad()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Driver_LoadOrder);
}

void DlgDriverModule::OnDriverMenuCopyObjeceaddr()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Driver_Object);
}

void DlgDriverModule::OnDriverMenuCopyObjectname()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Driver_ObjectName);
}

void DlgDriverModule::OnDriverMenuCopyServername()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Driver_ServerName);
}

void DlgDriverModule::OnDriverMenuCopySing()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Driver_DigitalSignature);
}

void DlgDriverModule::OnDriverMenuCopyPath()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Driver_FilePath);
}

void DlgDriverModule::OnDriverMenuCopyCompany()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Driver_FileName);
}

// ============================================================
//   驱动操作：加载/启动、停止卸载、强制卸载、改启动类型
// ============================================================
CString DlgDriverModule::GetSelText(int col)
{
POSITION pos = m_CListCtrl.GetFirstSelectedItemPosition();
if (!pos) return CString();
int idx = m_CListCtrl.GetNextSelectedItem(pos);
return m_CListCtrl.GetItemText(idx, col);
}

void DlgDriverModule::OnDriverLoadStart()
{
CFileDialog dlg(TRUE, L"sys", NULL, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
L"驱动文件 (*.sys)|*.sys||", this);
if (dlg.DoModal() != IDOK) return;

CString path = dlg.GetPathName();
CString name = dlg.GetFileTitle();              // 去后缀的文件名做服务名
std::wstring err;
DWORD rc = SvcUtil::LoadAndStart(name.GetString(), path.GetString(), &err);
CString msg;
if (rc == 0) msg.Format(L"加载并启动成功：\n服务名：%s\n路径：%s", name.GetString(), path.GetString());
else         msg.Format(L"加载失败：%s", err.c_str());
AfxMessageBox(msg);
OnDriverRefresh();
}

void DlgDriverModule::OnDriverGracefulUnload()
{
CString name = GetSelText(um_Driver_ServerName);
if (name.IsEmpty() || name == L"--") { AfxMessageBox(L"请先选中一个有服务名的驱动行"); return; }
if (AfxMessageBox(L"确定停止并卸载服务：" + name + L" ?", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
std::wstring err;
DWORD rc = SvcUtil::StopAndDelete(name.GetString(), &err);
CString msg;
if (rc == 0) msg = L"已停止并删除服务：" + name;
else         msg.Format(L"停止/删除失败：%s", err.c_str());
AfxMessageBox(msg);
OnDriverRefresh();
}

void DlgDriverModule::OnDriverForceUnload()
{
CString driverObjHex = GetSelText(um_Driver_Object);
CString baseHex      = GetSelText(um_Driver_BaseAddr);
CString name         = GetSelText(um_Driver_Name);
if (driverObjHex.IsEmpty() || driverObjHex == L"--")
{
AfxMessageBox(L"该模块没有 DRIVER_OBJECT（可能不是 SCM 加载的驱动），无法走内核强卸路径");
return;
}
CString warn;
warn.Format(L"? 危险操作：将通过内核强制卸载\n  模块：%s\n  DRIVER_OBJECT：0x%s\n\n"
L"驱动里会做：IoDeleteDevice(所有设备) → 调 DriverUnload → ObDereference\n"
L"如果该驱动没有 DriverUnload 例程，会被拒绝（强制需进一步确认）。\n\n继续？", name.GetString(), driverObjHex.GetString());
if (AfxMessageBox(warn, MB_YESNO | MB_ICONWARNING) != IDYES) return;

ULONG64 drvObj = _wcstoui64(driverObjHex, nullptr, 16);
ULONG64 imgBase = _wcstoui64(baseHex, nullptr, 16);
CString svcName = GetSelText(um_Driver_ServerName);   // 服务名（用于 ZwUnloadDriver 干净卸）

CForceUnloadInfo req = { 0 };
req.DriverObject = drvObj;
req.ImageBase    = imgBase;
req.Flags        = 0;
if (!svcName.IsEmpty() && svcName != L"--")
{
    int n = (svcName.GetLength() < 63) ? svcName.GetLength() : 63;
    for (int i = 0; i < n; ++i) req.ServiceName[i] = svcName[i];
    req.ServiceName[n] = 0;
}

g_LoadDriver.SendMsg(um_Cmd_ForceUnload_Driver_info, &req, nullptr, nullptr, nullptr);

if (req.Status == FU_STATUS_NO_UNLOAD)
{
if (AfxMessageBox(L"该驱动没有 DriverUnload 例程。强卸极易蓝屏，是否仍要继续？",
MB_YESNO | MB_ICONSTOP) != IDYES) return;
req.Flags = FU_FLAG_FORCE_NO_UNLOAD;
g_LoadDriver.SendMsg(um_Cmd_ForceUnload_Driver_info, &req, nullptr, nullptr, nullptr);
}

CString msg;
switch (req.Status)
{
case FU_STATUS_OK:
msg.Format(L"强卸完成：删除设备 %lu 个，DriverUnload=0x%016llX，ZwUnloadDriver=0x%08X",
    req.DeviceCount, req.UnloadRoutine, req.ZwUnloadStatus);
break;
case FU_STATUS_BAD_PARAM:   msg = L"参数错误"; break;
case FU_STATUS_BAD_DRIVER:  msg = L"传入的不是合法 DRIVER_OBJECT（Type != 4）"; break;
case FU_STATUS_NO_UNLOAD:   msg = L"已取消（驱动无 DriverUnload）"; break;
case FU_STATUS_EXCEPTION:   msg.Format(L"执行过程中发生异常，已删除 %lu 个设备后中断", req.DeviceCount); break;
default:                    msg.Format(L"未知状态：%lu", req.Status); break;
}
AfxMessageBox(msg);
OnDriverRefresh();
}

void DlgDriverModule::DoChangeStart(unsigned long type, const wchar_t* typeName)
{
CString name = GetSelText(um_Driver_ServerName);
if (name.IsEmpty() || name == L"--") { AfxMessageBox(L"请先选中一个有服务名的驱动行"); return; }
std::wstring err;
DWORD rc = SvcUtil::ChangeStartType(name.GetString(), type, &err);
CString msg;
if (rc == 0) msg.Format(L"已将 %s 启动类型改为：%s", name.GetString(), typeName);
else         msg.Format(L"更改失败：%s", err.c_str());
AfxMessageBox(msg);
}

void DlgDriverModule::OnDriverStartBoot()     { DoChangeStart(SERVICE_BOOT_START,    L"Boot Start (0)"); }
void DlgDriverModule::OnDriverStartSystem()   { DoChangeStart(SERVICE_SYSTEM_START,  L"System Start (1)"); }
void DlgDriverModule::OnDriverStartAuto()     { DoChangeStart(SERVICE_AUTO_START,    L"Auto Start (2)"); }
void DlgDriverModule::OnDriverStartDemand()   { DoChangeStart(SERVICE_DEMAND_START,  L"Demand Start (3)"); }
void DlgDriverModule::OnDriverStartDisabled() { DoChangeStart(SERVICE_DISABLED,      L"Disabled (4)"); }
void DlgDriverModule::OnDriverMMapLoad()
{
CFileDialog dlg(TRUE, L"sys", NULL, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
L"驱动文件 (*.sys)|*.sys|所有文件 (*.*)|*.*||", this);
if (dlg.DoModal() != IDOK) return;

CString path = dlg.GetPathName();
CString warn = L"? 手动映射加载：\n  " + path + L"\n\n"
L"驱动会被读入 NonPagedPool 后直接 jmp DriverEntry：\n"
L"? 无服务名、无注册表、不入 PsLoadedModuleList，PCHunter 列表里看不到\n"
L"? DriverEntry 拿到的是 fake DRIVER_OBJECT；想'卸载'只能依赖驱动自己写的清理\n"
L"? 仅适合简单驱动（依赖 ntoskrnl / hal 之类内置模块）\n\n继续？";
if (AfxMessageBox(warn, MB_YESNO | MB_ICONWARNING) != IDYES) return;

CMMapDriverInfo req = { 0 };
int plen = path.GetLength();
if (plen >= 260) plen = 259;
for (int i = 0; i < plen; ++i) req.SysPath[i] = path[i];
req.PathLen = plen;

g_LoadDriver.SendMsg(um_Cmd_MMap_Driver_info, &req, nullptr, nullptr, nullptr);

CString msg;
switch (req.Status)
{
case MMD_STATUS_OK:
msg.Format(L"手动映射加载成功：\n  基址=0x%016llX\n  入口=0x%016llX\n  大小=0x%lX\n  DriverEntry 返回=0x%08X",
req.ImageBase, req.EntryPoint, req.SizeOfImage, req.EntryStatus);
break;
case MMD_STATUS_BAD_PARAM:    msg = L"参数错误"; break;
case MMD_STATUS_FILE_FAIL:    msg = L"读 .sys 文件失败"; break;
case MMD_STATUS_BAD_PE:       msg = L"PE 头不合法或不是 64 位驱动"; break;
case MMD_STATUS_ALLOC_FAIL:   msg = L"NonPagedPool 分配失败"; break;
case MMD_STATUS_IMPORT_FAIL:
msg.Format(L"导入解析失败：\n  dll=%S\n  func=%S",
req.FailedImportDll[0] ? req.FailedImportDll : "(?)",
req.FailedImportFunc[0] ? req.FailedImportFunc : "(?)");
break;
case MMD_STATUS_RELOC_FAIL:   msg = L"重定位失败"; break;
case MMD_STATUS_ENTRY_NTSTATUS:
msg.Format(L"DriverEntry 返回失败：0x%08X\n  镜像保留在内存中，未卸载", req.EntryStatus);
break;
case MMD_STATUS_EXCEPTION:    msg = L"执行过程中异常"; break;
case MMD_STATUS_UNSUPPORTED_IMAGE:
msg = L"不支持的映像：手动映射加载只接受 .sys 驱动文件，不能加载 ntoskrnl.exe / hal.dll 这类系统核心映像";
break;
default:                      msg.Format(L"未知状态：%lu", req.Status); break;
}
AfxMessageBox(msg);
}