use anyhow::Result;
use axum::{
    extract::State,
    routing::get,
    Json, Router,
};
use hd_common::config::GatewayConfig;
use serde::{Deserialize, Serialize};
use std::sync::Arc;
use tokio::net::TcpListener;
use tower_http::cors::CorsLayer;
use tracing::info;

#[derive(Debug, Clone, Serialize, Deserialize)]
struct HealthResponse {
    status: String,
    version: String,
    uptime_seconds: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct ImagesResponse {
    images: Vec<ImageInfo>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct ImageInfo {
    image_id: u64,
    name: String,
    total_size: u64,
    block_count: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct TerminalsResponse {
    terminals: Vec<TerminalInfo>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct TerminalInfo {
    terminal_id: u64,
    hostname: String,
    status: String,
}

struct AppState {
    start_time: std::time::Instant,
}

async fn health_handler(State(state): State<Arc<AppState>>) -> Json<HealthResponse> {
    Json(HealthResponse {
        status: "ok".to_string(),
        version: env!("CARGO_PKG_VERSION").to_string(),
        uptime_seconds: state.start_time.elapsed().as_secs(),
    })
}

async fn list_images() -> Json<ImagesResponse> {
    Json(ImagesResponse { images: vec![] })
}

async fn list_terminals() -> Json<TerminalsResponse> {
    Json(TerminalsResponse { terminals: vec![] })
}

async fn metrics_handler() -> &'static str {
    "# HELP hd_gateway_up Gateway status\n# TYPE hd_gateway_up gauge\nhd_gateway_up 1\n"
}

#[tokio::main]
async fn main() -> Result<()> {
    let config_path = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "--config".to_string());

    let config = if config_path == "--config" {
        let config_file = std::env::args().nth(2).unwrap_or_else(|| "etc/gateway.toml".to_string());
        match GatewayConfig::load(&config_file) {
            Ok(c) => c,
            Err(e) => {
                eprintln!("Failed to load config from {}: {}, using defaults", config_file, e);
                return Err(e);
            }
        }
    } else {
        eprintln!("Usage: hd-api-gateway --config <path>");
        std::process::exit(1);
    };

    let log_level = match config.logging.level.as_str() {
        "trace" => "hd_api_gateway=trace,info",
        "debug" => "hd_api_gateway=debug,info",
        _ => "info",
    };

    tracing_subscriber::fmt()
        .with_env_filter(log_level)
        .init();

    info!("HD API Gateway starting...");
    info!("Config: listen={}", config.server.listen_addr);
    info!("MetadataCenter: {}", config.metadata.grpc_addr);

    let state = Arc::new(AppState {
        start_time: std::time::Instant::now(),
    });

    let api_v1 = Router::new()
        .route("/images", get(list_images))
        .route("/terminals", get(list_terminals));

    let app = Router::new()
        .route("/health", get(health_handler))
        .route("/metrics", get(metrics_handler))
        .nest("/api/v1", api_v1)
        .layer(CorsLayer::permissive())
        .layer(tower_http::trace::TraceLayer::new_for_http())
        .with_state(state);

    let listener = TcpListener::bind(&config.server.listen_addr).await?;
    info!("HTTP server listening on {}", config.server.listen_addr);

    axum::serve(listener, app).await?;

    info!("HD API Gateway stopped.");
    Ok(())
}
