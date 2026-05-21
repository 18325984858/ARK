/*
*   serial.c - 16550 UART serial port output
*
*   uses raw I/O port access (__outbyte / __inbyte) — works from VMX root
*   mode, DPC level, or any context. zero Windows API dependency.
*
*   configured for PCI serial at I/O base 0x4018 (range 0x4018-0x401F)
*/
#include "hv.h"

static __forceinline VOID
serial_wait_tx_ready(VOID)
{
    //
    // spin until THR is empty (LSR bit 5 = THRE)
    // bounded loop to prevent infinite hang if port is absent
    //
    for (UINT32 i = 0; i < 100000; i++)
    {
        if (__inbyte(SERIAL_PORT_BASE + SERIAL_LSR) & LSR_THRE)
            return;
        _mm_pause();
    }
}

/*
*   initialize the 16550 UART at SERIAL_PORT_BASE
*   115200 baud, 8N1, FIFOs enabled
*   safe to call multiple times
*/
VOID
serial_init(VOID)
{
    //
    // disable interrupts
    //
    __outbyte(SERIAL_PORT_BASE + SERIAL_IER, 0x00);

    //
    // set baud rate: 115200
    // divisor = 115200 / 115200 = 1  (clock = 1.8432 MHz assumed)
    //
    __outbyte(SERIAL_PORT_BASE + SERIAL_LCR, LCR_DLAB);    // enable DLAB
    __outbyte(SERIAL_PORT_BASE + SERIAL_DLL, 0x01);         // divisor low = 1
    __outbyte(SERIAL_PORT_BASE + SERIAL_DLH, 0x00);         // divisor high = 0

    //
    // 8 data bits, no parity, 1 stop bit — clear DLAB
    //
    __outbyte(SERIAL_PORT_BASE + SERIAL_LCR, LCR_8N1);

    //
    // enable & clear FIFOs, 14-byte trigger
    //
    __outbyte(SERIAL_PORT_BASE + SERIAL_FCR,
              FCR_ENABLE | FCR_CLEAR_RX | FCR_CLEAR_TX | FCR_TRIGGER_14);

    //
    // DTR + RTS + OUT2
    //
    __outbyte(SERIAL_PORT_BASE + SERIAL_MCR, MCR_DTR | MCR_RTS | MCR_OUT2);

    //
    // verify the port exists by checking scratch register round-trip
    //
    __outbyte(SERIAL_PORT_BASE + SERIAL_SCR, 0xAE);
    UINT8 scratch = __inbyte(SERIAL_PORT_BASE + SERIAL_SCR);
    if (scratch != 0xAE)
    {
        // port not present — serial_putc will still be safe (bounded spin)
        return;
    }

    serial_print("\r\n[hv] Serial port initialized at 0x");
    serial_hex32(SERIAL_PORT_BASE);
    serial_print(" (115200 8N1)\r\n");
}

VOID
serial_putc(CHAR c)
{
    serial_wait_tx_ready();
    __outbyte(SERIAL_PORT_BASE + SERIAL_THR, (UINT8)c);
}

VOID
serial_print(const CHAR * str)
{
    if (!str)
        return;

    while (*str)
    {
        if (*str == '\n')
            serial_putc('\r');
        serial_putc(*str);
        str++;
    }
}

static const CHAR hex_chars[] = "0123456789ABCDEF";

VOID
serial_hex64(UINT64 val)
{
    for (INT32 i = 60; i >= 0; i -= 4)
        serial_putc(hex_chars[(val >> i) & 0xF]);
}

VOID
serial_hex32(UINT32 val)
{
    for (INT32 i = 28; i >= 0; i -= 4)
        serial_putc(hex_chars[(val >> i) & 0xF]);
}

VOID
serial_hex8(UINT8 val)
{
    serial_putc(hex_chars[(val >> 4) & 0xF]);
    serial_putc(hex_chars[val & 0xF]);
}
