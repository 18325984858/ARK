// DlgEditRegValue.cpp
#define _CRT_NON_CONFORMING_SWPRINTFS
#include "pch.h"
#include "DlgEditRegValue.h"
#include <stdint.h>

// 局部控件 ID
#define IDC_REG_NAME   1001
#define IDC_REG_DATA   1002
#define IDC_REG_HEX    1003
#define IDC_REG_DEC    1004

BEGIN_MESSAGE_MAP(CDlgEditRegValue, CDialog)
	ON_BN_CLICKED(IDC_REG_HEX, &CDlgEditRegValue::OnRadioHex)
	ON_BN_CLICKED(IDC_REG_DEC, &CDlgEditRegValue::OnRadioDec)
END_MESSAGE_MAP()

CDlgEditRegValue::CDlgEditRegValue(ULONG64 type, LPCWSTR name, const void* data, size_t dataBytes)
	: m_Type(type), m_Name(name ? name : L""), m_Hex(true)
{
	m_Data.assign(
		(const BYTE*)data,
		(const BYTE*)data + dataBytes);
}

namespace
{
	void PushBytes(std::vector<BYTE>& v, const void* p, size_t n)
	{
		const BYTE* b = (const BYTE*)p;
		v.insert(v.end(), b, b + n);
	}
	void PushW(std::vector<BYTE>& v, WORD w) { PushBytes(v, &w, 2); }
	void PushDW(std::vector<BYTE>& v, DWORD d) { PushBytes(v, &d, 4); }
	void PushWStr(std::vector<BYTE>& v, LPCWSTR s)
	{
		if (!s) { PushW(v, 0); return; }
		while (*s) { PushW(v, (WORD)*s); ++s; }
		PushW(v, 0);
	}
	void AlignDW(std::vector<BYTE>& v) { while (v.size() & 3) v.push_back(0); }

	// 标准控件 atom
	const WORD kAtomButton = 0x0080;
	const WORD kAtomEdit   = 0x0081;
	const WORD kAtomStatic = 0x0082;

	void AddItem(std::vector<BYTE>& v,
		DWORD style, DWORD exStyle,
		short x, short y, short cx, short cy,
		WORD id, WORD atom, LPCWSTR text)
	{
		AlignDW(v);
		// DLGITEMTEMPLATE
		PushDW(v, style);
		PushDW(v, exStyle);
		PushW(v, x); PushW(v, y); PushW(v, cx); PushW(v, cy);
		PushW(v, id);
		// 类（atom）
		PushW(v, 0xFFFF);
		PushW(v, atom);
		// 标题
		PushWStr(v, text);
		// 创建数据（无）
		PushW(v, 0);
	}
}

void CDlgEditRegValue::BuildTemplate(LPCWSTR title)
{
	m_TplBuf.clear();

	const bool isStr     = (m_Type == REG_SZ || m_Type == REG_EXPAND_SZ || m_Type == REG_LINK);
	const bool isMulti   = (m_Type == REG_MULTI_SZ);
	const bool isNum     = (m_Type == REG_DWORD || m_Type == REG_DWORD_BIG_ENDIAN || m_Type == REG_QWORD);
	const bool isBinary  = !isStr && !isMulti && !isNum;
	const bool multiline = isMulti || isBinary;

	// 整体尺寸：数值类小，字符串中等，多行/二进制大
	const short cxAll = 320;
	const short cyAll = isNum ? 130 : 195;

	// DLGTEMPLATE
	DWORD style = DS_SETFONT | DS_MODALFRAME | DS_FIXEDSYS | DS_CENTER
		| WS_POPUP | WS_CAPTION | WS_SYSMENU;
	PushDW(m_TplBuf, style);
	PushDW(m_TplBuf, 0);                    // ex style
	PushW(m_TplBuf, 0);                     // cdit, 占位，最后回填
	PushW(m_TplBuf, 0);                     // x
	PushW(m_TplBuf, 0);                     // y
	PushW(m_TplBuf, cxAll);
	PushW(m_TplBuf, cyAll);
	PushW(m_TplBuf, 0);                     // menu
	PushW(m_TplBuf, 0);                     // class
	PushWStr(m_TplBuf, title);              // title
	// DS_SETFONT
	PushW(m_TplBuf, 9);                     // point size
	PushWStr(m_TplBuf, L"MS Shell Dlg");

	WORD cdit = 0;

	// 数值名称: label
	AddItem(m_TplBuf, WS_CHILD | WS_VISIBLE | SS_LEFT, 0,
		7, 7, 80, 9, (WORD)-1, kAtomStatic, L"数值名称(N):");
	++cdit;
	// 名称 edit（只读）
	AddItem(m_TplBuf,
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
		0,
		7, 18, cxAll - 14, 12,
		IDC_REG_NAME, kAtomEdit, L"");
	++cdit;

	// 数值数据: label
	AddItem(m_TplBuf, WS_CHILD | WS_VISIBLE | SS_LEFT, 0,
		7, 36, 80, 9, (WORD)-1, kAtomStatic, L"数值数据(V):");
	++cdit;

	// 数据 edit
	short dataCy = multiline ? (short)(cyAll - 76) : 12;
	short dataCx = cxAll - 14;
	short dataX = 7;
	if (isNum)
	{
		// 数值类把右侧留给进制单选区域，避免与数据编辑框重叠。
		dataCx = cxAll - 14 - 92;
	}
	DWORD eStyle = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL;
	if (multiline)
		eStyle |= ES_MULTILINE | ES_WANTRETURN | WS_VSCROLL | ES_AUTOVSCROLL;
	AddItem(m_TplBuf, eStyle, 0,
		dataX, 47, dataCx, dataCy,
		IDC_REG_DATA, kAtomEdit, L"");
	++cdit;

	short btnY = cyAll - 22;

	// 数值进制（仅数字类）
	if (isNum)
	{
		// 右侧进制区域
		AddItem(m_TplBuf,
			WS_CHILD | WS_VISIBLE | SS_LEFT,
			0,
			cxAll - 94, 49, 20, 9,
			(WORD)-1, kAtomStatic, L"进制");
		++cdit;
		AddItem(m_TplBuf,
			WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_TABSTOP | WS_GROUP,
			0,
			cxAll - 70, 47, 62, 10,
			IDC_REG_HEX, kAtomButton, L"十六进制(H)");
		++cdit;
		AddItem(m_TplBuf,
			WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_TABSTOP,
			0,
			cxAll - 70, 60, 62, 10,
			IDC_REG_DEC, kAtomButton, L"十进制(D)");
		++cdit;
	}

	// OK / Cancel
	AddItem(m_TplBuf,
		WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP | WS_GROUP,
		0,
		cxAll - 110, btnY, 50, 14,
		IDOK, kAtomButton, L"确定");
	++cdit;
	AddItem(m_TplBuf,
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
		0,
		cxAll - 55, btnY, 50, 14,
		IDCANCEL, kAtomButton, L"取消");
	++cdit;

	// 回填 cdit（位于头部偏移 8 处）
	*(WORD*)(m_TplBuf.data() + 8) = cdit;
}

void CDlgEditRegValue::FormatNumeric(CString& out) const
{
	if (m_Type == REG_QWORD)
	{
		ULONG64 v = 0;
		memcpy(&v, m_Data.data(), m_Data.size() < 8 ? m_Data.size() : 8);
		if (m_Hex) out.Format(L"%016I64X", v);
		else       out.Format(L"%I64u", v);
	}
	else
	{
		ULONG32 v = 0;
		memcpy(&v, m_Data.data(), m_Data.size() < 4 ? m_Data.size() : 4);
		if (m_Type == REG_DWORD_BIG_ENDIAN) v = _byteswap_ulong(v);
		if (m_Hex) out.Format(L"%08X", v);
		else       out.Format(L"%u", v);
	}
}

CString CDlgEditRegValue::FormatInitial() const
{
	CString s;
	switch (m_Type)
	{
	case REG_SZ:
	case REG_EXPAND_SZ:
	case REG_LINK:
		// 数据按 WCHAR 字符串解释
		if (!m_Data.empty())
			s = (LPCWSTR)m_Data.data();
		break;
	case REG_MULTI_SZ:
	{
		const WCHAR* p = (const WCHAR*)m_Data.data();
		const WCHAR* end = p + m_Data.size() / sizeof(WCHAR);
		while (p < end && *p)
		{
			if (!s.IsEmpty()) s += L"\r\n";
			s += p;
			p += wcslen(p) + 1;
		}
		break;
	}
	case REG_DWORD:
	case REG_DWORD_BIG_ENDIAN:
	case REG_QWORD:
		FormatNumeric(s);
		break;
	default:
	{
		// 二进制：尾部 0 截断 + 每行 16 字节 "AA BB ..."
		size_t n = m_Data.size();
		while (n > 0 && m_Data[n - 1] == 0) --n;
		WCHAR tmp[8];
		for (size_t i = 0; i < n; ++i)
		{
			if (i && (i % 16) == 0) s += L"\r\n";
			else if (i) s += L" ";
			swprintf(tmp, L"%02X", m_Data[i]);
			s += tmp;
		}
		break;
	}
	}
	return s;
}

INT_PTR CDlgEditRegValue::ShowModal(CWnd* parent)
{
	CString title;
	switch (m_Type)
	{
	case REG_SZ:               title = L"编辑字符串"; break;
	case REG_EXPAND_SZ:        title = L"编辑可扩展字符串"; break;
	case REG_MULTI_SZ:         title = L"编辑多字符串"; break;
	case REG_DWORD:            title = L"编辑 DWORD (32 位) 值"; break;
	case REG_DWORD_BIG_ENDIAN: title = L"编辑 DWORD_BE (32 位) 值"; break;
	case REG_QWORD:            title = L"编辑 QWORD (64 位) 值"; break;
	case REG_BINARY:           title = L"编辑二进制值"; break;
	case REG_LINK:             title = L"编辑链接值"; break;
	case REG_NONE:             title = L"编辑 NONE 值"; break;
	default:                   title = L"编辑值"; break;
	}

	BuildTemplate(title);
	InitModalIndirect((LPCDLGTEMPLATE)m_TplBuf.data(), parent);
	return DoModal();
}

BOOL CDlgEditRegValue::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetDlgItemTextW(IDC_REG_NAME, m_Name.IsEmpty() ? L"(默认)" : (LPCWSTR)m_Name);
	SetDlgItemTextW(IDC_REG_DATA, FormatInitial());

	if (m_Type == REG_DWORD || m_Type == REG_DWORD_BIG_ENDIAN || m_Type == REG_QWORD)
	{
		CheckRadioButton(IDC_REG_HEX, IDC_REG_DEC, m_Hex ? IDC_REG_HEX : IDC_REG_DEC);
	}

	CWnd* pData = GetDlgItem(IDC_REG_DATA);
	if (pData) pData->SetFocus();
	return FALSE; // 我们自己设置了焦点
}

void CDlgEditRegValue::OnRadioHex()
{
	if (m_Hex) return;
	m_Hex = true;
	CString s; FormatNumeric(s);
	SetDlgItemTextW(IDC_REG_DATA, s);
}

void CDlgEditRegValue::OnRadioDec()
{
	if (!m_Hex) return;
	m_Hex = false;
	CString s; FormatNumeric(s);
	SetDlgItemTextW(IDC_REG_DATA, s);
}
