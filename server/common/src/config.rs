use serde::Deserialize;

#[derive(Debug, Clone, Deserialize)]
pub struct MetadataCenterConfig {
    pub server: MetadataServerConfig,
    pub database: Option<DatabaseConfig>,
    pub rocksdb: RocksdbConfig,
    pub logging: LoggingConfig,
}

#[derive(Debug, Clone, Deserialize)]
pub struct MetadataServerConfig {
    pub listen_addr: String,
    #[serde(default = "default_grpc_reflection")]
    pub grpc_reflection: bool,
}

fn default_grpc_reflection() -> bool {
    true
}

#[derive(Debug, Clone, Deserialize)]
pub struct DatabaseConfig {
    #[serde(default = "default_db_host")]
    pub host: String,
    #[serde(default = "default_db_port")]
    pub port: u16,
    pub database: String,
    pub user: String,
    pub password: String,
    #[serde(default = "default_db_max_conn")]
    pub max_connections: u32,
}

fn default_db_host() -> String { "localhost".to_string() }
fn default_db_port() -> u16 { 5432 }
fn default_db_max_conn() -> u32 { 20 }

#[derive(Debug, Clone, Deserialize)]
pub struct RocksdbConfig {
    pub data_dir: String,
    #[serde(default = "default_cache_mb")]
    pub cache_size_mb: u32,
}

fn default_cache_mb() -> u32 { 4096 }

#[derive(Debug, Clone, Deserialize)]
pub struct GatewayConfig {
    pub server: GatewayServerConfig,
    pub metadata: GatewayMetadataConfig,
    pub auth: Option<AuthConfig>,
    pub logging: LoggingConfig,
}

#[derive(Debug, Clone, Deserialize)]
pub struct GatewayServerConfig {
    pub listen_addr: String,
}

#[derive(Debug, Clone, Deserialize)]
pub struct GatewayMetadataConfig {
    pub grpc_addr: String,
}

#[derive(Debug, Clone, Deserialize)]
pub struct AuthConfig {
    #[serde(default = "default_jwt_secret")]
    pub jwt_secret: String,
    #[serde(default = "default_jwt_expiry")]
    pub jwt_expiry_hours: u64,
}

fn default_jwt_secret() -> String { "change_me_in_production".to_string() }
fn default_jwt_expiry() -> u64 { 24 }

#[derive(Debug, Clone, Deserialize)]
pub struct LoggingConfig {
    #[serde(default = "default_log_level")]
    pub level: String,
    #[serde(default = "default_log_format")]
    pub format: String,
    pub output: Option<String>,
}

fn default_log_level() -> String { "info".to_string() }
fn default_log_format() -> String { "json".to_string() }

impl MetadataCenterConfig {
    pub fn load(path: &str) -> anyhow::Result<Self> {
        let content = std::fs::read_to_string(path)?;
        let config: Self = toml::from_str(&content)?;
        Ok(config)
    }
}

impl GatewayConfig {
    pub fn load(path: &str) -> anyhow::Result<Self> {
        let content = std::fs::read_to_string(path)?;
        let config: Self = toml::from_str(&content)?;
        Ok(config)
    }
}
