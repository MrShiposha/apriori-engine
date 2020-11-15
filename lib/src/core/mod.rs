pub mod vulkan_instance;
pub mod log;

use {
    std::{
        fmt,
        sync::PoisonError,
    },
    crate::{ffi, io},
};

pub use vulkan_instance::VulkanInstance;

#[derive(Debug)]
pub enum Error {
    Apriori2FFI(ffi::Apriori2Error),
    OsSpecific(String),
    KeyAndModifierMatch(io::VirtualKey),
    Sync(String),
    Serialization(String),
    Io(std::io::Error),
}

impl From<ffi::Apriori2Error> for Error {
    fn from(err: ffi::Apriori2Error) -> Self {
        Self::Apriori2FFI(err)
    }
}

impl From<std::io::Error> for Error {
    fn from(err: std::io::Error) -> Self {
        Self::Io(err)
    }
}

impl From<ron::error::Error> for Error {
    fn from(err: ron::error::Error) -> Self {
        Self::Serialization(err.to_string())
    }
}

impl<T> From<PoisonError<T>> for Error {
    fn from(err: PoisonError<T>) -> Self {
        Self::Sync(err.to_string())
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Apriori2FFI(err) => write!(f, "FFI error code = {}", err), // TODO string description
            Self::OsSpecific(err) => write!(f, "(OS) {}", err),
            Self::KeyAndModifierMatch(key) => {
                write!(f, "{:#?} - key and modifier are same", key)
            },
            Self::Sync(err) => write!(f, "{}", err),
            Self::Serialization(err) => write!(f, "{}", err),
            Self::Io(err) => write!(f, "(io error) {}", err),
        }
    }
}

impl std::error::Error for Error {}

pub type Result<T> = std::result::Result<T, Error>;

impl ffi::Result {
    pub fn try_unwrap<T>(&self) -> Result<*mut T> {
        let result;
        unsafe {
            if self.error == ffi::Apriori2Error_SUCCESS {
                result = std::mem::transmute::<ffi::Handle, *mut T>(self.object);
            } else {
                return Err(self.error.into())
            }
        }

        Ok(result)
    }
}

pub struct AssumeThreadSafe<T>(T);

unsafe impl<T> std::marker::Sync for AssumeThreadSafe<T> {}

unsafe impl<T> std::marker::Send for AssumeThreadSafe<T> {}

impl<T> From<T> for AssumeThreadSafe<T> {
    fn from(v: T) -> Self {
        Self(v)
    }
}

impl<T> std::ops::Deref for AssumeThreadSafe<T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}
