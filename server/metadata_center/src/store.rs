use anyhow::{Context, Result};
use rocksdb::{ColumnFamily, DB, Options};
use std::path::Path;
use std::sync::Arc;
use tokio::sync::RwLock;
use tracing::{debug, info};

const CF_BLOCK_INDEX: &str = "cf_block_index";
const CF_LAYER_MANIFEST: &str = "cf_layer_manifest";
const CF_DNA_INDEX: &str = "cf_dna_index";
const CF_CACHE_META: &str = "cf_cache_meta";
const CF_TERMINAL_SESSION: &str = "cf_terminal_session";
const CF_NODE_REGISTRY: &str = "cf_node_registry";

const ALL_CFS: &[&str] = &[
    CF_BLOCK_INDEX,
    CF_LAYER_MANIFEST,
    CF_DNA_INDEX,
    CF_CACHE_META,
    CF_TERMINAL_SESSION,
    CF_NODE_REGISTRY,
];

pub struct MetadataStore {
    db: Arc<DB>,
}

impl MetadataStore {
    pub fn open(path: &str) -> Result<Self> {
        let db_path = Path::new(path);
        let mut opts = Options::default();
        opts.create_if_missing(true);
        opts.create_missing_column_families(true);
        opts.set_max_open_files(10000);
        opts.set_use_fsync(false);
        opts.set_bytes_per_sync(1048576);

        let cfs: Vec<(&str, Options)> = ALL_CFS.iter().map(|&name| (name, Options::default())).collect();

        let db = DB::open_cf_descriptors(&opts, db_path, cfs.iter().map(|(name, opts)| {
            rocksdb::ColumnFamilyDescriptor::new(*name, opts.clone())
        }))
        .context("Failed to open RocksDB")?;

        info!("MetadataStore opened at {}", path);
        Ok(Self { db: Arc::new(db) })
    }

    pub fn put(&self, cf_name: &str, key: &[u8], value: &[u8]) -> Result<()> {
        let cf = self.cf(cf_name)?;
        self.db.put_cf(cf, key, value)?;
        debug!("PUT cf={} key={:?} len={}", cf_name, key, value.len());
        Ok(())
    }

    pub fn get(&self, cf_name: &str, key: &[u8]) -> Result<Option<Vec<u8>>> {
        let cf = self.cf(cf_name)?;
        let result = self.db.get_cf(cf, key)?;
        Ok(result)
    }

    pub fn delete(&self, cf_name: &str, key: &[u8]) -> Result<()> {
        let cf = self.cf(cf_name)?;
        self.db.delete_cf(cf, key)?;
        debug!("DELETE cf={} key={:?}", cf_name, key);
        Ok(())
    }

    fn cf(&self, name: &str) -> Result<&ColumnFamily> {
        self.db.cf_handle(name)
            .context(format!("Column family '{}' not found", name))
    }

    pub fn db(&self) -> Arc<DB> {
        Arc::clone(&self.db)
    }
}

pub struct MetadataService {
    store: Arc<RwLock<MetadataStore>>,
}

impl MetadataService {
    pub fn new(store: MetadataStore) -> Self {
        Self {
            store: Arc::new(RwLock::new(store)),
        }
    }

    pub async fn lookup_block(
        &self,
        image_id: u64,
        block_offset: u64,
        block_count: u32,
        layer_id: u8,
    ) -> Result<Option<Vec<u8>>> {
        let store = self.store.read().await;
        let mut key = Vec::with_capacity(21);
        key.extend_from_slice(&image_id.to_be_bytes());
        key.extend_from_slice(&block_offset.to_be_bytes());
        key.extend_from_slice(&layer_id.to_be_bytes());

        store.get(CF_BLOCK_INDEX, &key)
    }

    pub async fn register_block(
        &self,
        image_id: u64,
        block_offset: u64,
        block_count: u32,
        layer_id: u8,
        node_id: u32,
    ) -> Result<()> {
        let store = self.store.write().await;
        let mut key = Vec::with_capacity(21);
        key.extend_from_slice(&image_id.to_be_bytes());
        key.extend_from_slice(&block_offset.to_be_bytes());
        key.extend_from_slice(&layer_id.to_be_bytes());

        store.put(CF_BLOCK_INDEX, &key, &node_id.to_be_bytes())
    }
}
