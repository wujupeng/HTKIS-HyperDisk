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
    char buf[256];
    va_list args;
    va_start(args, Fmt);

    size_t len = 0;
    const char* p = Fmt;
    while (*p && len < sizeof(buf) - 1) {
        if (*p == '%') {
            p++;
            if (*p == 'd') {
                int val = va_arg(args, int);
                char num[16];
                for (int i = 0; val > 0 && i < 15; i++) {
                    num[14 - i] = '0' + (val % 10);
                    val /= 10;
                    len++;
                }
            } else if (*p == 's') {
                const char* s = va_arg(args, const char*);
                while (*s && len < sizeof(buf) - 1) buf[len++] = *s++;
            } else if (*p == 'x') {
                unsigned int val = va_arg(args, unsigned int);
                buf[len++] = '0'; buf[len++] = 'x';
                for (int i = 7; i >= 0; i--) {
                    buf[len++] = "0123456789abcdef"[(val >> (i * 4)) & 0xF];
                }
            }
            p++;
        } else {
            buf[len++] = *p++;
        }
    }
    va_end(args);
    buf[len] = '\0';

    HdSerialWriteString(Serial, buf);
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
