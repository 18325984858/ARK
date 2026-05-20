// DlgEnumRegistry.cpp: 实现文件
//
#define _CRT_NON_CONFORMING_SWPRINTFS
#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgEnumRegistry.h"
#include "Thread.h"

// DlgEnumRegistry 对话框

IMPLEMENT_DYNAMIC(DlgEnumRegistry, CDialogEx)

DlgEnumRegistry::DlgEnumRegistry(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_ENUMREGISTRY, pParent)
{
	m_ThreadFlags = FALSE;
}

DlgEnumRegistry::~DlgEnumRegistry()
{
}

void DlgEnumRegistry::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_ENUMREGSITRY_TREE, m_CTreeCtrl);
	DDX_Control(pDX, ID_ENUMREGSITRY_LIST, m_CListCtrl);
}

BEGIN_MESSAGE_MAP(DlgEnumRegistry, CDialogEx)
	ON_WM_SIZE()
	ON_NOTIFY(NM_DBLCLK, ID_ENUMREGSITRY_TREE, &DlgEnumRegistry::OnNMDblclkEnumregsitryTree)
END_MESSAGE_MAP()

// DlgEnumRegistry 消息处理程序

void DlgEnumRegistry::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	CRect rect;
	GetClientRect(&rect);

	float fwidth = rect.Width() / 4;

	m_CTreeCtrl.SetWindowPos(NULL, 0, 0, fwidth, rect.Height(), SWP_NOZORDER);
	m_CListCtrl.SetWindowPos(NULL, fwidth, 0, rect.Width() - fwidth, rect.Height(), SWP_NOZORDER);

	// TODO: 在此处添加消息处理程序代码
}

BOOL DlgEnumRegistry::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	InitControl();

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

ULONG64 DlgEnumRegistry::InitControl()
{
	//初始化Tree控件
	m_CTreeCtrl.SetExtendedStyle(m_CTreeCtrl.GetExtendedStyle() | TVS_FULLROWSELECT | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS, m_CTreeCtrl.GetExtendedStyle() | TVS_FULLROWSELECT | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS);

	CString TreeValue[] = { L"\\Registry\\Machine\\",L"\\Registry\\User\\" };
	CString TreeTitle[] = { L"HKEY_LOCAL_MACHINE",L"HKEY_USERS",L"HKEY_CLASSES_ROOT",L"HKEY_CURRENT_USER" };

	for (int i = 0; i < sizeof(TreeValue) / sizeof(CString); i++)
	{
		HTREEITEM  hTreeItrm = m_CTreeCtrl.InsertItem(TreeTitle[i]);
		CString* pData = new CString(TreeValue[i]);
		m_CTreeCtrl.SetItemData(hTreeItrm, (DWORD_PTR)pData);
	}

	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	CString ListTitle[] = { L"名称",L"类型",L"数据" };
	for (int i = 0; i < sizeof(ListTitle) / sizeof(CString); i++)
	{
		m_CListCtrl.InsertColumn(i, ListTitle[i]);
		m_CListCtrl.SetColumnWidth(i, 200);
	}
	return TRUE;
}

void DlgEnumRegistry::DelTreeChild(HTREEITEM pNode)
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

void DlgEnumRegistry::InsertCtrlListControl()
{
	//获取选择的项
	auto hSelectItem = m_CTreeCtrl.GetSelectedItem();

	//获取绑定的数据
	CString* pStr = (CString*)m_CTreeCtrl.GetItemData(hSelectItem);

	CString strtmp = pStr->GetString();
	//判断是否是目录不是返回

	DelTreeChild(hSelectItem);/*删除当前选择节点的所有孩子*/
	m_CListCtrl.DeleteAllItems();//删除全部数据

	//发送消息
	PCRegistryInfo pRegistryInfo = NULL;
	g_LoadDriver.SendMsg(um_Cmd_Enum_Registry_info, strtmp.GetBuffer(), (PVOID64*)&pRegistryInfo);

	if (pRegistryInfo == NULL)
	{
		return;
	}

	SIZE_T FreeSize = 0;
	PCLIST_ENTRY pCurList = &pRegistryInfo->List;
	do
	{
		if (pCurList == NULL)
		{
			return;
		}

		PCRegistryInfo pCurRegistryInfo = (PCRegistryInfo)pCurList;

		if (pCurRegistryInfo->nType == 0)
		{
			//0为 _KeyInfo结构
			//CRegistryInfo::Reserve1::_KeyInfo* pKey = (CRegistryInfo::Reserve1::_KeyInfo*)((ULONG64)pCurRegistryInfo + sizeof(CLIST_ENTRY) + (sizeof(ULONG64) * 2));
			//插入到Tree中

						//目录插入到Tree控件中
			CString* pNewFilePath = new CString();
			//拼接字符串  
			pNewFilePath->Format(L"%ws%ws\\", strtmp.GetBuffer(), pCurRegistryInfo->KeyName);

			TVINSERTSTRUCTW tvInsertTree = { 0 };

			tvInsertTree.hParent = hSelectItem;
			tvInsertTree.hInsertAfter = TVI_LAST;
			tvInsertTree.itemex.mask = TVIF_TEXT | TVIF_CHILDREN;
			tvInsertTree.itemex.pszText = pCurRegistryInfo->KeyName;
			tvInsertTree.itemex.cchTextMax = wcslen(pCurRegistryInfo->KeyName) + 1;
			tvInsertTree.itemex.cChildren = 1;

			auto hCurrentItem = m_CTreeCtrl.InsertItem(&tvInsertTree);
			/*设置与当前行绑定的数据*/
			m_CTreeCtrl.SetItemData(hCurrentItem, (DWORD_PTR)pNewFilePath);
		}
		else
		{
			//1为 _ValueInfo结构
			//CRegistryInfo::Reserve1::_ValueInfo* pValue = (CRegistryInfo::Reserve1::_ValueInfo*)((ULONG64)pCurRegistryInfo + sizeof(CLIST_ENTRY) + (sizeof(ULONG64) * 2));
			//插入到List控件中

			if (pCurRegistryInfo->ValueName[0] == 0 && pCurRegistryInfo->ValueName[1] == 0)
			{
				m_CListCtrl.InsertItem(0, L"默认");
			}
			else
			{
				m_CListCtrl.InsertItem(0, pCurRegistryInfo->ValueName);
			}

			WCHAR szBuf[MY_MAX_PATH] = { 0 };
			switch (pCurRegistryInfo->ValueType)
			{
			case REG_SZ:
				m_CListCtrl.SetItemText(0, 1, L"REG_SZ");
				m_CListCtrl.SetItemText(0, 2, pCurRegistryInfo->ValueData);
				break;
			case REG_MULTI_SZ:
				m_CListCtrl.SetItemText(0, 1, L"REG_MULTI_SZ");
				m_CListCtrl.SetItemText(0, 2, pCurRegistryInfo->ValueData);
				break;
			case REG_DWORD:
				m_CListCtrl.SetItemText(0, 1, L"REG_DWORD");
				swprintf(szBuf, L"0x%08X", *(PULONG32)pCurRegistryInfo->ValueData);
				m_CListCtrl.SetItemText(0, 2, szBuf);
				break;
			case REG_BINARY:
				m_CListCtrl.SetItemText(0, 1, L"REG_BINARY");
				for (int i = 0; i < sizeof(pCurRegistryInfo->ValueData); i++)
				{
					if (pCurRegistryInfo->ValueData[i] == 0)
					{
						break;
					}
					swprintf(&szBuf[i], L"%02X", pCurRegistryInfo->ValueData[i]);
				}
				m_CListCtrl.SetItemText(0, 2, szBuf);
				break;
			case REG_QWORD:
				m_CListCtrl.SetItemText(0, 1, L"REG_QWORD");
				swprintf(szBuf, L"%016I64X", *(PULONG64)pCurRegistryInfo->ValueData);
				m_CListCtrl.SetItemText(0, 2, szBuf);
				break;

			default:
				m_CListCtrl.SetItemText(0, 1, L"其它");
				for (int i = 0; i < sizeof(pCurRegistryInfo->ValueData); i++)
				{
					if (pCurRegistryInfo->ValueData[i] == 0)
					{
						break;
					}
					swprintf(&szBuf[i], L"%02X", pCurRegistryInfo->ValueData[i]);
				}
				m_CListCtrl.SetItemText(0, 2, szBuf);
				break;
			}
		}
		pCurList = pCurList->Flink;
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pCurRegistryInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}
	} while (pCurList != &pRegistryInfo->List);
}

void DlgEnumRegistry::OnNMDblclkEnumregsitryTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	// TODO: 在此添加控件通知处理程序代码
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

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumRegistryInfo, this });

}
