use thiserror::Error;

#[derive(Error, Debug)]
pub enum HdError {
    #[error("metadata error: {0}")]
    Metadata(String),
    #[error("block not found: image={image_id} offset={block_offset}")]
    BlockNotFound { image_id: u64, block_offset: u64 },
    #[error("layer referenced: layer_id={0}")]
    LayerReferenced(u32),
    #[error("DNA match failed: {0}")]
    DnaMatchFailed(String),
    #[error("update conflict: {0}")]
    UpdateConflict(String),
    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),
}

pub type HdResult<T> = Result<T, HdError>;
