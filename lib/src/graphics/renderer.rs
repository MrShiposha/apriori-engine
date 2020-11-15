use crate::{
    ffi,
    os::{self, WindowMethods},
    core::{Result, VulkanInstance},
    io,
};

pub struct Renderer {
    renderer_ffi: ffi::Renderer
}

impl Renderer {
    pub fn new<Id: io::InputId>(
        vk_instance: &VulkanInstance,
        window: &os::Window<Id>,
    ) -> Result<Self> {
        let renderer;
        unsafe {
            renderer = Self {
                renderer_ffi: ffi::new_renderer(
                    vk_instance.instance_ffi,
                    window.platform_handle()
                ).try_unwrap()?
            }
        }

        Ok(renderer)
    }
}

impl Drop for Renderer {
    fn drop(&mut self) {
        unsafe  {
            ffi::drop_renderer(self.renderer_ffi);
        }
    }
}