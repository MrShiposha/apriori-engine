use super::{ffi, Result};

pub struct VulkanInstance {
    pub instance_ffi: ffi::VulkanInstance
}

impl VulkanInstance {
    const LOG_TARGET: &'static str = "Rust/VulkanInstance";

    pub fn new() -> Result<Self> {
        log::trace! {
            target: Self::LOG_TARGET,
            "creating new vulkan instance..."
        }

        let instance;
        unsafe  {
            instance = Self {
                instance_ffi: ffi::new_vk_instance().try_unwrap()?
            };
        }

        log::trace! {
            target: Self::LOG_TARGET,
            "new vulkan instance successfully created"
        }

        Ok(instance)
    }
}

impl Drop for VulkanInstance {
    fn drop(&mut self) {
        unsafe {
            ffi::drop_vk_instance(self.instance_ffi);
        }
    }
}