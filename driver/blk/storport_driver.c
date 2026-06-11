#include "storport_driver.h"

ULONG DriverEntry(PVOID DriverObject, PVOID RegistryPath)
{
    HW_INITIALIZATION_DATA hwInitData;

    RtlZeroMemory(&hwInitData, sizeof(HW_INITIALIZATION_DATA));
    hwInitData.HwInitializationDataSize = sizeof(HW_INITIALIZATION_DATA);

    hwInitData.HwInitialize  = HdPortHwInitialize;
    hwInitData.HwStartIo     = HdPortHwStartIo;
    hwInitData.HwInterrupt   = NULL;
    hwInitData.HwFindAdapter = HdPortHwFindAdapter;
    hwInitData.HwResetBus    = HdPortHwResetBus;
    hwInitData.HwDmaStarted  = NULL;
    hwInitData.HwAdapterState = NULL;

    hwInitData.DeviceExtensionSize     = sizeof(HD_PORT_EXTENSION);
    hwInitData.SpecificLuExtensionSize = 0;
    hwInitData.SrbExtensionSize        = 0;
    hwInitData.NumberOfAccessRanges    = 0;
    hwInitData.Reserved                = NULL;

    hwInitData.MapBuffers             = TRUE;
    hwInitData.NeedPhysicalAddresses   = FALSE;
    hwInitData.TaggedQueuing           = TRUE;
    hwInitData.AutoRequestSense        = TRUE;
    hwInitData.MultipleRequestPerLu    = TRUE;
    hwInitData.ReceiveEvent            = FALSE;

    hwInitData.VendorIdLength = 8;
    hwInitData.VendorId       = "HTKIS   ";
    hwInitData.DeviceIdLength = 16;
    hwInitData.DeviceId       = "HyperDisk VDisk  ";

    hwInitData.HwAdapterControl = NULL;
    hwInitData.HwBuildIo        = NULL;

    hwInitData.HwFreeAdapterResources  = NULL;
    hwInitData.HwProcessServiceRequest  = NULL;
    hwInitData.HwCompleteServiceIrp     = NULL;
    hwInitData.HwInitializeTracing      = NULL;
    hwInitData.HwCleanupTracing         = NULL;
    hwInitData.HwTracingEnabled         = NULL;

    hwInitData.FeatureSupport    = 0;
    hwInitData.SrbTypeFlags      = SRB_TYPE_FLAG_STORAGE_REQUEST_BLOCK;
    hwInitData.AddressTypeFlags  = 0;

    hwInitData.AdapterInterfaceType = Internal;

    return StorPortInitialize(DriverObject, RegistryPath, &hwInitData, NULL);
}

ULONG HdPortHwFindAdapter(PVOID DeviceExtension, PVOID HwContext, PVOID BusInformation,
    PCHAR ArgumentString, PPORT_CONFIGURATION_INFORMATION ConfigInfo, PBOOLEAN Again)
{
    PHD_PORT_EXTENSION ext = (PHD_PORT_EXTENSION)DeviceExtension;

    UNREFERENCED_PARAMETER(HwContext);
    UNREFERENCED_PARAMETER(BusInformation);
    UNREFERENCED_PARAMETER(ArgumentString);

    ConfigInfo->MaximumNumberOfLogicalUnits = 1;
    ConfigInfo->MaximumNumberOfTargets      = 1;
    ConfigInfo->NumberOfBuses               = 1;
    ConfigInfo->MaximumTransferLength       = HD_MAX_TRANSFER;
    ConfigInfo->CachesData                  = FALSE;
    ConfigInfo->MapBuffers                  = TRUE;
    ConfigInfo->NeedPhysicalAddresses       = FALSE;
    ConfigInfo->TaggedQueuing               = TRUE;
    ConfigInfo->AutoRequestSense            = TRUE;
    ConfigInfo->MultipleRequestPerLu        = TRUE;
    ConfigInfo->ReceiveEvent                = FALSE;
    ConfigInfo->RealModeInitialized         = FALSE;

    ext->DiskSize     = (ULONGLONG)HD_DISK_SIZE_GB * 1024 * 1024 * 1024;
    ext->SectorSize   = HD_SECTOR_SIZE;
    ext->TotalSectors = (ULONG)(ext->DiskSize / ext->SectorSize);

    ext->SectorsPerTrack    = 63;
    ext->TracksPerCylinder  = 255;
    ext->Cylinders          = ext->TotalSectors / (ext->SectorsPerTrack * ext->TracksPerCylinder);
    if (ext->Cylinders == 0) ext->Cylinders = 1;

    RtlCopyMemory(ext->ServerAddress, "192.168.2.110", 14);
    ext->ServerPort   = 9527;
    ext->Initialized  = TRUE;

    *Again = FALSE;

    return SP_RETURN_FOUND;
}

BOOLEAN HdPortHwInitialize(PVOID DeviceExtension)
{
    UNREFERENCED_PARAMETER(DeviceExtension);
    return TRUE;
}

BOOLEAN HdPortHwStartIo(PVOID DeviceExtension, PSCSI_REQUEST_BLOCK Srb)
{
    PHD_PORT_EXTENSION ext = (PHD_PORT_EXTENSION)DeviceExtension;

    switch (Srb->Function) {
    case SRB_FUNCTION_EXECUTE_SCSI:
        HdPortHandleScsi(ext, Srb);
        break;

    case SRB_FUNCTION_IO_CONTROL:
        Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
        StorPortCompleteRequest(DeviceExtension, Srb->PathId, Srb->TargetId, Srb->Lun, Srb->SrbStatus);
        break;

    case SRB_FUNCTION_FLUSH:
    case SRB_FUNCTION_SHUTDOWN:
        Srb->SrbStatus = SRB_STATUS_SUCCESS;
        StorPortCompleteRequest(DeviceExtension, Srb->PathId, Srb->TargetId, Srb->Lun, Srb->SrbStatus);
        break;

    case SRB_FUNCTION_RESET_DEVICE:
    case SRB_FUNCTION_RESET_BUS:
        Srb->SrbStatus = SRB_STATUS_SUCCESS;
        StorPortCompleteRequest(DeviceExtension, Srb->PathId, Srb->TargetId, Srb->Lun, Srb->SrbStatus);
        break;

    case SRB_FUNCTION_RELEASE_DEVICE:
        Srb->SrbStatus = SRB_STATUS_SUCCESS;
        StorPortCompleteRequest(DeviceExtension, Srb->PathId, Srb->TargetId, Srb->Lun, Srb->SrbStatus);
        break;

    default:
        Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
        StorPortCompleteRequest(DeviceExtension, Srb->PathId, Srb->TargetId, Srb->Lun, Srb->SrbStatus);
        break;
    }

    return TRUE;
}

VOID HdPortHandleScsi(PHD_PORT_EXTENSION Ext, PSCSI_REQUEST_BLOCK Srb)
{
    switch (Srb->Cdb[0]) {
    case SCSIOP_READ:
    case SCSIOP_WRITE:
        HdPortHandleReadWrite(Ext, Srb);
        return;

    case SCSIOP_READ_CAPACITY:
        HdPortHandleReadCapacity(Ext, Srb);
        return;

    case SCSIOP_READ_CAPACITY16:
        HdPortHandleReadCapacity16(Ext, Srb);
        return;

    case SCSIOP_INQUIRY:
        HdPortHandleInquiry(Ext, Srb);
        return;

    case SCSIOP_MODE_SENSE:
        HdPortHandleModeSense(Ext, Srb);
        return;

    case SCSIOP_TEST_UNIT_READY:
        Srb->SrbStatus = SRB_STATUS_SUCCESS;
        Srb->DataTransferLength = 0;
        StorPortCompleteRequest(Ext, Srb->PathId, Srb->TargetId, Srb->Lun, Srb->SrbStatus);
        return;

    case SCSIOP_REQUEST_SENSE:
        HdPortHandleRequestSense(Ext, Srb);
        return;

    case SCSIOP_START_STOP_UNIT:
    case SCSIOP_VERIFY:
    case SCSIOP_MEDIUM_REMOVAL:
        Srb->SrbStatus = SRB_STATUS_SUCCESS;
        Srb->DataTransferLength = 0;
        StorPortCompleteRequest(Ext, Srb->PathId, Srb->TargetId, Srb->Lun, Srb->SrbStatus);
        return;

    default:
        Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
        StorPortCompleteRequest(Ext, Srb->PathId, Srb->TargetId, Srb->Lun, Srb->SrbStatus);
        return;
    }
}

VOID HdPortHandleInquiry(PHD_PORT_EXTENSION Ext, PSCSI_REQUEST_BLOCK Srb)
{
    PINQUIRYDATA inquiryData = (PINQUIRYDATA)Srb->DataBuffer;
    ULONG dataLen = Srb->DataTransferLength;

    UNREFERENCED_PARAMETER(Ext);

    if (dataLen < sizeof(INQUIRYDATA)) {
        Srb->SrbStatus = SRB_STATUS_DATA_OVERRUN;
        StorPortCompleteRequest(Ext, Srb->PathId, Srb->TargetId, Srb->Lun, Srb->SrbStatus);
        return;
    }

    RtlZeroMemory(inquiryData, sizeof(INQUIRYDATA));

    inquiryData->DeviceType       = DIRECT_ACCESS_DEVICE;
    inquiryData->DeviceTypeQualifier = DEVICE_CONNECTED;
    inquiryData->Versions         = 5;
    inquiryData->ResponseDataFormat = 2;
    inquiryData->AdditionalLength = 31;
    inquiryData->RemovableMedia   = FALSE;

    StorPortMoveMemory(inquiryData->VendorId, "HTKIS   ", 8);
    StorPortMoveMemory(inquiryData->ProductId, "HyperDisk VDisk  ", 16);
    StorPortMoveMemory(inquiryData->ProductRevisionLevel, "1.0", 4);

    Srb->SrbStatus = SRB_STATUS_SUCCESS;
    Srb->DataTransferLength = sizeof(INQUIRYDATA);
    StorPortCompleteRequest(Ext, Srb->PathId, Srb->TargetId, Srb->Lun, Srb->SrbStatus);
}

VOID HdPortHandleReadCapacity(PHD_PORT_EXTENSION Ext, PSCSI_REQUEST_BLOCK Srb)
{
    PREAD_CAPACITY_DATA capacityData = (PREAD_CAPACITY_DATA)Srb->DataBuffer;
    ULONG lastLba;

    if (Ext->TotalSectors - 1 > 0xFFFFFFFF) {
        lastLba = 0xFFFFFFFF;
    } else {
        lastLba = Ext->TotalSectors - 1;
    }

    RtlZeroMemory(capacityData, sizeof(READ_CAPACITY_DATA));

    capacityData->LogicalBlockAddress = _byteswap_ulong(lastLba);
    capacityData->BytesPerBlock = _byteswap_ulong(Ext->SectorSize);

    Srb->SrbStatus = SRB_STATUS_SUCCESS;
    Srb->DataTransferLength = sizeof(READ_CAPACITY_DATA);
    StorPortCompleteRequest(Ext, Srb->PathId, Srb->TargetId, Srb->Lun, Srb->SrbStatus);
}

VOID HdPortHandleReadCapacity16(PHD_PORT_EXTENSION Ext, PSCSI_REQUEST_BLOCK Srb)
{
    PREAD_CAPACITY16_DATA capacityData = (PREAD_CAPACITY16_DATA)Srb->DataBuffer;

    RtlZeroMemory(capacityData, sizeof(READ_CAPACITY16_DATA));

    capacityData->LogicalBlockAddress.QuadPart = (LONGLONG)(Ext->TotalSectors - 1);
    capacityData->LogicalBlockAddress.QuadPart = _byteswap_uint64(capacityData->LogicalBlockAddress.QuadPart);
    capacityData->BytesPerBlock = _byteswap_ulong(Ext->SectorSize);

    Srb->SrbStatus = SRB_STATUS_SUCCESS;
    Srb->DataTransferLength = sizeof(READ_CAPACITY16_DATA);
    StorPortCompleteRequest(Ext, Srb->PathId, Srb->TargetId, Srb->Lun, Srb->SrbStatus);
}

VOID HdPortHandleReadWrite(PHD_PORT_EXTENSION Ext, PSCSI_REQUEST_BLOCK Srb)
{
    UNREFERENCED_PARAMETER(Ext);

    Srb->SrbStatus = SRB_STATUS_SUCCESS;
    StorPortCompleteRequest(Ext, Srb->PathId, Srb->TargetId, Srb->Lun, Srb->SrbStatus);
}

VOID HdPortHandleModeSense(PHD_PORT_EXTENSION Ext, PSCSI_REQUEST_BLOCK Srb)
{
    Srb->SrbStatus = SRB_STATUS_SUCCESS;
    Srb->DataTransferLength = 0;
    StorPortCompleteRequest(Ext, Srb->PathId, Srb->TargetId, Srb->Lun, Srb->SrbStatus);
}

VOID HdPortHandleRequestSense(PHD_PORT_EXTENSION Ext, PSCSI_REQUEST_BLOCK Srb)
{
    PSENSE_DATA senseData = (PSENSE_DATA)Srb->DataBuffer;

    RtlZeroMemory(senseData, sizeof(SENSE_DATA));
    senseData->ErrorCode = 0x70;
    senseData->SenseKey  = SCSI_SENSE_NO_SENSE;

    Srb->SrbStatus = SRB_STATUS_SUCCESS;
    Srb->DataTransferLength = sizeof(SENSE_DATA);
    StorPortCompleteRequest(Ext, Srb->PathId, Srb->TargetId, Srb->Lun, Srb->SrbStatus);
}

ULONG HdPortHwResetBus(PVOID DeviceExtension, ULONG PathId)
{
    UNREFERENCED_PARAMETER(DeviceExtension);
    UNREFERENCED_PARAMETER(PathId);
    return SP_RETURN_FOUND;
}
