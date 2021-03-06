use {
    std::{
        os::windows::ffi::{OsStrExt, OsStringExt},
        ffi::{OsStr, OsString},
        pin::Pin,
        marker::Unpin,
    },
    winapi::{
        shared::{
            ntdef::{
                LPCWSTR,
                WCHAR,
                MAKELANGID,
                LANG_NEUTRAL,
                SUBLANG_DEFAULT,
            },
            minwindef::{
                HINSTANCE,
                DWORD,
                LPVOID,
            },
            windef::{
                HWND,
                HICON,
                HBRUSH,
                HMENU,
                RECT,
            }
        },
        um::{
            winbase::*,
            winuser::*,
            errhandlingapi::GetLastError,
        }
    },
    crate::{
        core::{Result, Error},
        os::*,
        io,
    }
};

mod input_handling;

const WINDOW_CLASS_NAME: &'static str = "Apriori2WindowClass";

pub(crate) struct WindowInternalInfo<Id: io::InputId> {
    pub(crate) input_handler: io::InputHandler<Id>,
    pub(crate) state_handler: Box<dyn FnMut(WindowState)>,
    pub(crate) is_dropped: bool,
}

impl<Id: io::InputId> Unpin for WindowInternalInfo<Id> {}

pub struct Window<Id: io::InputId> {
    hwnd: HWND,
    internal: Pin<Box<WindowInternalInfo<Id>>>,
}

impl<Id: io::InputId> Window<Id> {
    const LOG_TARGET: &'static str = "Window";

    pub fn new(
        title: &str,
        size: WindowSize,
        position: WindowPosition
    ) -> Result<Self> {
        log::info! {
            target: Self::LOG_TARGET,
            "creating new window..."
        };

        log::trace! {
            target: Self::LOG_TARGET,
            "\twindow class name: \"{}\"",
            WINDOW_CLASS_NAME
        };

        log::trace! {
            target: Self::LOG_TARGET,
            "\twindow title: \"{}\"",
            title
        };

        log::trace! {
            target: Self::LOG_TARGET,
            "\twindow size: \"{}\"",
            size
        };

        log::trace! {
            target: Self::LOG_TARGET,
            "\twindow position: \"{}\"",
            position
        };

        let mut window_class_name = OsStr::new(WINDOW_CLASS_NAME)
            .encode_wide().collect::<Vec<u16>>();

        // Add '\0' at the end
        window_class_name.push(0);

        let mut window_title = OsStr::new(title)
            .encode_wide().collect::<Vec<u16>>();

        // Add '\0' at the end
        window_title.push(0);

        let hwnd;
        let mut internal;
        unsafe {
            let window_class = WNDCLASSW {
                style: 0,
                cbClsExtra: 0,
                cbWndExtra: 0,
                hInstance: 0 as HINSTANCE,
                hIcon: 0 as HICON,
                hCursor: LoadCursorW(std::ptr::null_mut(), IDC_CROSS),
                hbrBackground: 0x10 as HBRUSH,
                lpszMenuName: 0 as LPCWSTR,
                lpfnWndProc: Some(input_handling::window_cb::<Id>),
                lpszClassName: window_class_name.as_ptr(),
            };

            if RegisterClassW(&window_class) == 0 {
                return Err(last_error("window class registration failure"));
            }

            let input_handler = io::InputHandler::new();
            let state_handler = Box::new(|_| { /* do nothing by default */ });

            internal = Pin::new(
                Box::new(
                    WindowInternalInfo {
                        input_handler,
                        state_handler,
                        is_dropped: false,
                    }
                )
            );

            let internal_ptr = &mut *internal as *mut _;

            hwnd = CreateWindowExW(
                0,
                window_class_name.as_ptr(),
                window_title.as_ptr(),
                WS_OVERLAPPEDWINDOW,
                position.x as i32,
                position.y as i32,
                size.width as i32,
                size.height as i32,
                0 as HWND,
                0 as HMENU,
                0 as HINSTANCE,
                internal_ptr as LPVOID
            );

            if hwnd == (0 as HWND) {
                return Err(last_error("window creation failure"));
            }
        }

        io::WINDOWS.write()?.push(hwnd.into());

        let wnd = Self {
            hwnd,
            internal
        };

        log::info! {
            target: Self::LOG_TARGET,
            "new window created successfully"
        };

        Ok(wnd)
    }
}

impl<Id: io::InputId> Drop for Window<Id> {
    fn drop(&mut self) {
        unsafe {
            if !self.internal.is_dropped {
                if DestroyWindow(self.hwnd) == 0 {
                    log::error! {
                        target: Self::LOG_TARGET,
                        "{}", last_error("window destroy")
                    }
                }
            }
        }

        log::debug! {
            target: Self::LOG_TARGET,
            "drop window"
        }
    }
}

impl<Id: io::InputId> WindowMethods<Id> for Window<Id> {
    fn show(&self) {
        log::trace! {
            target: Self::LOG_TARGET,
            "show window"
        }

        unsafe {
            ShowWindow(self.hwnd, SW_SHOW);
        }
    }

    fn hide(&self) {
        log::trace! {
            target: Self::LOG_TARGET,
            "hide window"
        }

        unsafe {
            ShowWindow(self.hwnd, SW_HIDE);
        }
    }

    fn size(&self) -> Result<WindowSize> {
        let size;
        unsafe {
            let mut rect: RECT = std::mem::zeroed();

            if GetWindowRect(self.hwnd, &mut rect) == 0 {
                return Err(last_error("get window size"));
            }

            size = WindowSize {
                width: (rect.right - rect.left).abs() as u16,
                height: (rect.bottom - rect.top).abs() as u16
            }
        }

        Ok(size)
    }

    fn platform_handle(&self) -> ffi::Handle {
        self.hwnd as ffi::Handle
    }

    fn input_handler(&self) -> &io::InputHandler<Id> {
        &self.internal.input_handler
    }

    fn input_handler_mut(&mut self) -> &mut io::InputHandler<Id> {
        &mut self.internal.input_handler
    }

    fn handle_window_state<H>(&mut self, handler: H)
    where
        H: FnMut(WindowState) + 'static
    {
        self.internal.state_handler = Box::new(handler);
    }
}

pub fn last_error(error_kind: &str) -> Error {
    let mut buffer = vec![0 as WCHAR; 1024];

    unsafe {
        FormatMessageW(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            std::ptr::null_mut(),
            GetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT) as DWORD,
            buffer.as_mut_ptr(),
            buffer.len() as DWORD,
            std::ptr::null_mut()
        );
    }

    let null_idx = buffer.as_slice().iter().position(|&c| c == '\r' as WCHAR).unwrap();
    buffer.truncate(null_idx);

    let description = OsString::from_wide(buffer.as_slice());
    let description = description.to_string_lossy().to_string();

    let description = format!("{} -- {}", error_kind, description);

    Error::OsSpecific(description)
}