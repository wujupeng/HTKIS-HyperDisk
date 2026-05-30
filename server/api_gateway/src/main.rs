use anyhow::Result;
use tracing::info;

#[tokio::main]
async fn main() -> Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter("hd_api_gateway=debug,info")
        .init();

    info!("HD API Gateway starting...");

    Ok(())
}
