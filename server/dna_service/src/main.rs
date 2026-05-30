use anyhow::Result;
use tracing::info;

#[tokio::main]
async fn main() -> Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter("hd_dna_service=debug,info")
        .init();

    info!("HD DNA Service starting...");

    Ok(())
}
