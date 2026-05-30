use anyhow::Result;
use tonic::{transport::Server, Request, Response, Status};
use tracing::info;

pub mod hyperdisk {
    pub mod dna {
        tonic::include_proto!("hyperdisk.dna");
    }
}

use hyperdisk::dna::{
    dna_service_server::{DnaService as DnaServiceTrait, DnaServiceServer},
    FingerprintRequest, FingerprintResponse, GetDriverComboRequest, GetDriverComboResponse,
    MatchDnaRequest, MatchDnaResponse,
};

#[derive(Default)]
pub struct MyDnaService;

#[tonic::async_trait]
impl DnaServiceTrait for MyDnaService {
    async fn collect_fingerprint(
        &self,
        request: Request<FingerprintRequest>,
    ) -> Result<Response<FingerprintResponse>, Status> {
        let req = request.into_inner();
        info!("collect_fingerprint: terminal_id={}", req.terminal_id);
        Ok(Response::new(FingerprintResponse {
            dna_digest: vec![],
            dna_group_id: 0,
            is_new_group: false,
        }))
    }

    async fn match_dna_group(
        &self,
        request: Request<MatchDnaRequest>,
    ) -> Result<Response<MatchDnaResponse>, Status> {
        let req = request.into_inner();
        info!("match_dna_group: digest_len={}", req.dna_digest.len());
        Ok(Response::new(MatchDnaResponse {
            dna_group_id: 0,
            match_score: 0.0,
        }))
    }

    async fn get_driver_combo(
        &self,
        request: Request<GetDriverComboRequest>,
    ) -> Result<Response<GetDriverComboResponse>, Status> {
        let req = request.into_inner();
        info!("get_driver_combo: group={} image={}", req.dna_group_id, req.image_id);
        Ok(Response::new(GetDriverComboResponse {
            driver_layer_id: 0,
            drivers: vec![],
            score: 0.0,
        }))
    }
}

#[tokio::main]
async fn main() -> Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter("hd_dna_service=debug,info")
        .init();

    info!("HD DNA Service starting...");

    let addr = "0.0.0.0:50052".parse()?;
    let service = MyDnaService::default();

    info!("gRPC server listening on {}", addr);
    Server::builder()
        .add_service(DnaServiceServer::new(service))
        .serve(addr)
        .await?;

    Ok(())
}
