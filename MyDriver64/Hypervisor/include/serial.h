/*
*   serial.h - 16550 UART serial port output for VMX root mode debugging
*   works from ANY context (PASSIVE, DPC, VMX root) — zero OS dependency
*/
#pragma once

#include <ntddk.h>

//
// PCI serial port base I/O address (8 registers: base+0 .. base+7)
//
#define SERIAL_PORT_BASE    0x4018

//
// 16550 UART register offsets
//
#define SERIAL_THR          0   // Transmit Holding Register (write)
#define SERIAL_RBR          0   // Receive Buffer Register (read)
#define SERIAL_IER          1   // Interrupt Enable Register
#define SERIAL_FCR          2   // FIFO Control Register (write)
#define SERIAL_IIR          2   // Interrupt Identification (read)
#define SERIAL_LCR          3   // Line Control Register
#define SERIAL_MCR          4   // Modem Control Register
#define SERIAL_LSR          5   // Line Status Register
#define SERIAL_MSR          6   // Modem Status Register
#define SERIAL_SCR          7   // Scratch Register

//
// Divisor Latch registers (when LCR.DLAB=1)
//
#define SERIAL_DLL          0   // Divisor Latch Low
#define SERIAL_DLH          1   // Divisor Latch High

//
// LSR bits
//
#define LSR_THRE            0x20    // Transmit Holding Register Empty
#define LSR_TEMT            0x40    // Transmitter Empty

//
// LCR bits
//
#define LCR_8N1             0x03    // 8 data bits, no parity, 1 stop bit
#define LCR_DLAB            0x80    // Divisor Latch Access Bit

//
// FCR bits
//
#define FCR_ENABLE          0x01    // Enable FIFOs
#define FCR_CLEAR_RX        0x02    // Clear receive FIFO
#define FCR_CLEAR_TX        0x04    // Clear transmit FIFO
#define FCR_TRIGGER_14      0xC0    // 14-byte trigger level

//
// MCR bits
//
#define MCR_DTR             0x01
#define MCR_RTS             0x02
#define MCR_OUT2            0x08    // required for interrupts (we don't use them)

//
// master switch
//
#define SERIAL_LOGGING_ENABLED  1

VOID serial_init(VOID);
VOID serial_putc(CHAR c);
VOID serial_print(const CHAR * str);
VOID serial_hex64(UINT64 val);
VOID serial_hex32(UINT32 val);
VOID serial_hex8(UINT8 val);

//
// convenience macro: serial_log("prefix", value)
// prints "[hv] prefix: 0xVALUE\n"
//
#if SERIAL_LOGGING_ENABLED
#define SERIAL_LOG(prefix, val) \
    do { serial_print("[hv] " prefix ": 0x"); serial_hex64((UINT64)(val)); serial_print("\r\n"); } while(0)
#define SERIAL_MSG(msg) \
    do { serial_print("[hv] " msg "\r\n"); } while(0)
#else
#define SERIAL_LOG(prefix, val) ((void)0)
#define SERIAL_MSG(msg) ((void)0)
#endif
