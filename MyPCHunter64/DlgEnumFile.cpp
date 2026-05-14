// DlgEnumFile.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgEnumFile.h"
#include "Thread.h"

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

	for (int i = 0; i < sizeof(szBuf1) / sizeof(TCHAR*); i++)
	{
		m_CListCtrl.InsertColumn(i, szBuf1[i]);
		m_CListCtrl.SetColumnWidth(i, 200);
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void DlgEnumFile::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	CRect rect;
	GetClientRect(&rect);

	float fwidth = rect.Width() / 4;

	m_CTreeCtrl.SetWindowPos(NULL, 0, 0, fwidth, rect.Height(), SWP_NOZORDER);
	m_CListCtrl.SetWindowPos(NULL, fwidth, 0, rect.Width() - fwidth, rect.Height(), SWP_NOZORDER);
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

			CString strBuf;
			strBuf.Format(L"%d/%d/%d--%d:%d:%d:%d",
				pCurFileInfo->CreationTime.Year,
				pCurFileInfo->CreationTime.Month,
				pCurFileInfo->CreationTime.Day,
				pCurFileInfo->CreationTime.Hour,
				pCurFileInfo->CreationTime.Minute,
				pCurFileInfo->CreationTime.Second,
				pCurFileInfo->CreationTime.Milliseconds);
			m_CListCtrl.SetItemText(i, 1, strBuf);

			strBuf.Format(L"%d/%d/%d--%d:%d:%d:%d",
				pCurFileInfo->ChangeTime.Year,
				pCurFileInfo->ChangeTime.Month,
				pCurFileInfo->ChangeTime.Day,
				pCurFileInfo->ChangeTime.Hour,
				pCurFileInfo->ChangeTime.Minute,
				pCurFileInfo->ChangeTime.Second,
				pCurFileInfo->ChangeTime.Milliseconds);
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

	const UINT kCopyBase = 9001;
	const UINT kRefresh  = 9000;

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
	if (nCols > 0) menu.AppendMenuW(MF_POPUP | (hasSel ? 0 : MF_GRAYED), (UINT_PTR)copySub.GetSafeHmenu(), L"复制");
	menu.AppendMenuW(MF_STRING | (this->m_ThreadFlags == 1 ? MF_GRAYED : 0), kRefresh, L"刷新");
	menu.AppendMenuW(MF_SEPARATOR);
	menu.AppendMenuW(MF_STRING | (hasItems ? 0 : MF_GRAYED), ID_FILE_DELETE,       L"强制删除文件");
	menu.AppendMenuW(MF_STRING | (hasItems ? 0 : MF_GRAYED), ID_FILE_FILEDEOCCUPY, L"解除文件占用");

	POINT pt = { 0 }; GetCursorPos(&pt);
	UINT cmd = menu.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, this);
	copySub.Detach();

	if (cmd == kRefresh) { OnFileRefresh(); return; }
	if (cmd >= kCopyBase && cmd < kCopyBase + (UINT)nCols)
	{
		CopyBufferToClipboard(&m_CListCtrl, (int)(cmd - kCopyBase));
		return;
	}
	if (cmd == ID_FILE_DELETE || cmd == ID_FILE_FILEDEOCCUPY)
	{
		// 路由到现有的 ON_COMMAND 处理函数
		SendMessageW(WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
		return;
	}
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