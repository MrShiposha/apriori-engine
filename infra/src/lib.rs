use std::{path::PathBuf, io, fmt, env};

pub mod shader;
pub mod ffi;

#[derive(Debug)]
pub enum Error {
    Io(io::Error),
    FilenameExpected(PathBuf),
    ParentDirExpected(PathBuf),
    Bindgen,
    ShaderFile(String),
    ShaderCompile(shaderc::Error),
    EnvVar(env::VarError),
    Internal(String),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Io(err) => write!(f, "io error: {}", err),
            Self::FilenameExpected(path) => write!(f, "{}: file name expected", path.display()),
            Self::ParentDirExpected(path) => write!(f, "{}: parent directory expected", path.display()),
            Self::Bindgen => write!(f, "c bindings generation error"),
            Self::ShaderFile(err) => write!(f, "shader error: {}", err),
            Self::ShaderCompile(err) => write!(f, "shader compiler error: {}", err),
            Self::EnvVar(err) => write!(f, "env variable error: {}", err),
            Self::Internal(err) => write!(f, "internal error: {}", err),
        }
    }
}

impl From<io::Error> for Error {
    fn from(err: io::Error) -> Self {
        Self::Io(err)
    }
}

impl From<shaderc::Error> for Error {
    fn from(err: shaderc::Error) -> Self {
        Self::ShaderCompile(err)
    }
}

impl From<env::VarError> for Error {
    fn from(err: env::VarError) -> Self {
        Self::EnvVar(err)
    }
}

pub type Result<T> = std::result::Result<T, Error>;

pub fn project_build(
    src_path: PathBuf,
    include_dirs: Vec<PathBuf>,
    libraries: Vec<PathBuf>,
) -> Result<()> {
    shader::process_shader_srcs(&src_path, &src_path)?;

    let mut cc_build = cc::Build::new();
    cc_build.includes(include_dirs.clone())
        .warnings_into_errors(true);

    if cfg!(target_os = "windows") {
        cc_build.define("___windows___", None)
            .define("VK_USE_PLATFORM_WIN32_KHR", None);
    } else if cfg!(target_os = "macos") {
        cc_build.define("___macos___", None)
            .define("VK_USE_PLATFORM_MACOS_MVK", None);
    } else if cfg!(target_os = "linux") {
        cc_build.define("___linux___", None);
    } else {
        cc_build.define("___unknown___", None);
    }

    if cfg!(debug_assertions) {
        cc_build.define("___debug___", None);
    } else {
        cc_build.define("___release___", None);
    }

    ffi::process_c_srcs(&src_path, &include_dirs, &mut cc_build)?;

    cc_build.compile("apriori2.c.ffi");

    for lib in libraries {
        let lib_name = lib.file_stem()
            .ok_or(Error::FilenameExpected(lib.clone()))?;
        let lib_dir = lib
            .parent()
            .ok_or(Error::ParentDirExpected(lib.clone()))?;

        println!("cargo:rustc-link-search=native={}", lib_dir.display());
        println!("cargo:rustc-link-lib=static={}", lib_name.to_string_lossy());
    }

    Ok(())
}