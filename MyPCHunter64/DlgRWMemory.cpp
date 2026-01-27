// DlgRWMemory.cpp: 实现文件
//
#define _CRT_NON_CONFORMING_SWPRINTFS
#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgRWMemory.h"
#include "Thread.h"
#include "MyPCHunter64Dlg.h"

// DlgRWMemory 对话框

IMPLEMENT_DYNAMIC(DlgRWMemory, CDialogEx)

DlgRWMemory::DlgRWMemory(ULONG64 Eprocess, ULONG64 Addr, UCHAR IsX64, CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_RW_MEMORY, pParent)
{
	m_DstAddr = Addr;
	m_DstEprocess = Eprocess;
	m_IsX64 = IsX64;
}

DlgRWMemory::~DlgRWMemory()
{
}

void DlgRWMemory::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_RW_MEMORY_EDIT, m_CEdit);
	DDX_Control(pDX, ID_RW_MEMORY_ADDR_EDIT, m_AddrEdit);
	DDX_Control(pDX, IDC_LIST_ASM, m_CListCtrl);
	DDX_Control(pDX, IDC_LIST_OPCODE, m_CListCtrlOpcode);
	DDX_Control(pDX, ID_RW_MEMORY_SAVE, m_CButton_Save);
}


BEGIN_MESSAGE_MAP(DlgRWMemory, CDialogEx)
	ON_BN_CLICKED(ID_RW_MEMORY_BUTTON, &DlgRWMemory::OnBnClickedRwMemoryButton)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_OPCODE, &DlgRWMemory::OnNMDblclkListOpcode)
	ON_BN_CLICKED(ID_RW_MEMORY_SAVE, &DlgRWMemory::OnBnClickedRwMemorySave)
END_MESSAGE_MAP()


// DlgRWMemory 消息处理程序


void DlgRWMemory::OnBnClickedRwMemoryButton()
{
	//删除全部反汇编的数据
	m_CListCtrl.DeleteAllItems();
	m_CListCtrlOpcode.DeleteAllItems();

	//清空Edit
	m_CEdit.Clear();

	CString AddrStr;
	m_AddrEdit.GetWindowText(AddrStr);
	m_DstAddr = (ULONG64)_wcstoui64(AddrStr.GetBuffer(), 0, 16);
	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserRWMemOryInfo, this });
}


BOOL DlgRWMemory::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	//当地址不为用户地址时返回,不初始化
	if (m_DstAddr < 0 || m_DstAddr>(ULONG64)0x7FFFFFFF0000)
	{
		return TRUE;
	}

	CString szBuf;
	szBuf.Format(L"%I64X", m_DstAddr);
	m_AddrEdit.SetWindowText(szBuf);

	m_CListCtrl.InsertColumn(um_RWMemory_Addr, _T("地址"), LVCFMT_LEFT, 110);
	m_CListCtrl.InsertColumn(um_RWMemory_Opcode, _T("字节码"), LVCFMT_LEFT, 150);
	m_CListCtrl.InsertColumn(um_RWMemory_Asm, _T("汇编"), LVCFMT_LEFT, 300);
	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);


	m_CListCtrlOpcode.InsertColumn(um_RWMemory_Opcode_Addr, _T("地址"), LVCFMT_LEFT, 110);
	m_CListCtrlOpcode.InsertColumn(um_RWMemory_Opcode_1, _T("1"), LVCFMT_LEFT, 30);
	m_CListCtrlOpcode.InsertColumn(um_RWMemory_Opcode_2, _T("2"), LVCFMT_LEFT, 30);
	m_CListCtrlOpcode.InsertColumn(um_RWMemory_Opcode_3, _T("3"), LVCFMT_LEFT, 30);
	m_CListCtrlOpcode.InsertColumn(um_RWMemory_Opcode_4, _T("4"), LVCFMT_LEFT, 30);
	m_CListCtrlOpcode.InsertColumn(um_RWMemory_Opcode_5, _T("5"), LVCFMT_LEFT, 30);
	m_CListCtrlOpcode.InsertColumn(um_RWMemory_Opcode_6, _T("6"), LVCFMT_LEFT, 30);
	m_CListCtrlOpcode.InsertColumn(um_RWMemory_Opcode_7, _T("7"), LVCFMT_LEFT, 30);
	m_CListCtrlOpcode.InsertColumn(um_RWMemory_Opcode_8, _T("8"), LVCFMT_LEFT, 30);
	m_CListCtrlOpcode.InsertColumn(um_RWMemory_Opcode_9, _T("9"), LVCFMT_LEFT, 30);
	m_CListCtrlOpcode.InsertColumn(um_RWMemory_Opcode_10, _T("10"), LVCFMT_LEFT, 30);
	m_CListCtrlOpcode.InsertColumn(um_RWMemory_Opcode_11, _T("11"), LVCFMT_LEFT, 30);
	m_CListCtrlOpcode.InsertColumn(um_RWMemory_Opcode_12, _T("12"), LVCFMT_LEFT, 30);
	m_CListCtrlOpcode.InsertColumn(um_RWMemory_Opcode_13, _T("13"), LVCFMT_LEFT, 30);
	m_CListCtrlOpcode.InsertColumn(um_RWMemory_Opcode_14, _T("14"), LVCFMT_LEFT, 30);
	m_CListCtrlOpcode.InsertColumn(um_RWMemory_Opcode_15, _T("15"), LVCFMT_LEFT, 30);
	m_CListCtrlOpcode.InsertColumn(um_RWMemory_Opcode_16, _T("16"), LVCFMT_LEFT, 30);
	m_CListCtrlOpcode.SetExtendedStyle(m_CListCtrlOpcode.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	OnBnClickedRwMemoryButton();

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void DlgRWMemory::ReadWriteMemOry()
{
	//读写内存

	//__debugbreak();
	//当地址不为用户地址时返回,目标进程对象为空
	if (m_DstAddr < 0 || m_DstAddr>(ULONG64)0x7FFFFFFF0000 || m_DstEprocess == NULL)
	{
		return;
	}


	if (m_MemInfo)
	{
		if (m_MemInfo->Buf)
		{
			VirtualFree(m_MemInfo->Buf, 0, MEM_RELEASE);
		}
		VirtualFree(m_MemInfo, 0, MEM_RELEASE);

		m_MemInfo = NULL;
	}

	SIZE_T nSize = sizeof(CRWMemoryInfo);
	//发送数据包
	PCRWMemoryInfo MemInfo = (PCRWMemoryInfo)VirtualAlloc(NULL, nSize, MEM_COMMIT, PAGE_READWRITE);
	if (MemInfo != NULL)
	{
		RtlZeroMemory(MemInfo, nSize);

		MemInfo->DstAddr = m_DstAddr & 0xFFFFFFFFFFFFF000;
		MemInfo->Eprocess = m_DstEprocess;
		MemInfo->Mode = RWMEMORY_READ;
		MemInfo->dqSize = MAX_READ_SIZE;

		MemInfo->offset = m_DstAddr & 0xFFF;				//偏移

		if (MemInfo->offset)
		{
			MemInfo->dqSize = MAX_READ_SIZE * 2;
		}

		MemInfo->Buf = (PUCHAR)VirtualAlloc(NULL, MemInfo->dqSize, MEM_COMMIT, PAGE_READWRITE);

		if (MemInfo->Buf)
		{

			//发送消息
			//__debugbreak();
			ULONG64 nStatus = g_LoadDriver.SendMsg(um_Cmd_RWProcessMemOry_info, MemInfo);
			if (nStatus > 0)
			{
				ULONG64 Addr1 = MemInfo->DstAddr + MemInfo->offset;	//定位到地址
				PUCHAR Buf1 = &MemInfo->Buf[MemInfo->offset > 0 ? MemInfo->offset : 0];	//定位到缓冲区
				size_t code_size = nStatus < MAX_READ_SIZE ? nStatus : MAX_READ_SIZE;

				//反汇编代码到List控件中
				{
					csh handle;
					cs_insn* insn;
					size_t count;

					// 初始化Capstone引擎 (x86-64架构)
					if (cs_open(CS_ARCH_X86, m_IsX64 ? CS_MODE_64 : CS_MODE_32, &handle) == CS_ERR_OK)
					{
						// 启用详细模式
						cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);
						// 反汇编代码
						size_t offset = 0;
						int i = 0;

						// 插入前关闭重绘
						m_CListCtrl.SetRedraw(FALSE);
						while (offset < code_size)
						{
							cs_insn* insn;
							CString StrBug;
							CString StrBug1;
							CString StrBug2;
							WCHAR szBuf[MAX_PATH] = { 0 };
							WCHAR szBuf1[MAX_PATH] = { 0 };


							size_t count = cs_disasm(handle, Buf1 + offset, code_size - offset, (uint64_t)Addr1 + offset, 1, &insn);

							StrBug1.Format(TEXT("%p"), (uint64_t)Addr1 + offset);

							if (count > 0)
							{

								AsciitoUniCodeString(insn->mnemonic, szBuf);
								AsciitoUniCodeString(insn->op_str, szBuf1);
								StrBug.Format(TEXT("%-10ws %ws"), szBuf, szBuf1);


								for (size_t j = 0; j < insn->size; j++) {
									CString tmp;
									tmp.Format(TEXT("%02X "), insn->bytes[j]);
									StrBug2 += tmp;
								}

								// 处理指令
								offset += insn->size;
								cs_free(insn, count);
							}
							else
							{
								StrBug.Format(TEXT("???"));
								StrBug2.Format(TEXT("%02X"), ((PUCHAR)Buf1)[offset]);
								// 跳过当前字节，继续分析
								offset += 1;
							}

							m_CListCtrl.InsertItem(i, StrBug1);
							m_CListCtrl.SetItemText(i, um_RWMemory_Opcode, StrBug2);
							m_CListCtrl.SetItemText(i, um_RWMemory_Asm, StrBug);
							i++;
						}

						// 插入后开启重绘并刷新
						m_CListCtrl.SetRedraw(TRUE);
						m_CListCtrl.Invalidate();
						m_CListCtrl.UpdateWindow();

						cs_close(&handle);
					}
				}

				m_CListCtrlOpcode.SetRedraw(FALSE);


				int j = 0;
				//一行多少个
				for (int i = 0; i < code_size; )
				{
					CString AddrStr;

					AddrStr.Format(L"%p", Addr1 + i);
					//插入地址
					m_CListCtrlOpcode.InsertItem(j, AddrStr);

					int CurNumber = ((code_size - i) - COL_NUM);					/*计算还剩多少个*/
					int nCol = CurNumber < 0 ? COL_NUM - CurNumber : COL_NUM;	/*判断是否越界*/

					for (int Coli = 0; Coli < nCol; Coli++)
					{
						CString StrBug2Opcode;
						StrBug2Opcode.Format(L"%02X", Buf1[i]);
						m_CListCtrlOpcode.SetItemText(j, um_RWMemory_Opcode_1 + Coli, StrBug2Opcode);
						i++;
					}
					j++;
				}
				m_CListCtrlOpcode.SetRedraw(TRUE);
				m_CListCtrlOpcode.Invalidate();
				m_CListCtrlOpcode.UpdateWindow();
			}

			m_MemInfo = MemInfo;
		}
		else
		{
			if (m_MemInfo)
			{
				VirtualFree(m_MemInfo, 0, MEM_RELEASE);
			}
		}
	}
}

void DlgRWMemory::OnNMDblclkListOpcode(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	int nItem = pNMItemActivate->iItem;     // 行索引
	int nSubItem = pNMItemActivate->iSubItem; // 列索引

	if (nItem != -1 && nSubItem > 0) {
		m_EditItem = nItem;
		m_EditSubItem = nSubItem;

		CRect rect;
		// 获取单元格矩形（用 m_CListCtrlOpcode）
		if (m_CListCtrlOpcode.GetSubItemRect(nItem, nSubItem, LVIR_BOUNDS, rect))
		{
			// 转换为对话框客户区坐标
			m_CListCtrlOpcode.ClientToScreen(&rect);
			ScreenToClient(&rect);

			// 获取当前单元格文本
			CString cellText = m_CListCtrlOpcode.GetItemText(nItem, nSubItem);

			// 如果已存在编辑框，先销毁
			if (m_CEdit1.GetSafeHwnd())
			{
				m_CEdit1.DestroyWindow();
			}

			// 创建编辑框，覆盖原单元格
			m_CEdit1.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, rect, this, 0x1234);
			m_CEdit1.SetWindowText(cellText);
			m_CEdit1.SetFocus();
			m_CEdit1.SetFont(m_CListCtrlOpcode.GetFont());

			SetWindowPos(&m_CEdit1, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}
	}
	*pResult = 0;
}

void DlgRWMemory::OnOK()
{
	// TODO: 在此添加专用代码和/或调用基类

	//判断如果编辑框存在，则修改List控件数据
	if (m_CEdit1.GetSafeHwnd())
	{
		if (m_EditItem != -1 && m_EditSubItem > 0)
		{

			CString EditBuf;
			m_CEdit1.GetWindowText(EditBuf);

			//检测是否是有效的16进制数据
			ULONG64 IsSuccess = (ULONG64)_wcstoui64(EditBuf.GetBuffer(), 0, 16);
			if (IsSuccess >= 0 && IsSuccess < 0x100)
			{
				PUCHAR buf = (PUCHAR)&m_MemInfo->Buf[m_MemInfo->offset];

				buf[((m_EditItem * COL_NUM) + m_EditSubItem) - 1] = (UCHAR)IsSuccess;
				m_CListCtrlOpcode.SetItemText(m_EditItem, m_EditSubItem, EditBuf);


				m_CButton_Save.EnableWindow(TRUE);
			}
			else
			{
				AfxMessageBox(L"请输入有效的16进制数据,范围0~FF!");
			}
		}
		//摧毁编辑框
		m_CEdit1.DestroyWindow();
	}
	//__super::OnOK();
}

BOOL DlgRWMemory::DestroyWindow()
{
	// TODO: 在此添加专用代码和/或调用基类

	if (m_CEdit1.GetSafeHwnd())
	{
		m_CEdit1.DestroyWindow();
	}

	if (m_MemInfo != NULL)
	{
		if (m_MemInfo->Buf)
		{
			VirtualFree(m_MemInfo->Buf, 0, MEM_RELEASE);
		}
		VirtualFree(m_MemInfo, 0, MEM_RELEASE);
	}

	return __super::DestroyWindow();
}

void DlgRWMemory::OnBnClickedRwMemorySave()
{
	// TODO: 在此添加控件通知处理程序代码

	UCHAR OldMode = m_MemInfo->Mode;

	//修改包的模式为写入
	m_MemInfo->Mode = RWMEMORY_WRITE;
	ULONG64 nStatus = g_LoadDriver.SendMsg(um_Cmd_RWProcessMemOry_info, m_MemInfo);
	m_MemInfo->Mode = OldMode;


	if (nStatus)
	{
		//重新读取内存
		OnBnClickedRwMemoryButton();

		//重置状态
		m_CButton_Save.EnableWindow(FALSE);

	}
	else

	{
		CString Buf;
		Buf.Format(TEXT("写入内存失败！写入地址:%I64X 写入大小:%I64X"), m_MemInfo->DstAddr, MAX_READ_SIZE);
		AfxMessageBox(L"写入内存失败！");
	}
}
