// DlgObjectCallBack.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgObjectCallBack.h"
#include "Thread.h"
#include "PdbResolver.h"
#include "resource.h"

// DlgObjectCallBack 对话框

IMPLEMENT_DYNAMIC(DlgObjectCallBack, CDialogEx)

DlgObjectCallBack::DlgObjectCallBack(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_KERNEL_OBJECTCALLBACK, pParent)
{

}

DlgObjectCallBack::~DlgObjectCallBack()
{
}

void DlgObjectCallBack::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_DLG_KERNEL_OBJECTCALLBACK_LIST, m_CListCtrl);
	DDX_Control(pDX, ID_DLG_KERNEL_OBJECTCALLBACK_TREE, m_CTreeCtrl);
}


BEGIN_MESSAGE_MAP(DlgObjectCallBack, CDialogEx)
	ON_WM_SIZE()
	ON_NOTIFY(NM_RCLICK, ID_DLG_KERNEL_OBJECTCALLBACK_LIST, &DlgObjectCallBack::OnNMRClickDlgKernelObjectcallbackList)
	ON_COMMAND(ID_OBJECTCALLBACK_REFRESH, &DlgObjectCallBack::OnObjectcallbackRefresh)
	ON_NOTIFY(NM_DBLCLK, ID_DLG_KERNEL_OBJECTCALLBACK_TREE, &DlgObjectCallBack::OnNMDblclkDlgKernelObjectcallbackTree)
END_MESSAGE_MAP()


// DlgObjectCallBack 消息处理程序


void DlgObjectCallBack::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	CRect rect;
	GetClientRect(&rect);

	float fwidth = rect.Width() / 4;

	m_CTreeCtrl.SetWindowPos(NULL, 0, 0, fwidth, rect.Height(), SWP_NOZORDER);
	m_CListCtrl.SetWindowPos(NULL, fwidth, 0, rect.Width() - fwidth, rect.Height(), SWP_NOZORDER);
}

BOOL DlgObjectCallBack::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	//设置控件类型
	m_CTreeCtrl.SetExtendedStyle(m_CTreeCtrl.GetExtendedStyle() | TVS_FULLROWSELECT | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
		m_CTreeCtrl.GetExtendedStyle() | TVS_FULLROWSELECT | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS);

	//创建根节点
	auto RootNode = m_CTreeCtrl.InsertItem(L"对象类型回调");
	//创建子节点1
	auto ChildNode0 = m_CTreeCtrl.InsertItem(L"ObjectTypeCallBack", RootNode);
	m_CTreeCtrl.SetItemData(ChildNode0, um_ObjectCallBack_Type);
	//创建子节点2
	auto ChildNode1 = m_CTreeCtrl.InsertItem(L"ObjectTypeCallBackInfo", RootNode);
	m_CTreeCtrl.SetItemData(ChildNode1, um_ObjectCallBackInfo_Type);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void DlgObjectCallBack::OnNMRClickDlgKernelObjectcallbackList(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	if (!nPerSel) return;
	int r = ShowListContextMenu(&m_CListCtrl, this);
	if (r == 0) { if (this->m_ThreadFlags != 1) OnObjectcallbackRefresh(); }
	else if (r > 0) CopyBufferToClipboard(&m_CListCtrl, r - 1);
}

void DlgObjectCallBack::OnObjectcallbackRefresh()
{
	m_CListCtrl.DeleteAllItems();
	//DWORD lpThreadId = 0;
	////HANDLE hThread = CreateThread(NULL, NULL, EnumObjectCallBackThreadProc,/*变量参数地址*/(LPVOID)this, 0, &lpThreadId);
	//
	//CThreadInfo* pThread = new CThreadInfo{ _LoadDriver::Um_UserCallBackType_UserEnumObjectCallBackInfo, this };
	//HANDLE hThread = CreateThread(NULL, NULL, UniversalThreadFunction,/*变量参数地址*/(LPVOID)pThread, 0, &lpThreadId);
	//CloseHandle(hThread);

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumObjectCallBackInfo, this });

}

void DlgObjectCallBack::InsertCtrlListControl(PCObjectTypeCallBackExInfo pCallBackInfo)
{
	if (pCallBackInfo == NULL)
	{
		return;
	}
	PCLIST_ENTRY pCurList = &pCallBackInfo->List.List;

	CString Type[] = { L"DumpProcedure" ,L"OpenProcedure",L"CloseProcedure",L"DeleteProcedure",L"ParseProcedure",L"SecurityProcedure",L"QueryNameProcedure",L"OkayToCloseProcedure" };

	do
	{
		if (pCurList == NULL)
		{
			break;
		}

		PCObjectTypeCallBackExInfo pInfo = (PCObjectTypeCallBackExInfo)pCurList;
		//显示

		CString StrBuf;

		//遍历当前的对象函数
		for (int j = 0; j < OBJECT_TYPE_CALLBACK_MAX_NUMBER; j++)
		{
			if (pInfo->FunAddr[j].FunAddr == 0)
			{
				continue;
			}
			ULONG64 i = m_CListCtrl.GetItemCount();
			//设置类型名
			m_CListCtrl.InsertItem(i, pInfo->TypeName);

			StrBuf.Format(L"0x%08I64X", pInfo->ValidAccessMask);
			m_CListCtrl.SetItemText(i, um_ObjectCallBackEx_Type_ValidAccessMask, StrBuf);

			StrBuf.Format(L"0x%016I64X", pInfo->FunAddr[j].FunAddr);
			m_CListCtrl.SetItemText(i, um_ObjectCallBackEx_Type_FunCallBack, StrBuf);

			m_CListCtrl.SetItemText(i, um_ObjectCallBackEx_Type_FunName, Type[pInfo->FunAddr[j].FunType]);

			// 位置：PdbResolver 解析符号
			{
				WCHAR resolved[256] = { 0 };
				PdbResolver_Resolve(pInfo->FunAddr[j].FunAddr, 0,
					pInfo->FunAddr[j].ModulePath, resolved, _countof(resolved));
				m_CListCtrl.SetItemText(i, um_ObjectCallBackEx_Type_Pos, resolved);
			}

			StrBuf.Format(L"0x%016I64X", pInfo->Object);
			m_CListCtrl.SetItemText(i, um_ObjectCallBackEx_Type_Object, StrBuf);

			CString FilePath = PathTransForm(pInfo->FunAddr[j].ModulePath);

			m_CListCtrl.SetItemText(i, um_ObjectCallBackEx_Type_ModulePath, FilePath.GetBuffer());

			CString szDstFileName;
			m_CListCtrl.SetItemText(i, um_ObjectCallBackEx_Type_Firm, TEXT("--"));
			if (this->GetCompanyName(FilePath, szDstFileName))
			{
				m_CListCtrl.SetItemText(i, um_ObjectCallBackEx_Type_Firm, (LPWSTR)szDstFileName.GetString());
			}
		}


		//获取下一个
		pCurList = pCurList->Blink;
		//释放资源
		SIZE_T FreeSize = 0;
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}
	} while (pCurList != &pCallBackInfo->List.List);
}

void DlgObjectCallBack::InsertCtrlListControl(PCObjectTypeCallBackInfo pCallBackInfo)
{
	if (pCallBackInfo == NULL)
	{
		return;
	}

	PCLIST_ENTRY pCurList = &pCallBackInfo->List.List;

	do
	{
		if (pCurList == NULL)
		{
			break;
		}
		ULONG64 i = m_CListCtrl.GetItemCount();
		PCObjectTypeCallBackInfo pInfo = (PCObjectTypeCallBackInfo)pCurList;

		CString StrBuf;

		//设置类型名
		m_CListCtrl.InsertItem(i, pInfo->szObjectTypeName);

		StrBuf.Format(L"0x%08I64X", pInfo->PreOperation);
		m_CListCtrl.SetItemText(i, um_ObjectCallBack_PreOperation, StrBuf);

		StrBuf.Format(L"0x%08I64X", pInfo->PostOperation);
		m_CListCtrl.SetItemText(i, um_ObjectCallBack_PostOperation, StrBuf);

		StrBuf.Format(L"0x%08I64X", pInfo->pHandle);
		m_CListCtrl.SetItemText(i, um_ObjectCallBack_pHandle, StrBuf);

		m_CListCtrl.SetItemText(i, um_ObjectCallBack_Altitude, pInfo->Altitude);

		CString FilePath = PathTransForm(pInfo->ModulePath);

		m_CListCtrl.SetItemText(i, um_ObjectCallBack_Moudle, FilePath.GetBuffer());

		CString szDstFileName;
		m_CListCtrl.SetItemText(i, um_ObjectCallBack_Firm, TEXT("--"));
		if (this->GetCompanyName(FilePath, szDstFileName))
		{
			m_CListCtrl.SetItemText(i, um_ObjectCallBack_Firm, (LPWSTR)szDstFileName.GetString());
		}

		//获取下一个
		pCurList = pCurList->Blink;
		//释放资源
		SIZE_T FreeSize = 0;
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}
	} while (pCurList != &pCallBackInfo->List.List);
}

void DlgObjectCallBack::OnNMDblclkDlgKernelObjectcallbackTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;


	//获取选择的子集
	auto hSelectItem = m_CTreeCtrl.GetSelectedItem();

	//获取绑定的数据
	int SelType = m_CTreeCtrl.GetItemData(hSelectItem);
	switch (SelType)
	{
	case um_ObjectCallBack_Type:
	{
		if (nPerSel != um_ObjectCallBack_Type)
		{
			//删除所有Column
			while (m_CListCtrl.DeleteColumn(0)) {}
			//初始化List控件
			m_CListCtrl.InsertColumn(um_ObjectCallBack_Type_Name, _T("类型"), LVCFMT_LEFT, 100);
			m_CListCtrl.InsertColumn(um_ObjectCallBack_PreOperation, _T("PreOperation"), LVCFMT_LEFT, 150);
			m_CListCtrl.InsertColumn(um_ObjectCallBack_PostOperation, _T("PostOperation"), LVCFMT_LEFT, 150);
			m_CListCtrl.InsertColumn(um_ObjectCallBack_pHandle, _T("句柄"), LVCFMT_LEFT, 150);
			m_CListCtrl.InsertColumn(um_ObjectCallBack_Altitude, _T("Altitude"), LVCFMT_LEFT, 150);
			m_CListCtrl.InsertColumn(um_ObjectCallBack_Moudle, _T("所在模块路径"), LVCFMT_LEFT, 200);
			m_CListCtrl.InsertColumn(um_ObjectCallBack_Firm, _T("文件厂商"), LVCFMT_LEFT, 100);
			m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
		}
		else
		{
			//相等直接返回
			return;
		}

		nPerSel = um_ObjectCallBack_Type;

		OnObjectcallbackRefresh();
	}
	break;
	case um_ObjectCallBackInfo_Type:
	{
		if (nPerSel != um_ObjectCallBackInfo_Type)
		{
			//删除所有Column
			while (m_CListCtrl.DeleteColumn(0)) {}
			//初始化List控件
			m_CListCtrl.InsertColumn(um_ObjectCallBackEx_Type_Type, _T("类型"), LVCFMT_LEFT, 100);
			m_CListCtrl.InsertColumn(um_ObjectCallBackEx_Type_ValidAccessMask, _T("ValidAccessMask"), LVCFMT_LEFT, 100);
			m_CListCtrl.InsertColumn(um_ObjectCallBackEx_Type_FunCallBack, _T("函数地址"), LVCFMT_LEFT, 150);
			m_CListCtrl.InsertColumn(um_ObjectCallBackEx_Type_FunName, _T("函数"), LVCFMT_LEFT, 150);
			m_CListCtrl.InsertColumn(um_ObjectCallBackEx_Type_Pos, _T("位置"), LVCFMT_LEFT, 250);
			m_CListCtrl.InsertColumn(um_ObjectCallBackEx_Type_Object, _T("对象地址"), LVCFMT_LEFT, 150);
			m_CListCtrl.InsertColumn(um_ObjectCallBackEx_Type_ModulePath, _T("所在模块路径"), LVCFMT_LEFT, 150);
			m_CListCtrl.InsertColumn(um_ObjectCallBackEx_Type_Firm, _T("文件厂商"), LVCFMT_LEFT, 200);
			m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
		}
		else
		{
			//相等直接返回
			return;
		}
		
		nPerSel = um_ObjectCallBackInfo_Type;

		OnObjectcallbackRefresh();
	}
	break;
	}
}
