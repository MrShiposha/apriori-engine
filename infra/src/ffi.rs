use {
    std::{
        fs,
        path::{Path, PathBuf}
    },
    crate::{Result, Error},
};

pub const FOREIGN_FN_IFACE_DIR_NAME: &'static str = "ffi";
const RUST_VISIBLE_DIR: &'static str = "export";
const VULKAN_ITEM_REGEX: &'static str = r"(PFN_)?((vk)|(Vk)|(VK)).*";

pub fn process_c_srcs(dir: &Path, include_dirs: &Vec<PathBuf>, cc_build: &mut cc::Build) -> Result<()> {
    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let path = entry.path();

        if path.is_file() {
            continue;
        }

        if let Some(dir_name) = path.components().last() {
            if dir_name.as_os_str().to_string_lossy() == FOREIGN_FN_IFACE_DIR_NAME {
                let builder = bindgen::Builder::default()
                    .clang_args(
                        include_dirs.iter()
                            .map(|path| format!("-F{}", path.display()))
                    )
                    .blacklist_item(VULKAN_ITEM_REGEX)
                    .raw_line("#![allow(unused_variables)]")
                    .raw_line("#![allow(non_snake_case)]")
                    .raw_line("#![allow(non_camel_case_types)]")
                    .raw_line("#![allow(dead_code)]")
                    .raw_line("#![allow(non_upper_case_globals)]");

                let (builder, bindings_count) = process_ffi_dir(&path, builder, cc_build)?;

                if bindings_count > 0 {
                    let ffi_mod_path = path.join("mod.rs");
                    let bindings = builder.generate()
                        .map_err(|_| Error::Bindgen)?;
                    bindings.write_to_file(ffi_mod_path)?;
                }

                break;
            }
        }
    }

    Ok(())
}

fn process_ffi_dir(dir: &Path, mut builder: bindgen::Builder, cc_build: &mut cc::Build)
    -> Result<(bindgen::Builder, usize)> {
    let c_ext = "c";
    let h_ext = "h";

    let mut bindings_count = 0;

    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let path = entry.path();

        if path.is_dir() {
            let (subdir_builder, subdir_bindins_count) = process_ffi_dir(
                &path,
                builder,
                cc_build
            )?;

            builder = subdir_builder;
            bindings_count += subdir_bindins_count;
        } else if let Some(ext) = path.extension() {
            let file_name = path.file_stem()
                .ok_or(Error::FilenameExpected(path.clone().into()))?;

            let parent_dir = path.parent()
                .ok_or(Error::ParentDirExpected(path.clone()))?;

            let parent_dir_name = parent_dir
                .file_stem()
                .ok_or(Error::FilenameExpected(parent_dir.into()))?
                .to_string_lossy();

            if file_name.to_string_lossy().ends_with("shader") {
                continue;
            } else if parent_dir_name == RUST_VISIBLE_DIR && ext == h_ext {
                builder = builder.header(path.to_string_lossy());
                bindings_count += 1;
            }

            if ext == c_ext {
                cc_build.file(path.clone());
            }

            println!("cargo:rerun-if-changed={}", path.display());

        }
    }

    Ok((builder, bindings_count))
}