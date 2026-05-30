#pragma once

#include <windows.h>
#include <ntddk.h>
#include <wdm.h>

#define HD_DEVICE_TYPE     0x8800
#define HD_POOL_TAG        'KDHd'

#define IOCTL_HD_ADD_DEVICE \
    CTL_CODE(HD_DEVICE_TYPE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_HD_REMOVE_DEVICE \
    CTL_CODE(HD_DEVICE_TYPE, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_HD_SUBMIT_REQUEST \
    CTL_CODE(HD_DEVICE_TYPE, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_HD_GET_STATUS \
    CTL_CODE(HD_DEVICE_TYPE, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_HD_SET_CONFIG \
    CTL_CODE(HD_DEVICE_TYPE, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _HD_DEVICE_CONFIG {
    ULONGLONG  ImageId;
    ULONGLONG  DiskSize;
    ULONG      BlockSize;
    ULONG      BlockCount;
    UCHAR      LayerId;
    CHAR       ServerAddress[64];
    USHORT     ServerPort;
} HD_DEVICE_CONFIG, *PHD_DEVICE_CONFIG;

typedef struct _HD_BLOCK_REQUEST {
    ULONGLONG  ImageId;
    ULONGLONG  BlockOffset;
    ULONG      BlockCount;
    UCHAR      LayerId;
    UCHAR      IsWrite;
} HD_BLOCK_REQUEST, *PHD_BLOCK_REQUEST;

typedef struct _HD_BLOCK_RESPONSE {
    ULONGLONG  ImageId;
    ULONGLONG  BlockOffset;
    ULONG      BlockCount;
    LONG       Status;
} HD_BLOCK_RESPONSE, *PHD_BLOCK_RESPONSE;
