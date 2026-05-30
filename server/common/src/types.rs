use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ImageMeta {
    pub image_id: u64,
    pub name: String,
    pub total_size: u64,
    pub block_count: u32,
    pub os_layer_id: u32,
    pub driver_layer_id: u32,
    pub app_layer_id: u32,
    pub created_at: i64,
    pub updated_at: i64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LayerMeta {
    pub layer_id: u32,
    pub layer_type: u8,
    pub total_size: u64,
    pub block_count: u32,
    pub ref_count: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DriverDna {
    pub terminal_id: u64,
    pub pci_ids: Vec<PciId>,
    pub acpi_hash: String,
    pub cpu_model: String,
    pub cpu_microcode: u32,
    pub bios_vendor: String,
    pub bios_version: String,
    pub digest: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PciId {
    pub vendor_id: u16,
    pub device_id: u16,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum LayerType {
    Os = 0,
    Driver = 1,
    App = 2,
    Overlay = 3,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum WritePolicy {
    RamOverlay = 0,
    NvmeOverlay = 1,
    PersistentUser = 2,
    ReadOnly = 3,
    Snapshot = 4,
    Hybrid = 5,
}
