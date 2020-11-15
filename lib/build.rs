use {
    std::{
        env,
        path::Path
    },
    infra::{self, project_build}
};

fn main() -> infra::Result<()> {
    let project_path = Path::new(env!("CARGO_MANIFEST_DIR"));
    let src_path = project_path.join("src");

    let vulkan_sdk = env::var("VULKAN_SDK")?;
    let vulkan_sdk = Path::new(&vulkan_sdk);

    let include_dirs = vec![
        src_path.clone(),
        vulkan_sdk.join("Include")
    ];

    let libraries = vec![
        vulkan_sdk.join("Lib").join("vulkan-1")
    ];

    project_build(src_path, include_dirs, libraries)?;

    Ok(())
}
