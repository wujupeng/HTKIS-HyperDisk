#include "hd_serial.h"

static VOID HdSerialWriteByte(PUCHAR ComBase, UCHAR Byte)
{
    LARGE_INTEGER delay;
    ULONG timeout = 100000;

    while (!(READ_PORT_UCHAR(ComBase + 5) & 0x20) && timeout > 0) {
        delay.QuadPart = -10;
        KeDelayExecutionThread(KernelMode, FALSE, &delay);
        timeout--;
    }

    if (timeout > 0) {
        WRITE_PORT_UCHAR(ComBase, Byte);
    }
}

VOID HdSerialInitialize(PHD_SERIAL_DEBUG Serial)
{
    PUCHAR base = (PUCHAR)(ULONG_PTR)HD_COM1_BASE;

    WRITE_PORT_UCHAR(base + 1, 0x00);
    WRITE_PORT_UCHAR(base + 3, 0x80);
    WRITE_PORT_UCHAR(base + 0, 0x01);
    WRITE_PORT_UCHAR(base + 1, 0x00);
    WRITE_PORT_UCHAR(base + 3, 0x03);
    WRITE_PORT_UCHAR(base + 2, 0xC7);
    WRITE_PORT_UCHAR(base + 4, 0x0B);

    Serial->ComBase = base;
    Serial->Initialized = TRUE;
    Serial->RingHead = 0;
    Serial->RingTail = 0;
    KeInitializeSpinLock(&Serial->RingLock);

    HdSerialWriteString(Serial, "[HyperDisk] COM1 serial debug initialized\r\n");
}

VOID HdSerialWriteChar(PHD_SERIAL_DEBUG Serial, UCHAR Ch)
{
    KIRQL oldIrql;

    if (!Serial->Initialized) return;

    HdSerialWriteByte(Serial->ComBase, Ch);

    KeAcquireSpinLock(&Serial->RingLock, &oldIrql);
    Serial->RingBuffer[Serial->RingHead] = Ch;
    Serial->RingHead = (Serial->RingHead + 1) % HD_SERIAL_BUF_SIZE;
    if (Serial->RingHead == Serial->RingTail) {
        Serial->RingTail = (Serial->RingTail + 1) % HD_SERIAL_BUF_SIZE;
    }
    KeReleaseSpinLock(&Serial->RingLock, oldIrql);
}

VOID HdSerialWriteString(PHD_SERIAL_DEBUG Serial, const char* Str)
{
    if (!Serial->Initialized) return;
    while (*Str) {
        HdSerialWriteChar(Serial, (UCHAR)*Str);
        Str++;
    }
}

VOID HdSerialWriteFormat(PHD_SERIAL_DEBUG Serial, const char* Fmt, ...)
{
    if (!Serial->Initialized) return;
    vDbgPrintExWithPrefix("", DPFLTR_DEFAULT_ID, DPFLTR_INFO_LEVEL, Fmt, (va_list)(&Fmt + 1));
}

VOID HdSerialDumpRingBuffer(PHD_SERIAL_DEBUG Serial)
{
    if (!Serial->Initialized) return;
    KIRQL oldIrql;
    KeAcquireSpinLock(&Serial->RingLock, &oldIrql);
    ULONG tail = Serial->RingTail;
    ULONG head = Serial->RingHead;
    KeReleaseSpinLock(&Serial->RingLock, oldIrql);

    while (tail != head) {
        HdSerialWriteByte(Serial->ComBase, Serial->RingBuffer[tail]);
        tail = (tail + 1) % HD_SERIAL_BUF_SIZE;
    }
}
