#pragma once
#include "../DefineArea.h"

#define F_MODRM                     0x00000001
#define F_SIB                       0x00000002
#define F_IMM8                      0x00000004
#define F_IMM16                     0x00000008
#define F_IMM32                     0x00000010
#define F_IMM64                     0x00000020
#define F_DISP8                     0x00000040
#define F_DISP16                    0x00000080
#define F_DISP32                    0x00000100
#define F_RELATIVE                  0x00000200
#define F_ERROR                     0x00001000
#define F_ERROR_OPCODE              0x00002000
#define F_ERROR_LENGTH              0x00004000
#define F_ERROR_LOCK                0x00008000
#define F_ERROR_OPERAND             0x00010000
#define F_PREFIX_REPNZ              0x01000000
#define F_PREFIX_REPX               0x02000000
#define F_PREFIX_REP                0x03000000
#define F_PREFIX_66                 0x04000000
#define F_PREFIX_67                 0x08000000
#define F_PREFIX_LOCK               0x10000000
#define F_PREFIX_SEG                0x20000000
#define F_PREFIX_REX                0x40000000
#define F_PREFIX_ANY                0x7f000000

#define PREFIX_SEGMENT_CS           0x2e
#define PREFIX_SEGMENT_SS           0x36
#define PREFIX_SEGMENT_DS           0x3e
#define PREFIX_SEGMENT_ES           0x26
#define PREFIX_SEGMENT_FS           0x64
#define PREFIX_SEGMENT_GS           0x65
#define PREFIX_LOCK                 0xf0
#define PREFIX_REPNZ                0xf2
#define PREFIX_REPX                 0xf3
#define PREFIX_OPERAND_SIZE         0x66
#define PREFIX_ADDRESS_SIZE         0x67


typedef struct _CHde64
{
	UCHAR m_len;
	UCHAR m_p_rep;
	UCHAR m_p_lock;
	UCHAR m_p_seg;
	UCHAR m_p_66;
	UCHAR m_p_67;
	UCHAR m_rex;
	UCHAR m_rex_w;
	UCHAR m_rex_r;
	UCHAR m_rex_x;
	UCHAR m_rex_b;
	UCHAR m_opcode;
	UCHAR m_opcode2;
	UCHAR m_modrm;
	UCHAR m_modrm_mod;
	UCHAR m_modrm_reg;
	UCHAR m_modrm_rm;
	UCHAR m_sib;
	UCHAR m_sib_scale;
	UCHAR m_sib_index;
	UCHAR m_sib_base;
	union
	{
		UCHAR m_imm8;
		USHORT m_imm16;
		ULONG32 m_imm32;
		ULONG64 m_imm64;
	} CImm;
	union
	{
		UCHAR m_disp8;
		USHORT m_disp16;
		USHORT m_disp32;
	} CDisp;
	ULONG32 m_flags;
} CHde64, * PCHde64;


ULONG32 Myhde64_disasm(PVOID64 code, PCHde64 Myhs);