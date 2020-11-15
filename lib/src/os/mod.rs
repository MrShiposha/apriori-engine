use crate::{ffi, io};

#[cfg(target_os = "windows")]
pub mod windows;

#[cfg(target_os = "windows")]
pub use windows::Window;

pub struct WindowSize {
    pub width: i32,
    pub height: i32
}

pub struct WindowPosition {
    pub x: i32,
    pub y: i32
}

pub trait WindowMethods<Id: io::InputId> {
    fn show(&self);

    fn hide(&self);

    fn platform_handle(&self) -> ffi::Handle;

    fn input_handler(&self) -> &io::InputHandler<Id>;

    fn input_handler_mut(&mut self) -> &mut io::InputHandler<Id>;
}
