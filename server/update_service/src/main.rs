use anyhow::Result;
use tonic::{transport::Server, Request, Response, Status};
use tracing::info;

pub mod hyperdisk {
    pub mod update {
        tonic::include_proto!("hyperdisk.update");
    }
}

use hyperdisk::update::{
    update_service_server::{UpdateService as UpdateServiceTrait, UpdateServiceServer},
    AdvanceCanaryRequest, AdvanceCanaryResponse, CanaryStrategy, CreateCanaryRequest,
    CreateSnapshotRequest, DeltaRequest, DeltaResponse, RollbackRequest, RollbackResponse,
    SnapshotInfo,
};

#[derive(Default)]
pub struct MyUpdateService;

#[tonic::async_trait]
impl UpdateServiceTrait for MyUpdateService {
    async fn create_canary_strategy(
        &self,
        request: Request<CreateCanaryRequest>,
    ) -> Result<Response<CanaryStrategy>, Status> {
        let req = request.into_inner();
        info!("create_canary_strategy: image_id={}", req.target_image_id);
        Ok(Response::new(CanaryStrategy {
            strategy_id: 0,
            target_image_id: req.target_image_id,
            state: "draft".to_string(),
            current_batch: 0,
            total_batches: req.total_batches,
            fault_threshold: req.fault_threshold,
        }))
    }

    async fn advance_canary_batch(
        &self,
        request: Request<AdvanceCanaryRequest>,
    ) -> Result<Response<AdvanceCanaryResponse>, Status> {
        let req = request.into_inner();
        info!("advance_canary_batch: strategy_id={}", req.strategy_id);
        Ok(Response::new(AdvanceCanaryResponse {
            new_state: "running".to_string(),
            current_batch: 1,
        }))
    }

    async fn rollback(
        &self,
        request: Request<RollbackRequest>,
    ) -> Result<Response<RollbackResponse>, Status> {
        let req = request.into_inner();
        info!("rollback: strategy_id={}", req.strategy_id);
        Ok(Response::new(RollbackResponse {
            success: true,
            elapsed_ms: 0,
        }))
    }

    async fn create_snapshot(
        &self,
        request: Request<CreateSnapshotRequest>,
    ) -> Result<Response<SnapshotInfo>, Status> {
        let req = request.into_inner();
        info!("create_snapshot: image_id={} name={}", req.image_id, req.name);
        Ok(Response::new(SnapshotInfo {
            snapshot_id: 0,
            image_id: req.image_id,
            name: req.name,
            created_at: chrono::Utc::now().timestamp_millis() as u64,
        }))
    }

    async fn compute_delta(
        &self,
        request: Request<DeltaRequest>,
    ) -> Result<Response<DeltaResponse>, Status> {
        let req = request.into_inner();
        info!("compute_delta: {} -> {}", req.source_version, req.target_version);
        Ok(Response::new(DeltaResponse {
            add_blocks: vec![],
            modify_blocks: vec![],
            delete_blocks: vec![],
        }))
    }
}

#[tokio::main]
async fn main() -> Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter("hd_update_service=debug,info")
        .init();

    info!("HD Update Service starting...");

    let addr = "0.0.0.0:50053".parse()?;
    let service = MyUpdateService::default();

    info!("gRPC server listening on {}", addr);
    Server::builder()
        .add_service(UpdateServiceServer::new(service))
        .serve(addr)
        .await?;

    Ok(())
}
