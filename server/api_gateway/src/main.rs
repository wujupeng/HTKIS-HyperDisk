use anyhow::Result;
use axum::{
    extract::State,
    routing::{get, post},
    Json, Router,
};
use hd_common::config::GatewayConfig;
use serde::{Deserialize, Serialize};
use std::collections::HashSet;
use std::sync::Arc;
use tokio::net::TcpListener;
use tower_http::cors::CorsLayer;
use tracing::info;
use chrono::{DateTime, Utc};

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

#[derive(Debug, Clone, Serialize, Deserialize)]
struct BootReport {
    boot_id: String,
    machine_id: String,
    mac: String,
    phase: String,
    duration_ms: u32,
    result: String,
    #[serde(default)]
    timestamp: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct BootReportResponse {
    accepted: bool,
    boot_id: String,
}

struct AppState {
    start_time: std::time::Instant,
    boot_reports: std::sync::Mutex<Vec<BootReport>>,
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

async fn boot_report_handler(
    State(state): State<Arc<AppState>>,
    Json(mut report): Json<BootReport>,
) -> Json<BootReportResponse> {
    if report.timestamp.is_none() {
        report.timestamp = Some(Utc::now().to_rfc3339());
    }
    info!(
        "BootReport: boot_id={} machine_id={} mac={} phase={} duration_ms={} result={}",
        report.boot_id, report.machine_id, report.mac, report.phase, report.duration_ms, report.result
    );
    let boot_id = report.boot_id.clone();
    if let Ok(mut reports) = state.boot_reports.lock() {
        reports.push(report);
        if reports.len() > 10000 {
            reports.drain(0..1000);
        }
    }
    Json(BootReportResponse {
        accepted: true,
        boot_id,
    })
}

async fn boot_sessions_handler(
    State(state): State<Arc<AppState>>,
) -> Json<serde_json::Value> {
    let reports = state.boot_reports.lock().unwrap_or_else(|e| e.into_inner());
    let sessions: Vec<&BootReport> = reports.iter().collect();
    serde_json::json!({
        "count": sessions.len(),
        "sessions": sessions
    }).into()
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct BootStats {
    machines: usize,
    boots: usize,
    success: usize,
    failed: usize,
    avg_duration_ms: u64,
    min_duration_ms: u32,
    max_duration_ms: u32,
}

async fn boot_stats_handler(
    State(state): State<Arc<AppState>>,
) -> Json<BootStats> {
    let reports = state.boot_reports.lock().unwrap_or_else(|e| e.into_inner());
    let mut machines = HashSet::new();
    let mut success = 0usize;
    let mut failed = 0usize;
    let mut total_duration = 0u64;
    let mut min_duration = u32::MAX;
    let mut max_duration = 0u32;

    for r in reports.iter() {
        machines.insert(r.machine_id.clone());
        match r.result.as_str() {
            "ok" | "success" => success += 1,
            _ => failed += 1,
        }
        total_duration += r.duration_ms as u64;
        if r.duration_ms < min_duration { min_duration = r.duration_ms; }
        if r.duration_ms > max_duration { max_duration = r.duration_ms; }
    }

    let boots = reports.len();
    let avg_duration_ms = if boots > 0 { total_duration / boots as u64 } else { 0 };
    if boots == 0 { min_duration = 0; }

    Json(BootStats {
        machines: machines.len(),
        boots,
        success,
        failed,
        avg_duration_ms,
        min_duration_ms: min_duration,
        max_duration_ms: max_duration,
    })
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct MachineInfo {
    machine_id: String,
    mac: String,
    boots: usize,
    success: usize,
    failed: usize,
    last_seen: String,
    last_boot_id: String,
}

async fn boot_machines_handler(
    State(state): State<Arc<AppState>>,
) -> Json<Vec<MachineInfo>> {
    use std::collections::HashMap;
    let reports = state.boot_reports.lock().unwrap_or_else(|e| e.into_inner());
    let mut machine_map: HashMap<String, MachineInfo> = HashMap::new();
    for r in reports.iter() {
        let entry = machine_map.entry(r.machine_id.clone()).or_insert_with(|| MachineInfo {
            machine_id: r.machine_id.clone(),
            mac: r.mac.clone(),
            boots: 0,
            success: 0,
            failed: 0,
            last_seen: String::new(),
            last_boot_id: String::new(),
        });
        entry.boots += 1;
        match r.result.as_str() {
            "ok" | "success" => entry.success += 1,
            _ => entry.failed += 1,
        }
        if let Some(ref ts) = r.timestamp {
            if ts > &entry.last_seen {
                entry.last_seen = ts.clone();
                entry.last_boot_id = r.boot_id.clone();
            }
        }
    }
    let mut machines: Vec<MachineInfo> = machine_map.into_values().collect();
    machines.sort_by(|a, b| b.boots.cmp(&a.boots));
    Json(machines)
}

async fn boot_sessions_clear_handler(
    State(state): State<Arc<AppState>>,
) -> Json<serde_json::Value> {
    let mut reports = state.boot_reports.lock().unwrap_or_else(|e| e.into_inner());
    let cleared = reports.len();
    reports.clear();
    info!("Cleared {} boot sessions", cleared);
    serde_json::json!({
        "cleared": cleared,
        "remaining": 0
    }).into()
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
        boot_reports: std::sync::Mutex::new(Vec::new()),
    });

    let api_v1 = Router::new()
        .route("/images", get(list_images))
        .route("/terminals", get(list_terminals))
        .route("/boot/report", post(boot_report_handler))
        .route("/boot/sessions", get(boot_sessions_handler))
        .route("/boot/sessions/clear", post(boot_sessions_clear_handler))
        .route("/boot/stats", get(boot_stats_handler))
        .route("/boot/machines", get(boot_machines_handler));

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
