
#include "hde64.h"
#include "table64.h"
#include "../DefineArea.h"

ULONG32 Myhde64_disasm (PVOID64 code, PCHde64 Myhs)
{
	UCHAR x = 0;
	UCHAR c = 0;
	PUCHAR p = (PUCHAR)code;
	UCHAR cflags = 0;
	UCHAR m_opcode = 0;
	UCHAR pref = 0;
	PUCHAR ht = hde64_table;
	UCHAR m_mod = 0;
	UCHAR m_reg = 0;
	UCHAR m_rm = 0;
	UCHAR disp_size = 0;
	UCHAR op64 = 0;

	memset(Myhs, 0, sizeof(CHde64));

	for (x = 16; x; x--)
		switch (c = *p++) {
		case 0xf3:
			Myhs->m_p_rep = c;
			pref |= PRE_F3;
			break;
		case 0xf2:
			Myhs->m_p_rep = c;
			pref |= PRE_F2;
			break;
		case 0xf0:
			Myhs->m_p_lock = c;
			pref |= PRE_LOCK;
			break;
		case 0x26: case 0x2e: case 0x36:
		case 0x3e: case 0x64: case 0x65:
			Myhs->m_p_seg = c;
			pref |= PRE_SEG;
			break;
		case 0x66:
			Myhs->m_p_66 = c;
			pref |= PRE_66;
			break;
		case 0x67:
			Myhs->m_p_67 = c;
			pref |= PRE_67;
			break;
		default:
			goto pref_done;
		}
pref_done:

	Myhs->m_flags = (ULONG32)pref << 23;

	if (!pref)
		pref |= PRE_NONE;

	if ((c & 0xf0) == 0x40) {
		Myhs->m_flags |= F_PREFIX_REX;
		Myhs->m_rex_w = (c & 0xf) >> 3;
		if (Myhs->m_rex_w && (*p & 0xf8) == 0xb8)
			op64++;
		Myhs->m_rex_r = (c & 7) >> 2;
		Myhs->m_rex_x = (c & 3) >> 1;
		Myhs->m_rex_b = c & 1;
		if (((c = *p++) & 0xf0) == 0x40) {
			m_opcode = c;
			goto error_opcode;
		}
	}

	if ((Myhs->m_opcode = c) == 0x0f) {
		Myhs->m_opcode2 = c = *p++;
		ht += DELTA_OPCODES;
	}
	else if (c >= 0xa0 && c <= 0xa3) {
		op64++;
		if (pref & PRE_67)
			pref |= PRE_66;
		else
			pref &= ~PRE_66;
	}

	m_opcode = c;
	cflags = ht[ht[m_opcode / 4] + (m_opcode % 4)];

	if (cflags == C_ERROR) {
	error_opcode:
		Myhs->m_flags |= F_ERROR | F_ERROR_OPCODE;
		cflags = 0;
		if ((m_opcode & -3) == 0x24)
			cflags++;
	}

	x = 0;
	if (cflags & C_GROUP) {
		USHORT t;
		t = *(PUSHORT)(ht + (cflags & 0x7f));
		cflags = (UCHAR)t;
		x = (UCHAR)(t >> 8);
	}

	if (Myhs->m_opcode2) {
		ht = hde64_table + DELTA_PREFIXES;
		if (ht[ht[m_opcode / 4] + (m_opcode % 4)] & pref)
			Myhs->m_flags |= F_ERROR | F_ERROR_OPCODE;
	}

	if (cflags & C_MODRM) {
		Myhs->m_flags |= F_MODRM;
		Myhs->m_modrm = c = *p++;
		Myhs->m_modrm_mod = m_mod = c >> 6;
		Myhs->m_modrm_rm = m_rm = c & 7;
		Myhs->m_modrm_reg = m_reg = (c & 0x3f) >> 3;

		if (x && ((x << m_reg) & 0x80))
			Myhs->m_flags |= F_ERROR | F_ERROR_OPCODE;

		if (!Myhs->m_opcode2 && m_opcode >= 0xd9 && m_opcode <= 0xdf) {
			UCHAR t = m_opcode - 0xd9;
			if (m_mod == 3) {
				ht = hde64_table + DELTA_FPU_MODRM + t * 8;
				t = ht[m_reg] << m_rm;
			}
			else {
				ht = hde64_table + DELTA_FPU_REG;
				t = ht[t] << m_reg;
			}
			if (t & 0x80)
				Myhs->m_flags |= F_ERROR | F_ERROR_OPCODE;
		}

		if (pref & PRE_LOCK) {
			if (m_mod == 3) {
				Myhs->m_flags |= F_ERROR | F_ERROR_LOCK;
			}
			else {
				PUCHAR table_end;
				UCHAR op = m_opcode;
				if (Myhs->m_opcode2) {
					ht = hde64_table + DELTA_OP2_LOCK_OK;
					table_end = ht + DELTA_OP_ONLY_MEM - DELTA_OP2_LOCK_OK;
				}
				else {
					ht = hde64_table + DELTA_OP_LOCK_OK;
					table_end = ht + DELTA_OP2_LOCK_OK - DELTA_OP_LOCK_OK;
					op &= -2;
				}
				for (; ht != table_end; ht++)
					if (*ht++ == op) {
						if (!((*ht << m_reg) & 0x80))
							goto no_lock_error;
						else
							break;
					}
				Myhs->m_flags |= F_ERROR | F_ERROR_LOCK;
			no_lock_error:
				;
			}
		}

		if (Myhs->m_opcode2) {
			switch (m_opcode) {
			case 0x20: case 0x22:
				m_mod = 3;
				if (m_reg > 4 || m_reg == 1)
					goto error_operand;
				else
					goto no_error_operand;
			case 0x21: case 0x23:
				m_mod = 3;
				if (m_reg == 4 || m_reg == 5)
					goto error_operand;
				else
					goto no_error_operand;
			}
		}
		else {
			switch (m_opcode) {
			case 0x8c:
				if (m_reg > 5)
					goto error_operand;
				else
					goto no_error_operand;
			case 0x8e:
				if (m_reg == 1 || m_reg > 5)
					goto error_operand;
				else
					goto no_error_operand;
			}
		}

		if (m_mod == 3) {
			PUCHAR table_end;
			if (Myhs->m_opcode2) {
				ht = hde64_table + DELTA_OP2_ONLY_MEM;
				table_end = ht + sizeof(hde64_table) - DELTA_OP2_ONLY_MEM;
			}
			else {
				ht = hde64_table + DELTA_OP_ONLY_MEM;
				table_end = ht + DELTA_OP2_ONLY_MEM - DELTA_OP_ONLY_MEM;
			}
			for (; ht != table_end; ht += 2)
				if (*ht++ == m_opcode) {
					if (*ht++ & pref && !((*ht << m_reg) & 0x80))
						goto error_operand;
					else
						break;
				}
			goto no_error_operand;
		}
		else if (Myhs->m_opcode2) {
			switch (m_opcode) {
			case 0x50: case 0xd7: case 0xf7:
				if (pref & (PRE_NONE | PRE_66))
					goto error_operand;
				break;
			case 0xd6:
				if (pref & (PRE_F2 | PRE_F3))
					goto error_operand;
				break;
			case 0xc5:
				goto error_operand;
			}
			goto no_error_operand;
		}
		else
			goto no_error_operand;

	error_operand:
		Myhs->m_flags |= F_ERROR | F_ERROR_OPERAND;
	no_error_operand:

		c = *p++;
		if (m_reg <= 1) {
			if (m_opcode == 0xf6)
				cflags |= C_IMM8;
			else if (m_opcode == 0xf7)
				cflags |= C_IMM_P66;
		}

		switch (m_mod) {
		case 0:
			if (pref & PRE_67) {
				if (m_rm == 6)
					disp_size = 2;
			}
			else
				if (m_rm == 5)
					disp_size = 4;
			break;
		case 1:
			disp_size = 1;
			break;
		case 2:
			disp_size = 2;
			if (!(pref & PRE_67))
				disp_size <<= 1;
		}

		if (m_mod != 3 && m_rm == 4) {
			Myhs->m_flags |= F_SIB;
			p++;
			Myhs->m_sib = c;
			Myhs->m_sib_scale = c >> 6;
			Myhs->m_sib_index = (c & 0x3f) >> 3;
			if ((Myhs->m_sib_base = c & 7) == 5 && !(m_mod & 1))
				disp_size = 4;
		}

		p--;
		switch (disp_size) {
		case 1:
			Myhs->m_flags |= F_DISP8;
			Myhs->CDisp.m_disp8 = *p;
			break;
		case 2:
			Myhs->m_flags |= F_DISP16;
			Myhs->CDisp.m_disp16 = *(PUSHORT)p;
			break;
		case 4:
			Myhs->m_flags |= F_DISP32;
			Myhs->CDisp.m_disp32 = *(PULONG32)p;
		}
		p += disp_size;
	}
	else if (pref & PRE_LOCK)
		Myhs->m_flags |= F_ERROR | F_ERROR_LOCK;

	if (cflags & C_IMM_P66) {
		if (cflags & C_REL32) {
			if (pref & PRE_66) {
				Myhs->m_flags |= F_IMM16 | F_RELATIVE;
				Myhs->CImm.m_imm16 = *(PUSHORT)p;
				p += 2;
				goto disasm_done;
			}
			goto rel32_ok;
		}
		if (op64) {
			Myhs->m_flags |= F_IMM64;
			Myhs->CImm.m_imm64 = *(PULONG64)p;
			p += 8;
		}
		else if (!(pref & PRE_66)) {
			Myhs->m_flags |= F_IMM32;
			Myhs->CImm.m_imm32 = *(PULONG32)p;
			p += 4;
		}
		else
			goto imm16_ok;
	}

	if (cflags & C_IMM16) {
	imm16_ok:
		Myhs->m_flags |= F_IMM16;
		Myhs->CImm.m_imm16 = *(PUSHORT)p;
		p += 2;
	}
	if (cflags & C_IMM8) {
		Myhs->m_flags |= F_IMM8;
		Myhs->CImm.m_imm8 = *p++;
	}

	if (cflags & C_REL32) {
	rel32_ok:
		Myhs->m_flags |= F_IMM32 | F_RELATIVE;
		Myhs->CImm.m_imm32 = *(PULONG32)p;
		p += 4;
	}
	else if (cflags & C_REL8) {
		Myhs->m_flags |= F_IMM8 | F_RELATIVE;
		Myhs->CImm.m_imm8 = *p++;
	}

disasm_done:

	if ((Myhs->m_len = (UCHAR)(p - (PUCHAR)code)) > 15) {
		Myhs->m_flags |= F_ERROR | F_ERROR_LENGTH;
		Myhs->m_len = 15;
	}

	return (ULONG32)Myhs->m_len;
}
