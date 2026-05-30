mod store;

use anyhow::Result;
use store::MetadataStore;
use tracing::info;

#[tokio::main]
async fn main() -> Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter("hd_metadata_center=debug,info")
        .init();

    info!("HD Metadata Center starting...");

    let store = MetadataStore::open("./data/metadata")?;
    info!("RocksDB metadata store initialized");

    Ok(())
}
