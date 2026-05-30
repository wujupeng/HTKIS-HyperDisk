mod store;

use anyhow::Result;
use hd_common::config::MetadataCenterConfig;
use store::{MetadataService, MetadataStore};
use tonic::{transport::Server, Request, Response, Status};
use tracing::{error, info};

pub mod hyperdisk {
    pub mod metadata {
        tonic::include_proto!("hyperdisk.metadata");
    }
}

use hyperdisk::metadata::{
    metadata_service_server::{MetadataService as MetadataServiceTrait, MetadataServiceServer},
    GetImageMetaRequest, GetImageMetaResponse, GetLayerMetaRequest, GetLayerMetaResponse,
    LookupBlockRequest, LookupBlockResponse, RegisterBlockRequest, RegisterBlockResponse,
    RegisterTerminalRequest, RegisterTerminalResponse, UpdateTerminalStatusRequest,
    UpdateTerminalStatusResponse,
};

#[tonic::async_trait]
impl MetadataServiceTrait for MetadataService {
    async fn lookup_block(
        &self,
        request: Request<LookupBlockRequest>,
    ) -> Result<Response<LookupBlockResponse>, Status> {
        let req = request.into_inner();
        info!(
            "lookup_block: image_id={} offset={} count={} layer={}",
            req.image_id, req.block_offset, req.block_count, req.layer_id
        );

        match self.lookup_block(req.image_id, req.block_offset, req.block_count, req.layer_id as u8).await {
            Ok(Some(data)) => {
                let node_id = u32::from_be_bytes(data[..4].try_into().unwrap_or([0; 4]));
                Ok(Response::new(LookupBlockResponse {
                    image_id: req.image_id,
                    block_offset: req.block_offset,
                    block_count: req.block_count,
                    node_id,
                    cache_hit: true,
                }))
            }
            Ok(None) => Ok(Response::new(LookupBlockResponse {
                image_id: req.image_id,
                block_offset: req.block_offset,
                block_count: req.block_count,
                node_id: 0,
                cache_hit: false,
            })),
            Err(e) => {
                error!("lookup_block error: {}", e);
                Err(Status::internal(e.to_string()))
            }
        }
    }

    async fn register_block(
        &self,
        request: Request<RegisterBlockRequest>,
    ) -> Result<Response<RegisterBlockResponse>, Status> {
        let req = request.into_inner();
        info!(
            "register_block: image_id={} offset={} node_id={}",
            req.image_id, req.block_offset, req.node_id
        );

        match self.register_block(req.image_id, req.block_offset, req.block_count, req.layer_id as u8, req.node_id).await {
            Ok(()) => Ok(Response::new(RegisterBlockResponse { success: true })),
            Err(e) => {
                error!("register_block error: {}", e);
                Err(Status::internal(e.to_string()))
            }
        }
    }

    async fn get_image_meta(
        &self,
        request: Request<GetImageMetaRequest>,
    ) -> Result<Response<GetImageMetaResponse>, Status> {
        let req = request.into_inner();
        info!("get_image_meta: image_id={}", req.image_id);
        Ok(Response::new(GetImageMetaResponse {
            image_id: req.image_id,
            name: String::new(),
            total_size: 0,
            block_count: 0,
            os_layer_id: 0,
            driver_layer_id: 0,
            app_layer_id: 0,
            created_at: 0,
            updated_at: 0,
        }))
    }

    async fn get_layer_meta(
        &self,
        request: Request<GetLayerMetaRequest>,
    ) -> Result<Response<GetLayerMetaResponse>, Status> {
        let req = request.into_inner();
        info!("get_layer_meta: layer_id={}", req.layer_id);
        Ok(Response::new(GetLayerMetaResponse {
            layer_id: req.layer_id,
            layer_type: 0,
            total_size: 0,
            block_count: 0,
            ref_count: 0,
            block_map_hash: Vec::new(),
        }))
    }

    async fn register_terminal(
        &self,
        request: Request<RegisterTerminalRequest>,
    ) -> Result<Response<RegisterTerminalResponse>, Status> {
        let req = request.into_inner();
        info!("register_terminal: terminal_id={} hostname={}", req.terminal_id, req.hostname);
        Ok(Response::new(RegisterTerminalResponse {
            success: true,
            session_id: uuid::Uuid::new_v4().as_u128() as u64,
        }))
    }

    async fn update_terminal_status(
        &self,
        request: Request<UpdateTerminalStatusRequest>,
    ) -> Result<Response<UpdateTerminalStatusResponse>, Status> {
        let req = request.into_inner();
        info!(
            "update_terminal_status: terminal_id={} status={}",
            req.terminal_id, req.status
        );
        Ok(Response::new(UpdateTerminalStatusResponse { success: true }))
    }
}

#[tokio::main]
async fn main() -> Result<()> {
    let config_path = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "--config".to_string());

    let config = if config_path == "--config" {
        let config_file = std::env::args().nth(2).unwrap_or_else(|| "etc/metadata.toml".to_string());
        match MetadataCenterConfig::load(&config_file) {
            Ok(c) => c,
            Err(e) => {
                eprintln!("Failed to load config from {}: {}, using defaults", config_file, e);
                return Err(e);
            }
        }
    } else {
        eprintln!("Usage: hd-metadata-center --config <path>");
        std::process::exit(1);
    };

    let log_level = match config.logging.level.as_str() {
        "trace" => "hd_metadata_center=trace,info",
        "debug" => "hd_metadata_center=debug,info",
        _ => "info",
    };

    tracing_subscriber::fmt()
        .with_env_filter(log_level)
        .init();

    info!("HD Metadata Center starting...");
    info!("Config: listen={}", config.server.listen_addr);

    let store = MetadataStore::open(&config.rocksdb.data_dir)?;
    info!("RocksDB metadata store initialized at {}", config.rocksdb.data_dir);

    let service = MetadataService::new(store);
    let addr = config.server.listen_addr.parse()?;

    info!("gRPC server listening on {}", addr);

    Server::builder()
        .add_service(MetadataServiceServer::new(service))
        .serve(addr)
        .await?;

    info!("HD Metadata Center stopped.");
    Ok(())
}
