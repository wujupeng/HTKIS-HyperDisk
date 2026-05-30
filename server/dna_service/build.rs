fn main() -> Result<(), Box<dyn std::error::Error>> {
    let proto_dir = std::path::PathBuf::from("../../proto");
    tonic_build::compile_protos(proto_dir.join("dna.proto"))?;
    Ok(())
}
