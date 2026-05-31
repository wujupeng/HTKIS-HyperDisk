#pragma once

#include <ntifs.h>

#define HD_COM1_BASE       0x3F8
#define HD_COM1_BAUD       115200
#define HD_COM1_DATA_BITS  8
#define HD_COM1_STOP_BITS  1
#define HD_COM1_PARITY     0

#define HD_SERIAL_BUF_SIZE 4096

typedef struct _HD_SERIAL_DEBUG {
    PUCHAR  ComBase;
    BOOLEAN Initialized;
    UCHAR   RingBuffer[HD_SERIAL_BUF_SIZE];
    volatile ULONG RingHead;
    volatile ULONG RingTail;
    KSPIN_LOCK RingLock;
} HD_SERIAL_DEBUG, *PHD_SERIAL_DEBUG;

VOID HdSerialInitialize(PHD_SERIAL_DEBUG Serial);
VOID HdSerialWriteChar(PHD_SERIAL_DEBUG Serial, UCHAR Ch);
VOID HdSerialWriteString(PHD_SERIAL_DEBUG Serial, const char* Str);
VOID HdSerialWriteFormat(PHD_SERIAL_DEBUG Serial, const char* Fmt, ...);

VOID HdSerialDumpRingBuffer(PHD_SERIAL_DEBUG Serial);
