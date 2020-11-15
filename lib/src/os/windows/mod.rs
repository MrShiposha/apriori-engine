use {
    std::{
        os::windows::ffi::{OsStrExt, OsStringExt},
        ffi::{OsStr, OsString},
        pin::Pin,
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

pub struct Window<Id: io::InputId> {
    hwnd: HWND,
    handler: Pin<Box<io::InputHandler<Id>>>,
}

impl<Id: io::InputId> Window<Id> {
    pub fn new(
        title: &str,
        size: WindowSize,
        position: WindowPosition
    ) -> Result<Self> {
        let mut window_class_name = OsStr::new(WINDOW_CLASS_NAME)
            .encode_wide().collect::<Vec<u16>>();

        // Add '\0' at the end
        window_class_name.push(0);

        let mut window_title = OsStr::new(title)
            .encode_wide().collect::<Vec<u16>>();

        // Add '\0' at the end
        window_title.push(0);

        let hwnd;
        let mut handler;
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

            handler = Pin::new(Box::new(io::InputHandler::new()));
            let handler_ptr = &mut *handler as *mut _;

            hwnd = CreateWindowExW(
                0,
                window_class_name.as_ptr(),
                window_title.as_ptr(),
                WS_OVERLAPPEDWINDOW,
                position.x,
                position.y,
                size.width,
                size.height,
                0 as HWND,
                0 as HMENU,
                0 as HINSTANCE,
                handler_ptr as LPVOID
            );

            if hwnd == (0 as HWND) {
                return Err(last_error("window creation failure"));
            }
        }

        io::WINDOWS.write()?.push(hwnd.into());

        let wnd = Self {
            hwnd,
            handler,
        };

        Ok(wnd)
    }
}

impl<Id: io::InputId> WindowMethods<Id> for Window<Id> {
    fn show(&self) {
        unsafe {
            ShowWindow(self.hwnd, SW_SHOW);
        }
    }

    fn hide(&self) {
        unsafe {
            ShowWindow(self.hwnd, SW_HIDE);
        }
    }

    fn platform_handle(&self) -> ffi::Handle {
        self.hwnd as ffi::Handle
    }

    fn input_handler(&self) -> &io::InputHandler<Id> {
        &self.handler
    }

    fn input_handler_mut(&mut self) -> &mut io::InputHandler<Id> {
        &mut self.handler
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