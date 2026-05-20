// DlgEnumFile.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgEnumFile.h"
#include "Thread.h"
#include <shlobj.h>
#include <vector>
#pragma comment(lib, "shell32.lib")

// 地址栏控件 ID（自定义范围，避开 MFC 资源 ID）
#define ID_NAVBAR_BACK    30001
#define ID_NAVBAR_FORWARD 30002
#define ID_NAVBAR_PATH    30003
#define ID_NAVBAR_GO      30004
#define ID_NAVBAR_SEARCH  30005
static const int kNavBarHeight = 28;

// DlgEnumFile 对话框

IMPLEMENT_DYNAMIC(DlgEnumFile, CDialogEx)

DlgEnumFile::DlgEnumFile(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_ENUMFILE, pParent)
{
	m_ThreadFlags = FALSE;
}

DlgEnumFile::~DlgEnumFile()
{
}

void DlgEnumFile::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_ENUMFILE_LIST, m_CListCtrl);
	DDX_Control(pDX, ID_ENUMFILE_TREE, m_CTreeCtrl);
}


BEGIN_MESSAGE_MAP(DlgEnumFile, CDialogEx)
	ON_WM_SIZE()
	ON_NOTIFY(NM_DBLCLK, ID_ENUMFILE_TREE, &DlgEnumFile::OnDblclkEnumfileTree)
	ON_COMMAND(ID_FILE_DELETE, &DlgEnumFile::OnFileDelete)
	ON_COMMAND(ID_FILE_REFRESH, &DlgEnumFile::OnFileRefresh)
	ON_NOTIFY(NM_RCLICK, ID_ENUMFILE_LIST, &DlgEnumFile::OnNMRClickEnumfileList)
	ON_COMMAND(ID_FILE_FILEDEOCCUPY, &DlgEnumFile::OnFileFiledeoccupy)
	ON_NOTIFY(LVN_ENDLABELEDIT, ID_ENUMFILE_LIST, &DlgEnumFile::OnEndLabelEditEnumfileList)
	ON_NOTIFY(NM_RCLICK, ID_ENUMFILE_TREE, &DlgEnumFile::OnNMRClickEnumfileTree)
	ON_NOTIFY(TVN_ENDLABELEDIT, ID_ENUMFILE_TREE, &DlgEnumFile::OnEndLabelEditEnumfileTree)
	ON_BN_CLICKED(ID_NAVBAR_BACK,    &DlgEnumFile::OnBtnNavBack)
	ON_BN_CLICKED(ID_NAVBAR_FORWARD, &DlgEnumFile::OnBtnNavForward)
	ON_BN_CLICKED(ID_NAVBAR_GO,      &DlgEnumFile::OnBtnNavGo)
END_MESSAGE_MAP()


// DlgEnumFile 消息处理程序


BOOL DlgEnumFile::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	//获取盘符信息
	TCHAR szBuf[MAXBYTE] = { 0 };
	auto nRet = GetLogicalDriveStrings(MAXBYTE, szBuf);
	if (!nRet)
	{
		ErrorMessage(GetLastError(), TEXT("获取盘符失败,错误信息:>"));
		return FALSE;
	}

	m_CTreeCtrl.SetExtendedStyle(m_CTreeCtrl.GetExtendedStyle() | TVS_FULLROWSELECT | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS, m_CTreeCtrl.GetExtendedStyle() | TVS_FULLROWSELECT | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS);
	// 允许树节点标签内编辑（右键"重命名"会触发 EditLabel）
	m_CTreeCtrl.ModifyStyle(0, TVS_EDITLABELS);

	//初始化树控件
	//初始化Tree控件
	PTCHAR tmp = szBuf;
	do
	{
		int nLen = 0;
		CString* pStr = new CString(tmp);
		HTREEITEM  hTreeItrm = m_CTreeCtrl.InsertItem(((tmp[wcslen(tmp) - 1] = L'\0'), tmp));
		m_CTreeCtrl.SetItemData(hTreeItrm, (DWORD_PTR)pStr);

		tmp = tmp + wcslen((WCHAR*)tmp) + 2;
	} while (wcscmp(tmp, L""));


	//初始化List控件

	WCHAR* szBuf1[] = { L"文件名",L"创建时间" ,L"上次访问时间", L"文件大小" };

	/*设置风格*/
	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	// 允许列表项标签内编辑：右键"重命名"会触发 EditLabel
	m_CListCtrl.ModifyStyle(0, LVS_EDITLABELS);

	for (int i = 0; i < sizeof(szBuf1) / sizeof(TCHAR*); i++)
	{
		m_CListCtrl.InsertColumn(i, szBuf1[i]);
		m_CListCtrl.SetColumnWidth(i, 200);
	}

	// ===== 顶部地址栏：[<] [>]  [Path .....] [Go]  [Search .....] =====
	// 位置由 OnSize 统一调整；这里只负责创建。
	CRect rcPlace(0, 0, 30, kNavBarHeight - 4);
	m_BtnBack.Create(L"\u25C0", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		rcPlace, this, ID_NAVBAR_BACK);     // ◀
	m_BtnForward.Create(L"\u25B6", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		rcPlace, this, ID_NAVBAR_FORWARD);  // ▶
	m_PathEdit.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
		rcPlace, this, ID_NAVBAR_PATH);
	m_BtnGo.Create(L"\u21B2", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		rcPlace, this, ID_NAVBAR_GO);       // ↲
	m_SearchEdit.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
		rcPlace, this, ID_NAVBAR_SEARCH);

	// 用对话框默认字体让控件外观一致
	CFont* pFont = GetFont();
	if (pFont)
	{
		m_BtnBack.SetFont(pFont);
		m_BtnForward.SetFont(pFont);
		m_PathEdit.SetFont(pFont);
		m_BtnGo.SetFont(pFont);
		m_SearchEdit.SetFont(pFont);
	}
	// Search 框显示占位提示 ("搜索 当前目录")
	m_SearchEdit.SetCueBanner(L"搜索当前目录", TRUE);
	m_PathEdit.SetCueBanner(L"输入目录路径，回车跳转", TRUE);
	UpdateNavButtons();

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void DlgEnumFile::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	CRect rect;
	GetClientRect(&rect);

	// 顶部地址栏：[<] [>] [path .....] [Go] [search ...]
	const int btnW   = 30;
	const int gap    = 4;
	const int margin = 2;
	const int searchW = (rect.Width() / 4 > 240) ? 240 : (rect.Width() / 4);
	if (m_BtnBack.GetSafeHwnd())
	{
		int x = margin;
		int y = margin;
		int h = kNavBarHeight - 2 * margin;
		m_BtnBack.SetWindowPos(NULL, x, y, btnW, h, SWP_NOZORDER);
		x += btnW + gap;
		m_BtnForward.SetWindowPos(NULL, x, y, btnW, h, SWP_NOZORDER);
		x += btnW + gap;

		int pathRight = rect.Width() - margin - searchW - gap - btnW - gap;
		if (pathRight < x + 60) pathRight = x + 60;
		m_PathEdit.SetWindowPos(NULL, x, y + 2, pathRight - x, h - 4, SWP_NOZORDER);

		x = pathRight + gap;
		m_BtnGo.SetWindowPos(NULL, x, y, btnW, h, SWP_NOZORDER);
		x += btnW + gap;

		m_SearchEdit.SetWindowPos(NULL, x, y + 2, rect.Width() - margin - x, h - 4, SWP_NOZORDER);
	}

	// 树/列表：留出顶部地址栏高度
	int top = kNavBarHeight;
	int bodyH = rect.Height() - top;
	if (bodyH < 0) bodyH = 0;
	float fwidth = rect.Width() / 4;

	m_CTreeCtrl.SetWindowPos(NULL, 0, top, (int)fwidth, bodyH, SWP_NOZORDER);
	m_CListCtrl.SetWindowPos(NULL, (int)fwidth, top, rect.Width() - (int)fwidth, bodyH, SWP_NOZORDER);
}

void DlgEnumFile::DelTreeChild(HTREEITEM pNode)
{
	auto CurrentChild = m_CTreeCtrl.GetChildItem(pNode); /*获取子集*/

	while (CurrentChild != NULL)
	{
		/*获取绑定的数据,并释放空间*/
		CString* pStr = (CString*)m_CTreeCtrl.GetItemData(CurrentChild);

		if (pStr != NULL)
		{
			delete pStr; //释放开辟的空间
		}

		/*备份要删除的位置*/
		auto Del = CurrentChild;

		/*获取下一个同级的数据*/
		CurrentChild = m_CTreeCtrl.GetNextSiblingItem(CurrentChild);

		/*删除*/
		m_CTreeCtrl.DeleteItem(Del);
	}
}

void DlgEnumFile::DelListItemData()
{
	//获取数量
	int Count = m_CListCtrl.GetItemCount();
	if (Count <= 0)
	{
		//没有数据返回
		return;
	}


	for (int i = 0; i < Count; i++)
	{
		CString* p = (CString*)m_CListCtrl.GetItemData(i);

		if (p != NULL)
		{
			delete p;
		}

	}

	m_CListCtrl.DeleteAllItems();//删除全部数据
}

void DlgEnumFile::OnDblclkEnumfileTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;

	//获取选择的项
	auto hSelectItem = m_CTreeCtrl.GetSelectedItem();
	if (hSelectItem == NULL) //当没有绑定的数据时返回 
	{
		return;
	}

	//获取绑定的数据
	CString* pStr = (CString*)m_CTreeCtrl.GetItemData(hSelectItem);
	if (pStr == NULL)
	{
		return;
	}

	CString strtmp = pStr->GetString();
	//判断是否是目录不是返回
	if (strtmp[strtmp.GetLength() - 1] != TEXT('\\'))
	{
		return;
	}

	//将点击的路径存储起来
	m_CurPath = strtmp;

	// 写入浏览历史并同步地址栏
	if (!m_NavSuppressHistory)
	{
		if (m_NavIndex >= 0 && m_NavIndex + 1 < (int)m_NavHistory.size())
			m_NavHistory.erase(m_NavHistory.begin() + m_NavIndex + 1, m_NavHistory.end());
		if (m_NavIndex < 0 || m_NavHistory[m_NavIndex].CompareNoCase(m_CurPath) != 0)
		{
			m_NavHistory.push_back(m_CurPath);
			m_NavIndex = (int)m_NavHistory.size() - 1;
		}
	}
	UpdateNavButtons();

	OnFileRefresh();
}

void DlgEnumFile::DelteFile()
{
	if (m_CListCtrl.GetItemCount() < 0)
	{
		return;
	}

	//获取当前选择的控件项
	POSITION FristIndex = m_CListCtrl.GetFirstSelectedItemPosition();//获取选中行的行数  pos = 行数 - 1
	int TempIndex = (int)FristIndex - 1;//存储第一次的索引位置

	//获取绑定的数据
	CString* pStrData = (CString*)m_CListCtrl.GetItemData(TempIndex);
	if (pStrData == NULL)
	{
		return;
	}

	//想驱动发送删除文件的消息
	ULONG64 dqRet = g_LoadDriver.SendMsg(um_Cmd_DeleteFile_info, pStrData->GetBuffer());
	if (dqRet == 0)
	{
		::MessageBoxW(NULL, L"文件删除成功!", L"提示", MB_OK);

		//删除子项
		delete pStrData;
		m_CListCtrl.DeleteItem(TempIndex);

	}
	else
	{
		::MessageBoxW(NULL, L"文件删除失败!", L"提示", MB_OK);
	}
}

void DlgEnumFile::EnunFile()
{
	//获取选择的项
	auto hSelectItem = m_CTreeCtrl.GetSelectedItem();


	//获取绑定的数据
	//CString* pStr = (CString*)m_CTreeCtrl.GetItemData(hSelectItem);

	//CString strtmp = pStr->GetString();
	//判断是否是目录不是返回


	DelTreeChild(hSelectItem);	/*删除当前选择节点的所有孩子*/
	DelListItemData();			/*删除List控件所有绑定的数据*/


	//发送消息
	PCFileInfo pFileInfo = NULL;
	g_LoadDriver.SendMsg(um_Cmd_Enum_File_info, m_CurPath.GetBuffer(), (PVOID64*)&pFileInfo);

	if (pFileInfo == NULL)
	{
		return;
	}

	SIZE_T FreeSize = 0;
	PCLIST_ENTRY pCurList = &pFileInfo->List;
	do
	{
		if (pCurList == NULL)
		{
			return;
		}

		PCFileInfo pCurFileInfo = (PCFileInfo)pCurList;

		//判断是目录还是文件
		if (pCurFileInfo->FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			//目录插入到Tree控件中
			CString* pNewFilePath = new CString();
			//拼接字符串  
			pNewFilePath->Format(L"%ws%ws\\", m_CurPath.GetBuffer(), pCurFileInfo->FileFullName);

			TVINSERTSTRUCTW tvInsertTree = { 0 };

			tvInsertTree.hParent = hSelectItem;
			tvInsertTree.hInsertAfter = TVI_LAST;
			tvInsertTree.itemex.mask = TVIF_TEXT | TVIF_CHILDREN;
			tvInsertTree.itemex.pszText = pCurFileInfo->FileFullName;
			tvInsertTree.itemex.cchTextMax = wcslen(pCurFileInfo->FileFullName) + 1;
			tvInsertTree.itemex.cChildren = 1;

			auto hCurrentItem = m_CTreeCtrl.InsertItem(&tvInsertTree);
			/*设置与当前行绑定的数据*/
			m_CTreeCtrl.SetItemData(hCurrentItem, (DWORD_PTR)pNewFilePath);
		}
		else
		{
			ULONG64 i = m_CListCtrl.GetItemCount();

			m_CListCtrl.InsertItem(i, pCurFileInfo->FileFullName);

			// 内核读到的时间字段为 UTC，转换到本地时区再显示
			auto FormatTimeLocal = [](const auto& tf, CString& out) {
				SYSTEMTIME stUtc = { 0 };
				stUtc.wYear         = (WORD)tf.Year;
				stUtc.wMonth        = (WORD)tf.Month;
				stUtc.wDay          = (WORD)tf.Day;
				stUtc.wHour         = (WORD)tf.Hour;
				stUtc.wMinute       = (WORD)tf.Minute;
				stUtc.wSecond       = (WORD)tf.Second;
				stUtc.wMilliseconds = (WORD)tf.Milliseconds;
				SYSTEMTIME stLocal = stUtc;
				SystemTimeToTzSpecificLocalTime(NULL, &stUtc, &stLocal);
				out.Format(L"%d/%d/%d--%d:%d:%d:%d",
					stLocal.wYear, stLocal.wMonth, stLocal.wDay,
					stLocal.wHour, stLocal.wMinute, stLocal.wSecond,
					stLocal.wMilliseconds);
			};

			CString strBuf;
			FormatTimeLocal(pCurFileInfo->CreationTime, strBuf);
			m_CListCtrl.SetItemText(i, 1, strBuf);

			FormatTimeLocal(pCurFileInfo->ChangeTime, strBuf);
			m_CListCtrl.SetItemText(i, 2, strBuf);

			strBuf.Format(L"%I64X", pCurFileInfo->AllocationSize);
			m_CListCtrl.SetItemText(i, 3, strBuf);

			//目录插入到List控件中
			CString* pNewFilePath = new CString();
			//拼接字符串  
			pNewFilePath->Format(L"%ws%ws", m_CurPath.GetBuffer(), pCurFileInfo->FileFullName);
			//设置控件路径
			m_CListCtrl.SetItemData(i, (DWORD_PTR)pNewFilePath);

		}
		pCurList = pCurList->Flink;


		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pCurFileInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}
	} while (pCurList != &pFileInfo->List);
}

void DlgEnumFile::OnFileDelete()
{
	//DWORD lpThreadId = 0;
	//
	//CThreadInfo* pThread = new CThreadInfo{ _LoadDriver::Um_UserCallBackType_UserDelteFileInfo, this };
	//HANDLE hThread = CreateThread(NULL, NULL, UniversalThreadFunction,/*变量参数地址*/(LPVOID)pThread, 0, &lpThreadId);
	//
	//
	//CloseHandle(hThread);

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserDelteFileInfo, this });

}

void DlgEnumFile::OnFileRefresh()
{
	//DWORD lpThreadId = 0;
	//CThreadInfo* pThread = new CThreadInfo{ _LoadDriver::Um_UserCallBackType_UserEnumFileInfo, this };
	//HANDLE hThread = CreateThread(NULL, NULL, UniversalThreadFunction,/*变量参数地址*/(LPVOID)pThread, 0, &lpThreadId);
	//CloseHandle(hThread);


	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumFileInfo, this });

}

void DlgEnumFile::OnNMRClickEnumfileList(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;

	// 自适应构造 "复制 ▸ 各列 / 刷新"，并保留原有 "强制删除文件 / 解除文件占用" 两项。
	CHeaderCtrl* hdr = m_CListCtrl.GetHeaderCtrl();
	int nCols = hdr ? hdr->GetItemCount() : 0;
	bool hasSel = (m_CListCtrl.GetFirstSelectedItemPosition() != NULL);
	bool hasItems = (m_CListCtrl.GetItemCount() > 0);

	// 取选中行对应的文件全路径，用于"打开/资源管理器/属性/打开方式/重命名"等
	CString selPath;
	int selRow = -1;
	if (hasSel)
	{
		POSITION p = m_CListCtrl.GetFirstSelectedItemPosition();
		selRow = m_CListCtrl.GetNextSelectedItem(p);
		CString* pStr = (CString*)m_CListCtrl.GetItemData(selRow);
		if (pStr) selPath = *pStr;
	}

	// 选中项是否是可执行类型，决定"以管理员身份运行"是否可点
	bool isExe = false;
	if (!selPath.IsEmpty())
	{
		int dot = selPath.ReverseFind(L'.');
		if (dot >= 0)
		{
			CString ext = selPath.Mid(dot + 1);
			ext.MakeLower();
			isExe = (ext == L"exe" || ext == L"bat" || ext == L"cmd" || ext == L"com" || ext == L"msi");
		}
	}

	const UINT kCopyBase       = 9001;
	const UINT kRefresh        = 9000;
	const UINT kFileOpen       = 9020;
	const UINT kFileRunAs      = 9021;
	const UINT kFileOpenAs     = 9022;
	const UINT kFileOpenExp    = 9023;
	const UINT kFileCopyAsPath = 9024;
	const UINT kFileCopyDir    = 9025;
	const UINT kFileRename     = 9026;
	const UINT kFileProperties = 9027;
	const UINT kFileNotepad    = 9028; // 以记事本打开
	const UINT kFileCmd        = 9029; // 在此处打开命令行
	const UINT kFilePwsh       = 9030; // 在此处打开 PowerShell
	const UINT kFileHash       = 9031; // 计算哈希
	const UINT kFileVerify     = 9032; // 检查数字签名
	const UINT kFileNewText    = 9034; // 新建文本文件

	CMenu copySub;
	copySub.CreatePopupMenu();
	for (int i = 0; i < nCols; ++i)
	{
		wchar_t buf[128] = { 0 };
		HDITEMW hi = { 0 };
		hi.mask = HDI_TEXT; hi.pszText = buf; hi.cchTextMax = _countof(buf);
		Header_GetItem(hdr->GetSafeHwnd(), i, &hi);
		copySub.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), kCopyBase + i, buf);
	}

	CMenu menu;
	menu.CreatePopupMenu();
	// 打开/运行 类
	menu.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), kFileOpen,    L"打开");
	menu.AppendMenuW(MF_STRING | (hasSel && isExe ? 0 : MF_GRAYED), kFileRunAs, L"以管理员身份运行");
	menu.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), kFileOpenAs,  L"打开方式...");
	menu.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), kFileNotepad, L"以记事本打开");
	menu.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), kFileOpenExp, L"在资源管理器中显示");
	menu.AppendMenuW(MF_SEPARATOR);

	// 在当前目录打开终端 / 新建
	menu.AppendMenuW(MF_STRING | (!m_CurPath.IsEmpty() ? 0 : MF_GRAYED), kFileCmd,  L"在此处打开命令行");
	menu.AppendMenuW(MF_STRING | (!m_CurPath.IsEmpty() ? 0 : MF_GRAYED), kFilePwsh, L"在此处打开 PowerShell");
	menu.AppendMenuW(MF_STRING | (!m_CurPath.IsEmpty() ? 0 : MF_GRAYED), kFileNewText,   L"新建文本文件");
	menu.AppendMenuW(MF_SEPARATOR);

	// 剪贴板/复制类
	if (nCols > 0) menu.AppendMenuW(MF_POPUP | (hasSel ? 0 : MF_GRAYED), (UINT_PTR)copySub.GetSafeHmenu(), L"复制");
	menu.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), kFileCopyAsPath, L"复制为路径");
	menu.AppendMenuW(MF_STRING | (!m_CurPath.IsEmpty() ? 0 : MF_GRAYED), kFileCopyDir, L"复制所在目录");
	menu.AppendMenuW(MF_SEPARATOR);

	// 重命名
	menu.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), kFileRename, L"重命名");
	menu.AppendMenuW(MF_SEPARATOR);

	// 危险/驱动相关
	menu.AppendMenuW(MF_STRING | (hasItems ? 0 : MF_GRAYED), ID_FILE_DELETE,       L"强制删除文件");
	menu.AppendMenuW(MF_STRING | (hasItems ? 0 : MF_GRAYED), ID_FILE_FILEDEOCCUPY, L"解除文件占用");
	menu.AppendMenuW(MF_SEPARATOR);

	// 哈希 / 签名
	menu.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), kFileHash,   L"计算 MD5 / SHA1 / SHA256");
	menu.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), kFileVerify, L"检查数字签名");
	menu.AppendMenuW(MF_SEPARATOR);

	// 属性 / 视图
	menu.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), kFileProperties, L"属性");
	menu.AppendMenuW(MF_STRING | (this->m_ThreadFlags == 1 ? MF_GRAYED : 0), kRefresh, L"刷新");

	POINT pt = { 0 }; GetCursorPos(&pt);
	UINT cmd = menu.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, this);
	copySub.Detach();

	if (cmd == 0) return;
	if (cmd == kRefresh) { OnFileRefresh(); return; }
	if (cmd >= kCopyBase && cmd < kCopyBase + (UINT)nCols)
	{
		CopyBufferToClipboard(&m_CListCtrl, (int)(cmd - kCopyBase));
		return;
	}
	if (cmd == kFileOpen && !selPath.IsEmpty())
	{
		ShellExecuteW(NULL, L"open", selPath, NULL, NULL, SW_SHOWNORMAL);
		return;
	}
	if (cmd == kFileRunAs && !selPath.IsEmpty())
	{
		ShellExecuteW(NULL, L"runas", selPath, NULL, NULL, SW_SHOWNORMAL);
		return;
	}
	if (cmd == kFileOpenAs && !selPath.IsEmpty())
	{
		OPENASINFO oai = { 0 };
		oai.pcszFile = selPath;
		oai.oaifInFlags = OAIF_EXEC | OAIF_HIDE_REGISTRATION;
		SHOpenWithDialog(GetSafeHwnd(), &oai);
		return;
	}
	if (cmd == kFileOpenExp && !selPath.IsEmpty())
	{
		PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(selPath);
		if (pidl)
		{
			SHOpenFolderAndSelectItems(pidl, 0, NULL, 0);
			ILFree(pidl);
		}
		return;
	}
	if ((cmd == kFileCopyAsPath && !selPath.IsEmpty()) ||
	    (cmd == kFileCopyDir && !m_CurPath.IsEmpty()))
	{
		CString text;
		if (cmd == kFileCopyAsPath)
		{
			text = L"\"" + selPath + L"\"";
		}
		else
		{
			text = m_CurPath;
			while (text.GetLength() > 3 && text[text.GetLength() - 1] == L'\\')
				text.Delete(text.GetLength() - 1);
		}
		if (OpenClipboard())
		{
			EmptyClipboard();
			size_t bytes = (text.GetLength() + 1) * sizeof(wchar_t);
			HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
			if (hMem)
			{
				void* dst = GlobalLock(hMem);
				if (dst) { memcpy(dst, (LPCWSTR)text, bytes); GlobalUnlock(hMem); }
				SetClipboardData(CF_UNICODETEXT, hMem);
			}
			CloseClipboard();
		}
		return;
	}
	if (cmd == kFileRename && selRow >= 0)
	{
		m_CListCtrl.SetFocus();
		m_CListCtrl.EditLabel(selRow);
		return;
	}
	if (cmd == kFileNotepad && !selPath.IsEmpty())
	{
		// 以记事本打开任意文件做快速文本预览
		ShellExecuteW(NULL, L"open", L"notepad.exe", L"\"" + selPath + L"\"", NULL, SW_SHOWNORMAL);
		return;
	}
	if (cmd == kFileCmd && !m_CurPath.IsEmpty())
	{
		// 以当前目录为工作目录打开 cmd（/K 保持窗口）
		ShellExecuteW(NULL, L"open", L"cmd.exe", L"/K cd /d \"" + m_CurPath + L"\"", NULL, SW_SHOWNORMAL);
		return;
	}
	if (cmd == kFilePwsh && !m_CurPath.IsEmpty())
	{
		ShellExecuteW(NULL, L"open", L"powershell.exe",
			L"-NoExit -Command \"Set-Location -LiteralPath '" + m_CurPath + L"'\"",
			NULL, SW_SHOWNORMAL);
		return;
	}
	if (cmd == kFileNewText && !m_CurPath.IsEmpty())
	{
		CString base = m_CurPath + L"新建文本文档.txt";
		CString full = base;
		for (int n = 2; n <= 99; ++n)
		{
			if (GetFileAttributesW(full) == INVALID_FILE_ATTRIBUTES) break;
			CString stem = m_CurPath + L"新建文本文档";
			full.Format(L"%s (%d).txt", (LPCWSTR)stem, n);
		}
		HANDLE h = CreateFileW(full, GENERIC_WRITE, 0, NULL, CREATE_NEW,
			FILE_ATTRIBUTE_NORMAL, NULL);
		if (h != INVALID_HANDLE_VALUE)
		{
			CloseHandle(h);
			// 只更新列表：在末尾追加一行（不重新枚举整个目录）
			CString name = full.Mid(m_CurPath.GetLength());
			int row = m_CListCtrl.GetItemCount();
			m_CListCtrl.InsertItem(row, name);
			// 时间/大小先填 "--"，下一次刷新会更准
			m_CListCtrl.SetItemText(row, 1, L"--");
			m_CListCtrl.SetItemText(row, 2, L"--");
			m_CListCtrl.SetItemText(row, 3, L"0");
			CString* pFull = new CString(full);
			m_CListCtrl.SetItemData(row, (DWORD_PTR)pFull);
			m_CListCtrl.EnsureVisible(row, FALSE);
		}
		return;
	}
	if (cmd == kFileHash && !selPath.IsEmpty())
	{
		ShowFileHashesDialog(GetSafeHwnd(), selPath);
		return;
	}
	if (cmd == kFileVerify && !selPath.IsEmpty())
	{
		VerifyFileSignatureDialog(GetSafeHwnd(), selPath);
		return;
	}
	if (cmd == kFileProperties && !selPath.IsEmpty())
	{
		SHObjectProperties(GetSafeHwnd(), SHOP_FILEPATH, selPath, NULL);
		return;
	}
	if (cmd == ID_FILE_DELETE || cmd == ID_FILE_FILEDEOCCUPY)
	{
		// 路由到现有的 ON_COMMAND 处理函数
		SendMessageW(WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
		return;
	}
}

// 列表项标签编辑结束(重命名)：用 MoveFileW 改名，并同步行内 CString*。
void DlgEnumFile::OnEndLabelEditEnumfileList(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLVDISPINFO* pInfo = (NMLVDISPINFO*)pNMHDR;
	*pResult = FALSE; // 默认不接受编辑结果

	if (pInfo == NULL || pInfo->item.pszText == NULL) return;
	int row = pInfo->item.iItem;
	if (row < 0) return;

	CString newName = pInfo->item.pszText;
	newName.Trim();
	if (newName.IsEmpty()) return;

	if (newName.FindOneOf(L"\\/:*?\"<>|") >= 0)
	{
		::MessageBoxW(GetSafeHwnd(), L"文件名不能包含 \\ / : * ? \" < > |", L"提示", MB_OK | MB_ICONWARNING);
		return;
	}

	CString* pOld = (CString*)m_CListCtrl.GetItemData(row);
	if (pOld == NULL || pOld->IsEmpty()) return;

	CString oldPath = *pOld;
	int slash = oldPath.ReverseFind(L'\\');
	if (slash < 0) return;
	CString dir = oldPath.Left(slash + 1);
	CString newPath = dir + newName;

	if (oldPath.CompareNoCase(newPath) == 0) return;

	if (!MoveFileW(oldPath, newPath))
	{
		DWORD err = GetLastError();
		CString msg;
		msg.Format(L"重命名失败，错误码: %lu", err);
		::MessageBoxW(GetSafeHwnd(), msg, L"提示", MB_OK | MB_ICONERROR);
		return;
	}

	*pOld = newPath;
	*pResult = TRUE;
}

// ---------------------------------------------------------------------------
//  Tree 控件：右键菜单 + 标签编辑回调
// ---------------------------------------------------------------------------

void DlgEnumFile::OnNMRClickEnumfileTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;

	// 把光标点击到的节点选中（避免对当前选中节点误操作）
	POINT ptScreen = { 0 }; GetCursorPos(&ptScreen);
	POINT ptClient = ptScreen; m_CTreeCtrl.ScreenToClient(&ptClient);
	UINT flags = 0;
	HTREEITEM hHit = m_CTreeCtrl.HitTest(ptClient, &flags);
	if (hHit) m_CTreeCtrl.SelectItem(hHit);

	HTREEITEM hSel = m_CTreeCtrl.GetSelectedItem();
	CString dirPath;
	if (hSel)
	{
		CString* pData = (CString*)m_CTreeCtrl.GetItemData(hSel);
		if (pData) dirPath = *pData;
	}
	BOOL hasSel = (hSel != NULL) && !dirPath.IsEmpty();
	// 是否是根盘符（"X:\"），根节点不允许重命名/删除
	BOOL isRoot = (hSel != NULL) && m_CTreeCtrl.GetParentItem(hSel) == NULL;

	const UINT kTreeNewFolder = 9300;
	const UINT kTreeRename    = 9301;
	const UINT kTreeDelete    = 9302;
	const UINT kTreeProps     = 9303;
	const UINT kTreeOpenExp   = 9304;
	const UINT kTreeRefresh   = 9305;
	const UINT kTreeCopyPath  = 9306;

	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), kTreeNewFolder, L"新建文件夹");
	menu.AppendMenuW(MF_SEPARATOR);
	menu.AppendMenuW(MF_STRING | (hasSel && !isRoot ? 0 : MF_GRAYED), kTreeRename, L"重命名");
	menu.AppendMenuW(MF_STRING | (hasSel && !isRoot ? 0 : MF_GRAYED), kTreeDelete, L"删除");
	menu.AppendMenuW(MF_SEPARATOR);
	menu.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), kTreeOpenExp,  L"在资源管理器中显示");
	menu.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), kTreeCopyPath, L"复制为路径");
	menu.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), kTreeProps,    L"属性");
	menu.AppendMenuW(MF_SEPARATOR);
	menu.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), kTreeRefresh,  L"刷新");

	UINT cmd = menu.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
		ptScreen.x, ptScreen.y, this);
	if (cmd == 0) return;

	if (cmd == kTreeNewFolder)
	{
		// 在当前选中目录下创建新文件夹，并把它插到树里
		CString base = dirPath + L"新建文件夹";
		CString full = base;
		for (int n = 2; n <= 99 && !CreateDirectoryW(full, NULL); ++n)
		{
			if (GetLastError() != ERROR_ALREADY_EXISTS) { full.Empty(); break; }
			full.Format(L"%s (%d)", (LPCWSTR)base, n);
		}
		if (!full.IsEmpty())
		{
			CString name = full.Mid(dirPath.GetLength());
			TVINSERTSTRUCTW ti = { 0 };
			ti.hParent = hSel;
			ti.hInsertAfter = TVI_LAST;
			ti.itemex.mask = TVIF_TEXT | TVIF_CHILDREN;
			ti.itemex.pszText = (LPWSTR)(LPCWSTR)name;
			ti.itemex.cChildren = 1;
			HTREEITEM hNew = m_CTreeCtrl.InsertItem(&ti);
			CString* pData = new CString();
			pData->Format(L"%s\\", (LPCWSTR)full);
			m_CTreeCtrl.SetItemData(hNew, (DWORD_PTR)pData);
			m_CTreeCtrl.Expand(hSel, TVE_EXPAND);
			m_CTreeCtrl.EnsureVisible(hNew);
		}
		return;
	}
	if (cmd == kTreeRename && hasSel && !isRoot)
	{
		m_CTreeCtrl.SetFocus();
		m_CTreeCtrl.EditLabel(hSel);
		return;
	}
	if (cmd == kTreeDelete && hasSel && !isRoot)
	{
		CString dirNoSlash = dirPath;
		while (dirNoSlash.GetLength() > 3 && dirNoSlash[dirNoSlash.GetLength() - 1] == L'\\')
			dirNoSlash.Delete(dirNoSlash.GetLength() - 1);
		CString prompt;
		prompt.Format(L"确认删除目录及其全部内容？\n\n%s", (LPCWSTR)dirNoSlash);
		if (::MessageBoxW(GetSafeHwnd(), prompt, L"删除目录",
			MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2) != IDOK) return;

		// SHFileOperationW 处理非空目录递归删除
		WCHAR buf[MAX_PATH + 2] = { 0 };
		wcsncpy_s(buf, _countof(buf), dirNoSlash, _TRUNCATE);
		SHFILEOPSTRUCTW fo = { 0 };
		fo.hwnd = GetSafeHwnd();
		fo.wFunc = FO_DELETE;
		fo.pFrom = buf;
		fo.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
		if (SHFileOperationW(&fo) == 0 && !fo.fAnyOperationsAborted)
		{
			// 释放子树绑定的 CString*
			DelTreeChild(hSel);
			CString* pData = (CString*)m_CTreeCtrl.GetItemData(hSel);
			if (pData) { delete pData; m_CTreeCtrl.SetItemData(hSel, 0); }
			m_CTreeCtrl.DeleteItem(hSel);
		}
		else
		{
			::MessageBoxW(GetSafeHwnd(), L"删除失败。", L"提示", MB_OK | MB_ICONERROR);
		}
		return;
	}
	if (cmd == kTreeOpenExp && hasSel)
	{
		CString dirNoSlash = dirPath;
		while (dirNoSlash.GetLength() > 3 && dirNoSlash[dirNoSlash.GetLength() - 1] == L'\\')
			dirNoSlash.Delete(dirNoSlash.GetLength() - 1);
		ShellExecuteW(NULL, L"open", L"explorer.exe",
			L"/select,\"" + dirNoSlash + L"\"", NULL, SW_SHOWNORMAL);
		return;
	}
	if (cmd == kTreeCopyPath && hasSel)
	{
		CString dirNoSlash = dirPath;
		while (dirNoSlash.GetLength() > 3 && dirNoSlash[dirNoSlash.GetLength() - 1] == L'\\')
			dirNoSlash.Delete(dirNoSlash.GetLength() - 1);
		CString quoted = L"\"" + dirNoSlash + L"\"";
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
	if (cmd == kTreeProps && hasSel)
	{
		CString dirNoSlash = dirPath;
		while (dirNoSlash.GetLength() > 3 && dirNoSlash[dirNoSlash.GetLength() - 1] == L'\\')
			dirNoSlash.Delete(dirNoSlash.GetLength() - 1);
		SHObjectProperties(GetSafeHwnd(), SHOP_FILEPATH, dirNoSlash, NULL);
		return;
	}
	if (cmd == kTreeRefresh && hasSel)
	{
		m_CurPath = dirPath;
		OnFileRefresh();
		return;
	}
}

// 树标签编辑结束：重命名目录
void DlgEnumFile::OnEndLabelEditEnumfileTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMTVDISPINFO* pInfo = (NMTVDISPINFO*)pNMHDR;
	*pResult = FALSE;

	if (!pInfo || !pInfo->item.pszText) return;
	HTREEITEM hItem = pInfo->item.hItem;
	if (!hItem) return;
	if (m_CTreeCtrl.GetParentItem(hItem) == NULL) return; // 不允许重命名根盘符

	CString newName = pInfo->item.pszText;
	newName.Trim();
	if (newName.IsEmpty()) return;
	if (newName.FindOneOf(L"\\/:*?\"<>|") >= 0)
	{
		::MessageBoxW(GetSafeHwnd(), L"目录名不能包含 \\ / : * ? \" < > |",
			L"提示", MB_OK | MB_ICONWARNING);
		return;
	}

	CString* pOld = (CString*)m_CTreeCtrl.GetItemData(hItem);
	if (!pOld || pOld->IsEmpty()) return;

	CString oldFull = *pOld; // 形如 "C:\Foo\Bar\"
	CString trimmed = oldFull;
	while (trimmed.GetLength() > 3 && trimmed[trimmed.GetLength() - 1] == L'\\')
		trimmed.Delete(trimmed.GetLength() - 1);
	int slash = trimmed.ReverseFind(L'\\');
	if (slash < 0) return;
	CString parentDir = trimmed.Left(slash + 1);
	CString newFull   = parentDir + newName;

	if (oldFull.Left(oldFull.GetLength() - 1).CompareNoCase(newFull) == 0) return;

	if (!MoveFileW(trimmed, newFull))
	{
		DWORD err = GetLastError();
		CString msg; msg.Format(L"重命名失败，错误码: %lu", err);
		::MessageBoxW(GetSafeHwnd(), msg, L"提示", MB_OK | MB_ICONERROR);
		return;
	}

	// 更新节点绑定数据；子树绑定的旧路径将在下次刷新时重新生成
	*pOld = newFull + L"\\";
	*pResult = TRUE;
}

// 由"进程模块"等页面调用，跳转到指定文件所在目录并选中该文件。
// 沿路径一级一级在 Tree 上展开/选中节点，再选中目标行；
// 这样后续 EnunFile() 的 hSelectItem 始终是正确的父节点，
// 不会把子目录插到根层级，避免破坏树形层级结构。
void DlgEnumFile::NavigateToFile(const CString& fullPath)
{
	if (fullPath.IsEmpty()) return;

	CString p = fullPath;
	p.Replace(L'/', L'\\');

	int colon = p.Find(L':');
	if (colon < 0) return;
	CString driveRoot = p.Left(colon + 1) + L"\\"; // 例如 "C:\\"

	// 把驱动器之后的部分拆成各级目录 + 末尾文件名
	CString rest = p.Mid(colon + 1);
	while (!rest.IsEmpty() && rest[0] == L'\\') rest = rest.Mid(1);

	std::vector<CString> parts;
	int s = 0;
	while (s < rest.GetLength())
	{
		int sl = rest.Find(L'\\', s);
		if (sl < 0) { parts.push_back(rest.Mid(s)); break; }
		if (sl > s) parts.push_back(rest.Mid(s, sl - s));
		s = sl + 1;
	}
	if (parts.empty()) return;

	CString fileName = parts.back();
	parts.pop_back(); // 余下的是目录链

	// 找到对应盘符的根节点（OnInitDialog 里根节点文本是 "C:", 数据是 CString("C:\\")）
	HTREEITEM hRoot = m_CTreeCtrl.GetRootItem();
	HTREEITEM hCur = NULL;
	while (hRoot)
	{
		CString* pData = (CString*)m_CTreeCtrl.GetItemData(hRoot);
		if (pData && pData->CompareNoCase(driveRoot) == 0) { hCur = hRoot; break; }
		hRoot = m_CTreeCtrl.GetNextSiblingItem(hRoot);
	}
	if (!hCur) return;

	// 在根盘符上同步刷新一次，让它的子节点（一级目录）出现
	m_CTreeCtrl.SelectItem(hCur);
	m_CTreeCtrl.Expand(hCur, TVE_EXPAND);
	m_CurPath = driveRoot;
	EnunFile();

	// 沿路径逐级匹配/展开
	for (const CString& comp : parts)
	{
		HTREEITEM child = m_CTreeCtrl.GetChildItem(hCur);
		HTREEITEM match = NULL;
		while (child)
		{
			if (m_CTreeCtrl.GetItemText(child).CompareNoCase(comp) == 0) { match = child; break; }
			child = m_CTreeCtrl.GetNextSiblingItem(child);
		}
		if (!match) break;
		hCur = match;
		m_CTreeCtrl.SelectItem(hCur);
		m_CTreeCtrl.Expand(hCur, TVE_EXPAND);
		CString* pData = (CString*)m_CTreeCtrl.GetItemData(hCur);
		if (pData) m_CurPath = *pData;
		EnunFile();
	}
	m_CTreeCtrl.EnsureVisible(hCur);

	// 在列表里高亮目标文件
	int cnt = m_CListCtrl.GetItemCount();
	for (int i = 0; i < cnt; ++i)
	{
		if (m_CListCtrl.GetItemText(i, 0).CompareNoCase(fileName) == 0)
		{
			m_CListCtrl.SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			m_CListCtrl.EnsureVisible(i, FALSE);
			m_CListCtrl.SetFocus();
			break;
		}
	}
}

// ---------------------------------------------------------------------------
//  顶部地址栏：后退 / 前进 / 跳转 / 搜索
// ---------------------------------------------------------------------------

void DlgEnumFile::UpdateNavButtons()
{
	if (m_BtnBack.GetSafeHwnd())
		m_BtnBack.EnableWindow(m_NavIndex > 0);
	if (m_BtnForward.GetSafeHwnd())
		m_BtnForward.EnableWindow(m_NavIndex >= 0 && m_NavIndex + 1 < (int)m_NavHistory.size());
	if (m_PathEdit.GetSafeHwnd() && m_NavIndex >= 0 && m_NavIndex < (int)m_NavHistory.size())
	{
		// 显示给用户：去掉末尾反斜杠，更接近 Explorer 的地址栏写法
		CString shown = m_NavHistory[m_NavIndex];
		while (shown.GetLength() > 3 && shown[shown.GetLength() - 1] == L'\\')
			shown.Delete(shown.GetLength() - 1);
		m_PathEdit.SetWindowTextW(shown);
	}
}

// 沿路径在 Tree 上一级一级展开/选中到目标目录，并填充列表
void DlgEnumFile::NavigateToDirectory(const CString& dirWithSlash, bool addHistory)
{
	if (dirWithSlash.IsEmpty()) return;
	CString p = dirWithSlash;
	p.Replace(L'/', L'\\');
	if (p[p.GetLength() - 1] != L'\\') p += L'\\';

	int colon = p.Find(L':');
	if (colon < 0) return;
	CString driveRoot = p.Left(colon + 1) + L"\\"; // "C:\"

	CString rest = p.Mid(colon + 1);
	while (!rest.IsEmpty() && rest[0] == L'\\') rest = rest.Mid(1);
	std::vector<CString> parts;
	int s = 0;
	while (s < rest.GetLength())
	{
		int sl = rest.Find(L'\\', s);
		if (sl < 0) { if (s < rest.GetLength()) parts.push_back(rest.Mid(s)); break; }
		if (sl > s) parts.push_back(rest.Mid(s, sl - s));
		s = sl + 1;
	}

	HTREEITEM hRoot = m_CTreeCtrl.GetRootItem();
	HTREEITEM hCur = NULL;
	while (hRoot)
	{
		CString* pData = (CString*)m_CTreeCtrl.GetItemData(hRoot);
		if (pData && pData->CompareNoCase(driveRoot) == 0) { hCur = hRoot; break; }
		hRoot = m_CTreeCtrl.GetNextSiblingItem(hRoot);
	}
	if (!hCur) return;

	m_CTreeCtrl.SelectItem(hCur);
	m_CTreeCtrl.Expand(hCur, TVE_EXPAND);
	m_CurPath = driveRoot;
	EnunFile();

	for (const CString& comp : parts)
	{
		HTREEITEM child = m_CTreeCtrl.GetChildItem(hCur);
		HTREEITEM match = NULL;
		while (child)
		{
			if (m_CTreeCtrl.GetItemText(child).CompareNoCase(comp) == 0) { match = child; break; }
			child = m_CTreeCtrl.GetNextSiblingItem(child);
		}
		if (!match) break;
		hCur = match;
		m_CTreeCtrl.SelectItem(hCur);
		m_CTreeCtrl.Expand(hCur, TVE_EXPAND);
		CString* pData = (CString*)m_CTreeCtrl.GetItemData(hCur);
		if (pData) m_CurPath = *pData;
		EnunFile();
	}
	m_CTreeCtrl.EnsureVisible(hCur);

	if (addHistory && !m_NavSuppressHistory)
	{
		// 截断当前位置之后的"前进"历史，再追加新位置（去重）
		if (m_NavIndex >= 0 && m_NavIndex + 1 < (int)m_NavHistory.size())
			m_NavHistory.erase(m_NavHistory.begin() + m_NavIndex + 1, m_NavHistory.end());
		CString cur = m_CurPath;
		if (m_NavIndex < 0 || m_NavHistory[m_NavIndex].CompareNoCase(cur) != 0)
		{
			m_NavHistory.push_back(cur);
			m_NavIndex = (int)m_NavHistory.size() - 1;
		}
	}
	UpdateNavButtons();
}

void DlgEnumFile::OnBtnNavBack()
{
	if (m_NavIndex <= 0) return;
	m_NavSuppressHistory = true;
	--m_NavIndex;
	NavigateToDirectory(m_NavHistory[m_NavIndex], false);
	m_NavSuppressHistory = false;
}

void DlgEnumFile::OnBtnNavForward()
{
	if (m_NavIndex + 1 >= (int)m_NavHistory.size()) return;
	m_NavSuppressHistory = true;
	++m_NavIndex;
	NavigateToDirectory(m_NavHistory[m_NavIndex], false);
	m_NavSuppressHistory = false;
}

void DlgEnumFile::OnBtnNavGo()
{
	CString text;
	m_PathEdit.GetWindowTextW(text);
	text.Trim();
	if (text.IsEmpty()) return;
	if (GetFileAttributesW(text) == INVALID_FILE_ATTRIBUTES)
	{
		::MessageBoxW(GetSafeHwnd(), L"路径不存在。", L"提示", MB_OK | MB_ICONWARNING);
		return;
	}
	NavigateToDirectory(text, true);
}

// 关键字过滤：把当前列表中文件名不含 keyword 的行删掉（不重新枚举目录）
void DlgEnumFile::ApplyListSearchFilter(const CString& keyword)
{
	if (keyword.IsEmpty())
	{
		// 关键字清空时重新拉一次目录恢复全量
		OnFileRefresh();
		return;
	}
	CString needle = keyword; needle.MakeLower();
	for (int i = m_CListCtrl.GetItemCount() - 1; i >= 0; --i)
	{
		CString name = m_CListCtrl.GetItemText(i, 0);
		CString lower = name; lower.MakeLower();
		if (lower.Find(needle) < 0)
		{
			CString* p = (CString*)m_CListCtrl.GetItemData(i);
			if (p) delete p;
			m_CListCtrl.DeleteItem(i);
		}
	}
}

BOOL DlgEnumFile::PreTranslateMessage(MSG* pMsg)
{
	// 让路径栏 / 搜索栏的回车键触发跳转/筛选，而不是关闭对话框
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
	{
		HWND hFocus = pMsg->hwnd;
		if (m_PathEdit.GetSafeHwnd() == hFocus)
		{
			OnBtnNavGo();
			return TRUE;
		}
		if (m_SearchEdit.GetSafeHwnd() == hFocus)
		{
			CString kw;
			m_SearchEdit.GetWindowTextW(kw);
			kw.Trim();
			ApplyListSearchFilter(kw);
			return TRUE;
		}
	}
	return CDialogEx::PreTranslateMessage(pMsg);
}

void DlgEnumFile::OnFileFiledeoccupy()
{
	
	//DWORD lpThreadId = 0;
	//
	//CThreadInfo* pThread = new CThreadInfo{ _LoadDriver::Um_UserCallBackType_UserFileDeoccupy, this };
	//HANDLE hThread = CreateThread(NULL, NULL, UniversalThreadFunction,/*变量参数地址*/(LPVOID)pThread, 0, &lpThreadId);
	//
	//CloseHandle(hThread);

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserFileDeoccupy, this });

}

void DlgEnumFile::FileFiledeoccupy()
{
	if (m_CListCtrl.GetItemCount() <= 0)
	{
		return;
	}

	//获取当前选择的控件项
	POSITION FristIndex = m_CListCtrl.GetFirstSelectedItemPosition();//获取选中行的行数  pos = 行数 - 1
	int TempIndex = (int)FristIndex - 1;//存储第一次的索引位置

	//获取绑定的数据
	CString* pStrData = (CString*)m_CListCtrl.GetItemData(TempIndex);
	if (pStrData == NULL)
	{
		return;
	}

	//想驱动发送删除文件的消息
	ULONG64 dqRet = g_LoadDriver.SendMsg(um_Cmd_FileDeoccupy_info, pStrData->GetBuffer());
	if (dqRet == 0)
	{
		::MessageBoxW(NULL, L"文件解除占用成功!", L"提示", MB_OK);
	}
	else
	{
		::MessageBoxW(NULL, L"文件解除占用失败!", L"提示", MB_OK);
	}

}