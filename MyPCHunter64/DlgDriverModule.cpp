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
#include "include/capstone-5.0-Release/include/capstone/capstone.h"
#include <shlobj.h>
#include <commdlg.h>
#include <commctrl.h>
#include <vector>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "comctl32.lib")

extern void AppLog_Write(const char* level, const char* fmt, ...);
#define DRVMOD_LOG(...) AppLog_Write("INFO ", __VA_ARGS__)

extern _LoadDriver g_LoadDriver;

// ---- 本文件局部 PE 解析 / 反汇编 / 转储辅助 ----
namespace {

// 将一个文件整段读到内存里；返回 buffer + size。失败时返回 nullptr。
static BYTE* ReadAllFile(LPCWSTR path, DWORD* outSize)
{
	*outSize = 0;
	HANDLE hf = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hf == INVALID_HANDLE_VALUE) return nullptr;
	LARGE_INTEGER li = { 0 };
	if (!GetFileSizeEx(hf, &li) || li.QuadPart <= 0 || li.QuadPart > 64 * 1024 * 1024)
	{
		CloseHandle(hf); return nullptr;
	}
	DWORD size = (DWORD)li.QuadPart;
	BYTE* buf = (BYTE*)malloc(size);
	if (!buf) { CloseHandle(hf); return nullptr; }
	DWORD got = 0;
	if (!ReadFile(hf, buf, size, &got, NULL) || got != size)
	{
		free(buf); CloseHandle(hf); return nullptr;
	}
	CloseHandle(hf);
	*outSize = size;
	return buf;
}

// 把任意 RVA 翻译成文件偏移；找不到节区返回 0。
static DWORD RvaToFileOffset(BYTE* base, DWORD size, DWORD rva)
{
	if (size < sizeof(IMAGE_DOS_HEADER)) return 0;
	auto dos = (PIMAGE_DOS_HEADER)base;
	if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
	if ((DWORD)dos->e_lfanew + sizeof(IMAGE_NT_HEADERS64) > size) return 0;
	auto nt = (PIMAGE_NT_HEADERS64)(base + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
	auto sec = IMAGE_FIRST_SECTION(nt);
	for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i)
	{
		DWORD va = sec[i].VirtualAddress;
		DWORD vs = sec[i].Misc.VirtualSize ? sec[i].Misc.VirtualSize : sec[i].SizeOfRawData;
		if (rva >= va && rva < va + vs)
		{
			return sec[i].PointerToRawData + (rva - va);
		}
	}
	return 0;
}

// 拿到 NT 头指针；不是 PE64 返回 nullptr。
static PIMAGE_NT_HEADERS64 GetNt64(BYTE* base, DWORD size)
{
	if (size < sizeof(IMAGE_DOS_HEADER)) return nullptr;
	auto dos = (PIMAGE_DOS_HEADER)base;
	if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
	if ((DWORD)dos->e_lfanew + sizeof(IMAGE_NT_HEADERS64) > size) return nullptr;
	auto nt = (PIMAGE_NT_HEADERS64)(base + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;
	if (nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return nullptr;
	return nt;
}

// 安全把文件缓冲区里的 ANSI 字符串拷成宽字符串。
// 关键：MultiByteToWideChar 用 -1 会一路扫到 null。如果 buf 里的字符串
// 紧贴 buf+size 且没有 null 结尾，会越界读到未映射页 → AV。
// ntoskrnl.exe 导出表巨大、字符串密集，正是这个坑的高发场景。
// 这里先在 [offset, size) 区间用 strnlen 限定长度，再显式传给 MBToWC。
static CString SafeAnsiAtOffsetToCString(BYTE* buf, DWORD size, DWORD offset)
{
	CString out;
	if (!buf || offset >= size) return out;
	const char* p = (const char*)(buf + offset);
	size_t maxLen = (size_t)(size - offset);
	size_t slen = strnlen(p, maxLen);
	if (slen == 0) return out;
	int needed = MultiByteToWideChar(CP_ACP, 0, p, (int)slen, NULL, 0);
	if (needed <= 0) return out;
	if (needed > 4096) needed = 4096;  // 极端保护
	LPWSTR wbuf = out.GetBuffer(needed);
	int got = MultiByteToWideChar(CP_ACP, 0, p, (int)slen, wbuf, needed);
	out.ReleaseBuffer(got > 0 ? got : 0);
	return out;
}

// 写一个 UTF-16 LE BOM 文本文件到 %TEMP%，并用记事本打开。
// 已被 ShowTextViewer 取代，保留作旧调用兼容（暂未使用）。
static void WriteTempAndOpen(LPCWSTR baseName, const CString& content)
{
	WCHAR tmpDir[MAX_PATH] = { 0 };
	GetTempPathW(MAX_PATH, tmpDir);
	WCHAR tmpFile[MAX_PATH] = { 0 };
	swprintf_s(tmpFile, L"%s%s_%u.txt", tmpDir, baseName, GetCurrentProcessId());
	HANDLE hf = CreateFileW(tmpFile, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (hf == INVALID_HANDLE_VALUE) return;
	WORD bom = 0xFEFF;
	DWORD wr = 0;
	WriteFile(hf, &bom, sizeof(bom), &wr, NULL);
	WriteFile(hf, (LPCWSTR)content, content.GetLength() * sizeof(wchar_t), &wr, NULL);
	CloseHandle(hf);
	CString quoted; quoted.Format(L"\"%s\"", tmpFile);
	ShellExecuteW(NULL, L"open", L"notepad.exe", quoted, NULL, SW_SHOWNORMAL);
}

// ============================================================
// 通用 "PE 结果" 子窗口：客户区放一个 LVS_REPORT 列表 + 顶部状态栏。
// 列定义 + 行数据由调用方传入，三个用途（导出/导入/反汇编）共用同一份
// 弹窗代码。Modeless，方便叠多个对照。
struct ListViewerCol
{
	LPCWSTR name;
	int     width;
};
struct ListViewerCtx
{
	HWND  hList;
	HWND  hHeader;     // 顶部小标签（显示文件路径等摘要）
	HFONT hFont;
};

static LRESULT CALLBACK ListViewerWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	auto* ctx = (ListViewerCtx*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
	switch (msg)
	{
	case WM_SIZE:
		if (ctx && ctx->hList)
		{
			int w = LOWORD(lp), h = HIWORD(lp);
			int hdrH = 22;
			MoveWindow(ctx->hHeader, 0, 0, w, hdrH, TRUE);
			MoveWindow(ctx->hList, 0, hdrH, w, h - hdrH, TRUE);
		}
		return 0;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		return 0;
	case WM_DESTROY:
		if (ctx)
		{
			if (ctx->hFont) DeleteObject(ctx->hFont);
			delete ctx;
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
		}
		return 0;
	}
	return DefWindowProcW(hwnd, msg, wp, lp);
}

static void ShowListViewer(HWND parent, LPCWSTR title, LPCWSTR header,
	const std::vector<ListViewerCol>& cols,
	const std::vector<std::vector<CString>>& rows)
{
	DRVMOD_LOG("[ListViewer] BEGIN title='%ws' rows=%zu cols=%zu",
		title, rows.size(), cols.size());
	static bool s_registered = false;
	static const wchar_t* kClass = L"PCHunterPeListViewer";
	HINSTANCE hi = GetModuleHandleW(NULL);
	if (!s_registered)
	{
		WNDCLASSEXW wc = { sizeof(wc) };
		wc.style         = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc   = ListViewerWndProc;
		wc.hInstance     = hi;
		wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
		wc.lpszClassName = kClass;
		RegisterClassExW(&wc);
		s_registered = true;
	}

	HWND hwnd = CreateWindowExW(0, kClass, title,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 1000, 640,
		parent, NULL, hi, NULL);
	if (!hwnd) return;

	auto* ctx = new ListViewerCtx{ NULL, NULL, NULL };
	SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)ctx);

	ctx->hHeader = CreateWindowExW(0, L"STATIC", header,
		WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
		0, 0, 0, 22, hwnd, (HMENU)100, hi, NULL);
	ctx->hList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEW, L"",
		WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
		0, 22, 0, 0, hwnd, (HMENU)101, hi, NULL);
	ctx->hFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
	SendMessageW(ctx->hList, WM_SETFONT, (WPARAM)ctx->hFont, FALSE);
	SendMessageW(ctx->hHeader, WM_SETFONT,
		(WPARAM)GetStockObject(DEFAULT_GUI_FONT), FALSE);
	ListView_SetExtendedListViewStyle(ctx->hList,
		LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

	// 注入列
	for (size_t i = 0; i < cols.size(); ++i)
	{
		LVCOLUMNW c = { 0 };
		c.mask     = LVCF_TEXT | LVCF_WIDTH;
		c.cx       = cols[i].width;
		c.pszText  = (LPWSTR)cols[i].name;
		ListView_InsertColumn(ctx->hList, (int)i, &c);
	}
	// 批量插入：关闭重画，全部完了再打开
	SendMessageW(ctx->hList, WM_SETREDRAW, FALSE, 0);
	for (size_t r = 0; r < rows.size(); ++r)
	{
		LVITEMW it = { 0 };
		it.mask     = LVIF_TEXT;
		it.iItem    = (int)r;
		it.pszText  = rows[r].empty() ? (LPWSTR)L"" : (LPWSTR)(LPCWSTR)rows[r][0];
		int idx = ListView_InsertItem(ctx->hList, &it);
		for (size_t c = 1; c < rows[r].size() && c < cols.size(); ++c)
		{
			ListView_SetItemText(ctx->hList, idx, (int)c,
				(LPWSTR)(LPCWSTR)rows[r][c]);
		}
	}
	SendMessageW(ctx->hList, WM_SETREDRAW, TRUE, 0);
	DRVMOD_LOG("[ListViewer] inserted %zu rows", rows.size());

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);
	DRVMOD_LOG("[ListViewer] DONE");
}

static void ShowPeExports(HWND hwnd, const CString& path)
{
	DRVMOD_LOG("[Exports] BEGIN path='%ws'", (LPCWSTR)path);
	DWORD size = 0;
	BYTE* buf = ReadAllFile(path, &size);
	DRVMOD_LOG("[Exports] ReadAllFile -> buf=%p size=%u", buf, size);
	if (!buf) { ::MessageBoxW(hwnd, L"无法读取文件。", L"查看导出表", MB_OK | MB_ICONWARNING); return; }
	auto nt = GetNt64(buf, size);
	if (!nt)
	{
		free(buf);
		::MessageBoxW(hwnd, L"不是 64 位 PE 文件。", L"查看导出表", MB_OK | MB_ICONWARNING);
		return;
	}
	auto& dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
	if (dd.VirtualAddress == 0 || dd.Size == 0)
	{
		free(buf);
		::MessageBoxW(hwnd, L"该文件没有导出表。", L"查看导出表", MB_OK | MB_ICONINFORMATION);
		return;
	}
	DWORD off = RvaToFileOffset(buf, size, dd.VirtualAddress);
	if (!off || off + sizeof(IMAGE_EXPORT_DIRECTORY) > size)
	{
		free(buf); ::MessageBoxW(hwnd, L"导出表已损坏。", L"查看导出表", MB_OK | MB_ICONWARNING); return;
	}
	auto exp = (PIMAGE_EXPORT_DIRECTORY)(buf + off);
	DWORD funcsOff = RvaToFileOffset(buf, size, exp->AddressOfFunctions);
	DWORD namesOff = RvaToFileOffset(buf, size, exp->AddressOfNames);
	DWORD ordOff   = RvaToFileOffset(buf, size, exp->AddressOfNameOrdinals);
	DRVMOD_LOG("[Exports] NumberOfFunctions=%u NumberOfNames=%u Base=%u funcsOff=0x%X namesOff=0x%X ordOff=0x%X",
		exp->NumberOfFunctions, exp->NumberOfNames, exp->Base, funcsOff, namesOff, ordOff);

	// 上限保护：防止损坏 PE 里 NumberOfFunctions 被写成天文数字
	DWORD nFuncs = exp->NumberOfFunctions;
	DWORD nNames = exp->NumberOfNames;
	const DWORD kMaxFuncs = 200000;
	if (nFuncs > kMaxFuncs) { DRVMOD_LOG("[Exports] WARN cap NumberOfFunctions %u -> %u", nFuncs, kMaxFuncs); nFuncs = kMaxFuncs; }
	if (nNames > kMaxFuncs) { DRVMOD_LOG("[Exports] WARN cap NumberOfNames %u -> %u",     nNames, kMaxFuncs); nNames = kMaxFuncs; }

	std::vector<std::vector<CString>> rows;
	rows.reserve(nFuncs);
	for (DWORD i = 0; i < nFuncs; ++i)
	{
		DWORD rva = 0;
		if (funcsOff && funcsOff + (i + 1) * sizeof(DWORD) <= size)
			rva = ((DWORD*)(buf + funcsOff))[i];

		CString name = L"(未命名 / 序号导出)";
		if (namesOff && ordOff)
		{
			for (DWORD k = 0; k < nNames; ++k)
			{
				if (ordOff + (k + 1) * sizeof(WORD) > size) break;
				WORD ordIdx = ((WORD*)(buf + ordOff))[k];
				if (ordIdx == i)
				{
					if (namesOff + (k + 1) * sizeof(DWORD) > size) break;
					DWORD nameRva = ((DWORD*)(buf + namesOff))[k];
					DWORD nameO = RvaToFileOffset(buf, size, nameRva);
					if (nameO && nameO < size)
					{
						CString s = SafeAnsiAtOffsetToCString(buf, size, nameO);
						if (!s.IsEmpty()) name = s;
					}
					break;
				}
			}
		}
		CString sOrd, sRva;
		sOrd.Format(L"%u", exp->Base + i);
		sRva.Format(L"0x%08X", rva);
		rows.push_back({ sOrd, sRva, name });
	}
	DRVMOD_LOG("[Exports] rows built: %zu", rows.size());
	// 快照出 header 需要的标量再 free(buf)。dd / exp 是 buf 内部引用 /
	// 指针， free 之后访问它们是 use-after-free —— ntoskrnl 导出夺，循环
	// 正好让 LFH 重用原 buf 那块 → header.Format 一读就 AV （0xC0000005）。
	const DWORD ddVa     = dd.VirtualAddress;
	const DWORD ddSize   = dd.Size;
	const DWORD expBase  = exp->Base;
	const DWORD expNfunc = exp->NumberOfFunctions;
	const DWORD expNname = exp->NumberOfNames;
	free(buf);

	CString header;
	header.Format(L"文件: %s    导出表 RVA=0x%08X 大小=0x%X    函数=%u  名称=%u  Base=%u",
		(LPCWSTR)path, ddVa, ddSize, expNfunc, expNname, expBase);

	std::vector<ListViewerCol> cols = {
		{ L"序号", 60 },
		{ L"RVA",  100 },
		{ L"名称", 600 },
	};
	DRVMOD_LOG("[Exports] calling ShowListViewer rows=%zu", rows.size());
	ShowListViewer(hwnd, L"导出表", header, cols, rows);
	DRVMOD_LOG("[Exports] DONE");
}

static void ShowPeImports(HWND hwnd, const CString& path)
{
	DWORD size = 0;
	BYTE* buf = ReadAllFile(path, &size);
	if (!buf) { ::MessageBoxW(hwnd, L"无法读取文件。", L"查看导入表", MB_OK | MB_ICONWARNING); return; }
	auto nt = GetNt64(buf, size);
	if (!nt)
	{
		free(buf);
		::MessageBoxW(hwnd, L"不是 64 位 PE 文件。", L"查看导入表", MB_OK | MB_ICONWARNING);
		return;
	}
	auto& dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	if (dd.VirtualAddress == 0 || dd.Size == 0)
	{
		free(buf);
		::MessageBoxW(hwnd, L"该文件没有导入表。", L"查看导入表", MB_OK | MB_ICONINFORMATION);
		return;
	}
	DWORD descOff = RvaToFileOffset(buf, size, dd.VirtualAddress);
	if (!descOff)
	{
		free(buf); ::MessageBoxW(hwnd, L"导入表 RVA 无效。", L"查看导入表", MB_OK | MB_ICONWARNING); return;
	}

	std::vector<std::vector<CString>> rows;
	auto desc = (PIMAGE_IMPORT_DESCRIPTOR)(buf + descOff);
	while ((BYTE*)desc + sizeof(IMAGE_IMPORT_DESCRIPTOR) <= buf + size && desc->Name)
	{
		DWORD nameOff = RvaToFileOffset(buf, size, desc->Name);
		CString wmod = (nameOff && nameOff < size)
			? SafeAnsiAtOffsetToCString(buf, size, nameOff) : CString(L"(?)");

		DWORD oftRva = desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk;
		DWORD thunkOff = RvaToFileOffset(buf, size, oftRva);
		if (!thunkOff) { ++desc; continue; }

		auto thunk = (PIMAGE_THUNK_DATA64)(buf + thunkOff);
		while ((BYTE*)thunk + sizeof(IMAGE_THUNK_DATA64) <= buf + size && thunk->u1.AddressOfData)
		{
			CString sHint, sName;
			if (thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64)
			{
				sHint = L"-";
				sName.Format(L"#%llu (Ordinal)",
					(ULONGLONG)IMAGE_ORDINAL64(thunk->u1.Ordinal));
			}
			else
			{
				DWORD ibnOff = RvaToFileOffset(buf, size, (DWORD)thunk->u1.AddressOfData);
				if (ibnOff && ibnOff + sizeof(WORD) < size)
				{
					auto ibn = (PIMAGE_IMPORT_BY_NAME)(buf + ibnOff);
					sHint.Format(L"%u", ibn->Hint);
					DWORD nameByteOff = ibnOff + (DWORD)FIELD_OFFSET(IMAGE_IMPORT_BY_NAME, Name);
					sName = SafeAnsiAtOffsetToCString(buf, size, nameByteOff);
				}
				else { sHint = L"?"; sName = L"(? bad RVA)"; }
			}
			rows.push_back({ wmod, sHint, sName });
			++thunk;
		}
		++desc;
	}
	// 同样预先拍快照，避免 free(buf) 后 dd 被踩
	const DWORD ddVa   = dd.VirtualAddress;
	const DWORD ddSize = dd.Size;
	free(buf);

	CString header;
	header.Format(L"文件: %s    导入表 RVA=0x%08X 大小=0x%X    共 %u 条",
		(LPCWSTR)path, ddVa, ddSize, (unsigned)rows.size());

	std::vector<ListViewerCol> cols = {
		{ L"模块",   200 },
		{ L"Hint",    60 },
		{ L"函数名", 600 },
	};
	ShowListViewer(hwnd, L"导入表", header, cols, rows);
}

static void DisasmDriverEntry(HWND hwnd, const CString& path)
{
	DWORD size = 0;
	BYTE* buf = ReadAllFile(path, &size);
	if (!buf) { ::MessageBoxW(hwnd, L"无法读取文件。", L"反汇编入口点", MB_OK | MB_ICONWARNING); return; }
	auto nt = GetNt64(buf, size);
	if (!nt)
	{
		free(buf);
		::MessageBoxW(hwnd, L"不是 64 位 PE 文件。", L"反汇编入口点", MB_OK | MB_ICONWARNING);
		return;
	}
	DWORD entryRva = nt->OptionalHeader.AddressOfEntryPoint;
	ULONGLONG imageBase = nt->OptionalHeader.ImageBase;
	DWORD entryOff = RvaToFileOffset(buf, size, entryRva);
	if (!entryOff || entryOff >= size)
	{
		free(buf);
		::MessageBoxW(hwnd, L"入口点 RVA 无效。", L"反汇编入口点", MB_OK | MB_ICONWARNING);
		return;
	}
	const DWORD kMax = 256;
	DWORD avail = size - entryOff;
	DWORD codeLen = avail < kMax ? avail : kMax;

	csh handle = 0;
	if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK)
	{
		free(buf);
		::MessageBoxW(hwnd, L"capstone 初始化失败。", L"反汇编入口点", MB_OK | MB_ICONWARNING);
		return;
	}
	cs_option(handle, CS_OPT_SKIPDATA, CS_OPT_ON);
	cs_insn* insn = nullptr;
	size_t cnt = cs_disasm(handle, buf + entryOff, codeLen,
		imageBase + entryRva, 0, &insn);

	std::vector<std::vector<CString>> rows;
	rows.reserve(cnt);
	for (size_t i = 0; i < cnt; ++i)
	{
		CString sAddr, sBytes, sMnem, sOp;
		sAddr.Format(L"0x%016llX", (ULONGLONG)insn[i].address);
		for (UCHAR b = 0; b < insn[i].size && b < 16; ++b)
		{
			CString one; one.Format(L"%02X ", insn[i].bytes[b]);
			sBytes += one;
		}
		WCHAR mn[32] = { 0 }, op[160] = { 0 };
		MultiByteToWideChar(CP_ACP, 0, insn[i].mnemonic, -1, mn, _countof(mn) - 1);
		MultiByteToWideChar(CP_ACP, 0, insn[i].op_str,   -1, op, _countof(op) - 1);
		sMnem = mn;
		sOp   = op;
		rows.push_back({ sAddr, sBytes, sMnem, sOp });
	}
	if (insn) cs_free(insn, cnt);
	cs_close(&handle);
	free(buf);

	CString header;
	header.Format(L"文件: %s    ImageBase=0x%016llX  RVA=0x%08X  DriverEntry VA=0x%016llX  共 %u 条",
		(LPCWSTR)path, (ULONGLONG)imageBase, entryRva,
		(ULONGLONG)(imageBase + entryRva), (unsigned)rows.size());

	std::vector<ListViewerCol> cols = {
		{ L"地址",  170 },
		{ L"字节",  280 },
		{ L"指令",   80 },
		{ L"操作数", 360 },
	};
	ShowListViewer(hwnd, L"反汇编入口点 (DriverEntry)", header, cols, rows);
}

// ---- 内存转储驱动到 .sys ----
// 通过 um_Cmd_Read_KernelRange_info 把已加载驱动的 SizeOfImage 整段从内核
// 空间拷回用户缓冲，然后把每个 section 的 PointerToRawData/SizeOfRawData
// 改写成 VirtualAddress/VirtualSize（=== 文件 layout 等于内存 layout），
// 这样 dump 出来的 .sys 可以被各种 PE 工具打开。
//
// 已知局限：
//  - IAT 在加载后已经被填成绝对内核地址；本实现保留这一现状，没有把
//    每个槽位反查"模块+导出名"还原成 OriginalFirstThunk 形态。若需要
//    重建 IAT，需要进一步枚举所有内核模块的导出表来反向匹配。
//  - 重定位表本身保留；OptionalHeader.ImageBase 改写成实际加载基址。
static void MemoryDumpDriverToSys(HWND hwnd, ULONG64 imageBase, ULONG64 imageSize,
	const CString& origPath)
{
	if (imageBase == 0 || imageSize == 0)
	{
		::MessageBoxW(hwnd, L"基址/大小为空，无法内存转储。",
			L"内存转储驱动", MB_OK | MB_ICONWARNING);
		return;
	}
	if (imageSize > 256ULL * 1024 * 1024)
	{
		::MessageBoxW(hwnd, L"驱动镜像超过 256MB，已拒绝。",
			L"内存转储驱动", MB_OK | MB_ICONWARNING);
		return;
	}

	BYTE* buf = (BYTE*)calloc(1, (size_t)imageSize);
	if (!buf)
	{
		::MessageBoxW(hwnd, L"内存不足。", L"内存转储驱动", MB_OK | MB_ICONWARNING);
		return;
	}

	ULONG64 done = 0;
	ULONG totalRead = 0;
	while (done < imageSize)
	{
		ULONG64 left = imageSize - done;
		ULONG chunk = left > MAX_KRD_PER_CALL ? MAX_KRD_PER_CALL : (ULONG)left;
		CKernelRangeReadInfo req = { 0 };
		req.KernelAddr = imageBase + done;
		req.Length     = chunk;
		req.UserBuf    = buf + done;
		// 用"强制读"路径：内核侧按页 MmGetPhysicalAddress + MmMapIoSpaceEx
		// 重新映射物理页到临时 PAGE_READONLY VA，绕过 PAGE_NX / INIT 段回收
		// 等导致原 VA 不可直接 RtlCopyMemory 的情形；拷完立即 MmUnmapIoSpace。
		g_LoadDriver.SendMsg(um_Cmd_Force_Read_KernelRange_info, &req, nullptr, nullptr, nullptr);
		// 读不到的块（被分页 / 物理失效）就保留全 0，继续往下推进
		totalRead += req.BytesRead;
		done += chunk;
	}

	if (totalRead == 0)
	{
		free(buf);
		::MessageBoxW(hwnd, L"驱动镜像不可读（驱动可能未加载/已卸载，或区域被特殊保护）。",
			L"内存转储驱动", MB_OK | MB_ICONWARNING);
		return;
	}

	// PE 头校验 + section 表回填
	bool peOk = false;
	if (imageSize >= sizeof(IMAGE_DOS_HEADER))
	{
		auto dos = (PIMAGE_DOS_HEADER)buf;
		if (dos->e_magic == IMAGE_DOS_SIGNATURE
			&& (DWORD)dos->e_lfanew + sizeof(IMAGE_NT_HEADERS64) < imageSize)
		{
			auto nt = (PIMAGE_NT_HEADERS64)(buf + dos->e_lfanew);
			if (nt->Signature == IMAGE_NT_SIGNATURE
				&& nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
			{
				// 内存 layout 落盘：raw == virtual
				auto sec = IMAGE_FIRST_SECTION(nt);
				for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i)
				{
					DWORD vs = sec[i].Misc.VirtualSize;
					if (vs == 0) vs = sec[i].SizeOfRawData;
					sec[i].PointerToRawData = sec[i].VirtualAddress;
					sec[i].SizeOfRawData    = vs;
				}
				// 把 OptionalHeader.ImageBase 改写成实际加载基址，方便 IDA/PE-bear 之类工具自然识别
				nt->OptionalHeader.ImageBase = imageBase;
				peOk = true;
			}
		}
	}

	WCHAR initName[MAX_PATH] = { 0 };
	// 按原驱动名的后缀决定 dump 文件后缀（.sys/.exe/.dll/.dll/...）
	CString ext = L"sys";   // 兜底
	LPCWSTR slash = origPath.IsEmpty() ? NULL : wcsrchr(origPath, L'\\');
	if (slash)
	{
		CString base = slash + 1;
		int dot = base.ReverseFind(L'.');
		CString stem = (dot > 0) ? base.Left(dot) : base;
		if (dot > 0 && dot + 1 < base.GetLength())
			ext = base.Mid(dot + 1);
		swprintf_s(initName, L"%s_mem.%s", (LPCWSTR)stem, (LPCWSTR)ext);
	}
	else
	{
		swprintf_s(initName, L"driver_mem.%s", (LPCWSTR)ext);
	}

	WCHAR file[MAX_PATH] = { 0 };
	wcscpy_s(file, initName);

	// 过滤器也跟着扩展名走，方便另存为同类型
	CString filterSpec, filterAll = L"所有文件 (*.*)";
	filterSpec.Format(L"原类型 (*.%s)", (LPCWSTR)ext);
	WCHAR filterBuf[256] = { 0 };
	int off = 0;
	off += swprintf_s(filterBuf + off, _countof(filterBuf) - off, L"%s", (LPCWSTR)filterSpec) + 1;
	off += swprintf_s(filterBuf + off, _countof(filterBuf) - off, L"*.%s", (LPCWSTR)ext) + 1;
	off += swprintf_s(filterBuf + off, _countof(filterBuf) - off, L"%s", (LPCWSTR)filterAll) + 1;
	off += swprintf_s(filterBuf + off, _countof(filterBuf) - off, L"*.*") + 1;
	filterBuf[off] = 0;  // 双 null 结尾

	OPENFILENAMEW ofn = { 0 };
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner   = hwnd;
	ofn.lpstrFilter = filterBuf;
	ofn.lpstrFile   = file;
	ofn.nMaxFile    = _countof(file);
	ofn.lpstrTitle  = L"内存转储驱动到 .sys/.exe/.dll";
	ofn.lpstrDefExt = (LPCWSTR)ext;
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	if (!GetSaveFileNameW(&ofn))
	{
		free(buf);
		return;
	}

	HANDLE hf = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (hf == INVALID_HANDLE_VALUE)
	{
		free(buf);
		CString m; m.Format(L"无法创建文件，GetLastError=%u", GetLastError());
		::MessageBoxW(hwnd, m, L"内存转储驱动", MB_OK | MB_ICONWARNING);
		return;
	}
	DWORD wr = 0;
	WriteFile(hf, buf, (DWORD)imageSize, &wr, NULL);
	CloseHandle(hf);
	free(buf);

	CString msg;
	msg.Format(L"已内存转储到:\n%s\n\n大小: 0x%llX (%llu)\n"
		L"PE 修复: %s\n\n注意：IAT 已是绝对内核地址；重定位表保留。",
		file, (ULONGLONG)imageSize, (ULONGLONG)imageSize,
		peOk ? L"section raw=virtual / ImageBase 已改写" : L"非 PE64 头，原样写出");
	::MessageBoxW(hwnd, msg, L"内存转储驱动", MB_OK | MB_ICONINFORMATION);
}

static void DumpDriverToSys(HWND hwnd, const CString& path)
{
	if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES)
	{
		::MessageBoxW(hwnd, L"驱动文件不存在或无权访问。", L"转储驱动", MB_OK | MB_ICONWARNING);
		return;
	}
	WCHAR initName[MAX_PATH] = { 0 };
	LPCWSTR slash = wcsrchr(path, L'\\');
	wcsncpy_s(initName, slash ? slash + 1 : (LPCWSTR)path, _TRUNCATE);

	WCHAR file[MAX_PATH] = { 0 };
	wcsncpy_s(file, initName, _TRUNCATE);

	OPENFILENAMEW ofn = { 0 };
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFilter = L"驱动文件 (*.sys)\0*.sys\0所有文件 (*.*)\0*.*\0";
	ofn.lpstrFile = file;
	ofn.nMaxFile = _countof(file);
	ofn.lpstrTitle = L"转储驱动到 .sys";
	ofn.lpstrDefExt = L"sys";
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	if (!GetSaveFileNameW(&ofn)) return;

	if (CopyFileW(path, file, FALSE))
	{
		CString msg;
		msg.Format(L"已转储到:\n%s", file);
		::MessageBoxW(hwnd, msg, L"转储驱动", MB_OK | MB_ICONINFORMATION);
	}
	else
	{
		CString msg;
		msg.Format(L"复制失败，GetLastError=%u", GetLastError());
		::MessageBoxW(hwnd, msg, L"转储驱动", MB_OK | MB_ICONWARNING);
	}
}

} // anonymous namespace

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

	// 动态追加：哈希 / 签名 / 服务注册表 / (待实现的几项)
	const UINT kDrvHash     = 9100;
	const UINT kDrvVerify   = 9101;
	const UINT kDrvSvcReg   = 9102;
	const UINT kDrvExports  = 9103;
	const UINT kDrvImports  = 9104;
	const UINT kDrvDisasm   = 9105;
	const UINT kDrvDump     = 9106;
	const UINT kDrvMemDump  = 9109;
	const UINT kDrvCopyPath = 9107;
	const UINT kDrvProps    = 9108;
	CString drvPath = GetSelText(um_Driver_FilePath);
	BOOL hasPath = !drvPath.IsEmpty() && drvPath != L"--";
	pPopup->AppendMenuW(MF_SEPARATOR, 0, (LPCTSTR)NULL);
	pPopup->AppendMenuW(MF_STRING | (hasPath ? 0 : MF_GRAYED), kDrvCopyPath, L"复制为路径");
	pPopup->AppendMenuW(MF_STRING | (hasPath ? 0 : MF_GRAYED), kDrvProps,    L"属性");
	pPopup->AppendMenuW(MF_STRING | (hasPath ? 0 : MF_GRAYED), kDrvHash,     L"计算 MD5 / SHA1 / SHA256");
	pPopup->AppendMenuW(MF_STRING | (hasPath ? 0 : MF_GRAYED), kDrvVerify,   L"检查数字签名");
	pPopup->AppendMenuW(MF_STRING | (hasSvc ? 0 : MF_GRAYED),  kDrvSvcReg,   L"打开服务注册表项");
	pPopup->AppendMenuW(MF_SEPARATOR, 0, (LPCTSTR)NULL);
	pPopup->AppendMenuW(MF_STRING | (hasPath ? 0 : MF_GRAYED), kDrvExports, L"查看导出表");
	pPopup->AppendMenuW(MF_STRING | (hasPath ? 0 : MF_GRAYED), kDrvImports, L"查看导入表");
	pPopup->AppendMenuW(MF_STRING | (hasPath ? 0 : MF_GRAYED), kDrvDisasm, L"反汇编入口点 (DriverEntry)");
	pPopup->AppendMenuW(MF_STRING | (hasPath ? 0 : MF_GRAYED), kDrvDump,    L"转储驱动到 .sys (磁盘拷贝)");
	CString baseHex = GetSelText(um_Driver_BaseAddr);
	CString sizeHex = GetSelText(um_Driver_Size);
	BOOL hasMemImg = !baseHex.IsEmpty() && baseHex != L"--" && !sizeHex.IsEmpty() && sizeHex != L"--";
	pPopup->AppendMenuW(MF_STRING | (hasMemImg ? 0 : MF_GRAYED), kDrvMemDump, L"内存转储驱动到 .sys");

	UINT cmd = pPopup->TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD, point.x, point.y, this);
	if (HandleOpenInExplorerCmd(cmd, explorerCmd, explorerPath))
	{
		return;
	}
	// 处理动态追加项
	if (cmd == kDrvCopyPath && hasPath)
	{
		CString quoted = L"\"" + drvPath + L"\"";
		if (OpenClipboard())
		{
			EmptyClipboard();
			size_t bytes = (quoted.GetLength() + 1) * sizeof(wchar_t);
			HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
			if (hMem)
			{
				void* p = GlobalLock(hMem);
				if (p) { memcpy(p, (LPCWSTR)quoted, bytes); GlobalUnlock(hMem); }
				SetClipboardData(CF_UNICODETEXT, hMem);
			}
			CloseClipboard();
		}
		return;
	}
	if (cmd == kDrvProps && hasPath)
	{
		SHObjectProperties(GetSafeHwnd(), SHOP_FILEPATH, drvPath, NULL);
		return;
	}
	if (cmd == kDrvHash && hasPath)   { ShowFileHashesDialog(GetSafeHwnd(), drvPath); return; }
	if (cmd == kDrvVerify && hasPath) { VerifyFileSignatureDialog(GetSafeHwnd(), drvPath); return; }
	if (cmd == kDrvSvcReg && hasSvc)
	{
		// 通过 regedit 的 LastKey 跳转到指定服务键
		HKEY hk = NULL;
		if (RegCreateKeyExW(HKEY_CURRENT_USER,
			L"Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit",
			0, NULL, 0, KEY_SET_VALUE, NULL, &hk, NULL) == ERROR_SUCCESS)
		{
			CString target;
			target.Format(L"Computer\\HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\%s",
				(LPCWSTR)svcName);
			RegSetValueExW(hk, L"LastKey", 0, REG_SZ,
				(const BYTE*)(LPCWSTR)target,
				(DWORD)((target.GetLength() + 1) * sizeof(wchar_t)));
			RegCloseKey(hk);
		}
		ShellExecuteW(NULL, L"open", L"regedit.exe", NULL, NULL, SW_SHOWNORMAL);
		return;
	}
	if (cmd == kDrvExports && hasPath) { ShowPeExports(GetSafeHwnd(), drvPath);     return; }
	if (cmd == kDrvImports && hasPath) { ShowPeImports(GetSafeHwnd(), drvPath);     return; }
	if (cmd == kDrvDisasm  && hasPath) { DisasmDriverEntry(GetSafeHwnd(), drvPath); return; }
	if (cmd == kDrvDump    && hasPath) { DumpDriverToSys(GetSafeHwnd(), drvPath);   return; }
	if (cmd == kDrvMemDump && hasMemImg)
	{
		ULONG64 ib = _wcstoui64(baseHex, nullptr, 16);
		// sizeHex 是 "0x%08X" 形式，_wcstoui64 带前缀 0x 能识别
		ULONG64 iz = _wcstoui64(sizeHex, nullptr, 16);
		MemoryDumpDriverToSys(GetSafeHwnd(), ib, iz, drvPath);
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