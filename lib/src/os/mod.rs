use {
    std::fmt,
    crate::{ffi, io}
};

#[cfg(target_os = "windows")]
pub mod windows;

#[cfg(target_os = "windows")]
pub use windows::Window;

#[derive(Debug)]
pub struct WindowSize {
    pub width: u16,
    pub height: u16
}

#[derive(Debug)]
pub struct WindowPosition {
    pub x: i16,
    pub y: i16
}

#[derive(Debug)]
pub enum WindowState {
    Close,
    Show,
    Hide,
    SizeChanged(WindowSize),
    PositionChanged(WindowPosition),
}

impl fmt::Display for WindowSize {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "(width = {}, height = {})",
            self.width, self.height
        )
    }
}

impl fmt::Display for WindowPosition {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "(x = {}, y = {})",
            self.x, self.y
        )
    }
}

pub trait WindowMethods<Id: io::InputId> {
    fn show(&self);

    fn hide(&self);

    fn platform_handle(&self) -> ffi::Handle;

    fn input_handler(&self) -> &io::InputHandler<Id>;

    fn input_handler_mut(&mut self) -> &mut io::InputHandler<Id>;

    fn handle_window_state<H>(&mut self, handler: H)
    where
        H: FnMut(WindowState) + 'static;
}
