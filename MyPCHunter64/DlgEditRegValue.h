#pragma once
#include <afxwin.h>
#include <vector>

// 注册表值编辑对话框（无 .rc 资源，运行时构建 DLGTEMPLATE）。
// 为了简单：OK 只是关闭——驱动当前没有写回注册表的 IPC，所以只做查看/编辑预览。
class CDlgEditRegValue : public CDialog
{
public:
	// type: REG_SZ / REG_EXPAND_SZ / REG_MULTI_SZ / REG_DWORD / REG_QWORD / REG_BINARY / ...
	// data + dataBytes：值的原始字节
	CDlgEditRegValue(ULONG64 type, LPCWSTR name, const void* data, size_t dataBytes);

	// 自己构建模板并 DoModal
	INT_PTR ShowModal(CWnd* parent);

protected:
	virtual BOOL OnInitDialog();
	afx_msg void OnRadioHex();
	afx_msg void OnRadioDec();
	DECLARE_MESSAGE_MAP()

private:
	void  BuildTemplate(LPCWSTR title);
	void  FormatNumeric(CString& out) const;
	CString FormatInitial() const;

	ULONG64           m_Type;
	CString           m_Name;
	std::vector<BYTE> m_Data;     // 原始字节
	bool              m_Hex;      // DWORD/QWORD 当前进制
	std::vector<BYTE> m_TplBuf;   // DLGTEMPLATE 二进制（要在 DoModal 期间保活）
};
