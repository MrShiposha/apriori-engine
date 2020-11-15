use {
    std::{
        fs,
        io::prelude::*,
        path::{Path, PathBuf}
    },
    shaderc::{
        Compiler,
        CompileOptions,
        IncludeType,
        IncludeCallbackResult,
        ResolvedInclude
    },
    convert_case::{Case, Casing},
    crate::{Result, Error},
};

const SHADER_DIR_NAME: &'static str = "gpu";

pub fn process_shader_srcs(src_path: &PathBuf, dir: &Path) -> Result<()> {
    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let path = entry.path();

        if path.is_file() {
            continue;
        }

        if let Some(dir_name) = path.components().last() {
            if dir_name.as_os_str().to_string_lossy() == SHADER_DIR_NAME {
                process_shader_dir(src_path, &path)?;
                break;
            }
        }
    }

    Ok(())
}

fn process_shader_dir(src_path: &PathBuf, dir: &Path) -> Result<()> {
    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let path = entry.path();

        if path.is_dir() {
            process_shader_dir(src_path, &path)?;
        } else {
            compile_shader(src_path, &path)?;
        }
    }

    Ok(())
}

fn compile_shader(src_path: &PathBuf, file_path: &Path) -> Result<()> {
    let mut options = CompileOptions::new()
        .ok_or(Error::Internal("shader compile options allocation failure".to_string()))?;

    options.set_include_callback(include_callback(src_path));

    let source_language;
    if let Some(ext) = file_path.extension() {
        if ext == "glsl" {
            source_language = shaderc::SourceLanguage::GLSL;
        } else if ext == "hlsl" {
            source_language = shaderc::SourceLanguage::HLSL;
        } else {
            return Err(
                Error::ShaderFile(
                    format!(
                        "the shader {} has unknown extension {}",
                        file_path.display(), ext.to_string_lossy()
                    )
                )
            )
        }
    } else {
        return Err(
            Error::ShaderFile(
                format!(
                    "the shader {} has no extension",
                    file_path.display()
                )
            )
        )
    }

    options.set_source_language(source_language);
    // TODO options.set_optimization_level(level)

    let source_text = fs::read_to_string(file_path)?;
    let shader_kind = shaderc::ShaderKind::InferFromSource;
    let input_file_name = file_path.to_string_lossy();
    let entry_point_name = "main";

    let mut compiler = Compiler::new()
        .ok_or(Error::Internal("shader compiler allocation failure".to_string()))?;

    let spirv = compiler.compile_into_spirv(
        &source_text,
        shader_kind,
        &input_file_name,
        entry_point_name,
        Some(&options)
    )?;

    let binary_spirv = spirv.as_binary();
    let file_name = file_path
        .file_stem()
        .expect("shader file name")
        .to_str()
        .expect("shader file name str");

    let shader_ffi_dir = src_path
        .join("ffi")
        .join(SHADER_DIR_NAME);

    let shader_ffi_base = shader_ffi_dir.join(file_name);

    if !shader_ffi_dir.exists() {
        fs::create_dir(shader_ffi_dir)?;
    }

    let mut shader_ffi_header = shader_ffi_base.clone();
    shader_ffi_header.set_extension("h");

    println!("cargo:rerun-if-changed={}", shader_ffi_header.display());

    let do_not_modify_comment = format! {
r#"// This file generated automatically.
// DO NOT MODIFY IT MANUALLY!
// Original shader source path: file:///{shader_source_path}"#,
    shader_source_path = file_path.display()
};

    let header_guard = format!(
        "___FFI_SHADER_HEADER_{}_H___",
        file_name.to_case(Case::UpperSnake)
    );

    let shader_fn_decl = format!("uint32_t *{}()", file_name.to_case(Case::Snake));

    let spirv_binary_hex = binary_spirv.iter()
        .map(|word| format!("{:#010X}", word))
        .collect::<Vec<_>>()
        .join(",\n\t\t");

    let shader_ffi_header_content = format! {
r#"{do_not_modify_comment}

#ifndef {header_guard}
#define {header_guard}

#include <stdint.h>

{shader_fn_decl};

#endif // {header_guard}"#,
    do_not_modify_comment = do_not_modify_comment,
    header_guard = header_guard,
    shader_fn_decl = shader_fn_decl,
};

    let shader_ffi_src_content = format! {
r#"{do_not_modify_comment}

#include "{header_file_path}"

{shader_fn_decl} {{
    static uint32_t shader_src[] = {{
        {spirv_binary}
    }};

    return shader_src;
}}"#,
    do_not_modify_comment = do_not_modify_comment,
    header_file_path = shader_ffi_header.display(),
    shader_fn_decl = shader_fn_decl,
    spirv_binary = spirv_binary_hex
};

    let mut out = fs::OpenOptions::new()
        .create(true)
        .write(true)
        .truncate(true)
        .open(shader_ffi_header.clone())?;

    out.write_all(shader_ffi_header_content.as_bytes())?;

    // Impl
    let mut shader_ffi_src = shader_ffi_base.clone();
    shader_ffi_src.set_extension("c");

    println!("cargo:rerun-if-changed={}", shader_ffi_src.display());

    let mut out = fs::OpenOptions::new()
        .create(true)
        .write(true)
        .truncate(true)
        .open(shader_ffi_src)?;

    out.write_all(shader_ffi_src_content.as_bytes())?;

    Ok(())
}

fn include_callback(src_path: &PathBuf)
    -> impl Fn(&str, IncludeType, &str, usize) -> IncludeCallbackResult
{
    let src_path = src_path.clone();
    move |requested_source, include_type, requesting_source, _include_depth| {
        let header_path;
        let requested_source_path = Path::new(requested_source);
        let standard_path = src_path.join(requested_source_path);

        match include_type {
            IncludeType::Relative => {
                let requesting_source_path = Path::new(requesting_source);
                let requesting_dir_path = requesting_source_path.parent()
                    .ok_or(format!("{}: expected parent path", requested_source_path.display()))?;

                let relative_path = requesting_dir_path.join(requested_source_path);

                if relative_path.is_file() {
                    header_path = relative_path;
                } else if standard_path.is_file() {
                    header_path = standard_path.clone();
                } else {
                    return Err("relative header path is not found".to_string());
                }
            },
            IncludeType::Standard => if standard_path.is_file() {
                header_path = standard_path.clone();
            } else {
                return Err("standard header path is not found".to_string());
            }
        }

        let header_content = fs::read_to_string(header_path)
            .map_err(|err| err.to_string())?;

        let resolved_include = ResolvedInclude {
            resolved_name: standard_path.to_string_lossy().to_string(),
            content: header_content
        };

        Ok(resolved_include)
    }
}
