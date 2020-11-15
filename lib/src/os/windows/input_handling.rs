use {
    winapi::{
        shared::{
            minwindef::{
                FALSE,
                UINT,
                USHORT,
                DWORD,
                WPARAM,
                LPARAM,
                LRESULT,
                LPVOID,
            },
            windef::{
                HWND,
            },
            basetsd::LONG_PTR,
        },
        um::{
            errhandlingapi::{
                SetLastError,
                GetLastError
            },
            winuser::*
        }
    },
    crate::{
        core::Result,
        os,
        io::*,
    }
};

const LOG_TARGET: &'static str = "Windows Window Callback";

// https://docs.microsoft.com/en-us/windows-hardware/drivers/hid/hid-usages#usage-page
const GENERIC_DESKTOP_CONTROLS: USHORT = 0x01;

// https://docs.microsoft.com/en-us/windows-hardware/drivers/hid/hid-usages#usage-id
const HID_USAGE_GENERIC_MOUSE: USHORT = 0x02;
const HID_USAGE_GENERIC_KEYBOARD: USHORT = 0x06;

pub unsafe extern "system" fn window_cb<Id: InputId>(
    hwnd: HWND,
    msg: UINT,
    wparam: WPARAM,
    lparam: LPARAM
) -> LRESULT {
    match window_cb_inner::<Id>(hwnd, msg, wparam, lparam) {
        Ok(Some(result)) => result,
        Ok(None) => DefWindowProcW(hwnd, msg, wparam, lparam),
        Err(err) => {
            log::error! {
                target: LOG_TARGET,
                "{}", err
            };

            DefWindowProcW(hwnd, msg, wparam, lparam)
        }
    }


}

unsafe fn window_cb_inner<Id: InputId>(
    hwnd: HWND,
    msg: UINT,
    _wparam: WPARAM,
    lparam: LPARAM
) -> Result<Option<LRESULT>> {
    if msg == WM_NCCREATE {
        // Why we call SetWindowLongPtrW here:
        // https://devblogs.microsoft.com/oldnewthing/20191014-00/?p=102992

        let win_create  = &mut *(lparam as LPCREATESTRUCTW);
        let input_handler = win_create.lpCreateParams as *mut InputHandler<Id>;

        // See SetWindowLongPtrW docs
        // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowlongptrw#return-value
        // (About SetLastError and SetWindowLongPtrW return value)

        SetLastError(0);

        let result = SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            input_handler as LONG_PTR
        );

        let last_error = GetLastError();

        if result == 0 && last_error != 0 {
            return Err(os::windows::last_error("set window long ptr"));
        }

        init_raw_input(hwnd)?;
    }

    let window_long_ptr = GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if window_long_ptr == 0 {
        log::debug! {
            target: LOG_TARGET,
            "get window long ptr"
        }
        return Ok(None);
    }

    let input_handler = &mut *(window_long_ptr as *mut InputHandler<Id>);
    let mods = input_handler.aux.mods;

    match msg {
        WM_INPUT => {
            let mut input_size: UINT = 0;

            let result = GetRawInputData(
                lparam as HRAWINPUT,
                RID_INPUT,
                std::ptr::null_mut(),
                &mut input_size,
                std::mem::size_of::<RAWINPUTHEADER>() as UINT
            );

            if result != 0 {
                return Err(os::windows::last_error("get raw input data (get input size)"));
            }

            let mut bytes = vec![0u8; input_size as usize];

            let size = GetRawInputData(
                lparam as HRAWINPUT,
                RID_INPUT,
                bytes.as_mut_ptr() as LPVOID,
                &mut input_size,
                std::mem::size_of::<RAWINPUTHEADER>() as UINT
            );

            if size != input_size {
                return Err(os::windows::last_error("get raw input data (invalid size)"));
            }

            let input = &mut *(bytes.as_mut_ptr() as *mut RAWINPUT);

            if input.header.dwType == RIM_TYPEKEYBOARD {
                const FAKE_KEY: USHORT = 0xFF;

                let keyboard = input.data.keyboard();
                let scan_code = keyboard.MakeCode;
                let key = keyboard.VKey;
                let flags = keyboard.Flags;

                if key == FAKE_KEY {
                    return Ok(None);
                }

                let is_e0 = (flags as DWORD & RI_KEY_E0) != 0;

                if let Some(key) = vkey(key, scan_code, is_e0) {
                    let is_down = (flags as DWORD & RI_KEY_BREAK) == 0;

                    let event = if is_down {
                        InputEvent::Pressed
                    } else {
                        InputEvent::Released
                    };

                    let mods = match key.as_key_mods() {
                        Some(key_mods) => {
                            input_handler.aux.mods.set(key_mods, is_down);
                            mods & !key_mods
                        },
                        None => mods
                    };

                    input_handler.run_action_handler(
                        Action::new(key, mods)?,
                        event
                    );
                }
            } else if input.header.dwType == RIM_TYPEMOUSE {
                let mouse = input.data.mouse();

                if mouse.usFlags & MOUSE_MOVE_RELATIVE == MOUSE_MOVE_RELATIVE {
                    let x = mouse.lLastX;
                    let y = mouse.lLastY;

                    if x != 0 {
                        input_handler.run_axis_handler(
                            Axis::with_unit_scale(AxisId::MousePositionX, mods),
                            InputEvent::Axis(x as AxisValue)
                        );
                    }

                    if y != 0 {
                        input_handler.run_axis_handler(
                            Axis::with_unit_scale(AxisId::MousePositionY, mods),
                            InputEvent::Axis(y as AxisValue)
                        );
                    }
                }

                macro_rules! match_mouse_btn {
                    (
                        $first_variant:expr => $first_block:block
                        $($variant:expr => $block:block)*
                    ) => {
                        let btn_flags = mouse.usButtonFlags;

                        if btn_flags & $first_variant == $first_variant $first_block
                        $(else if btn_flags & $variant == $variant $block)*
                    };
                }

                match_mouse_btn! {
                    RI_MOUSE_WHEEL => {
                        let wheel_delta = mouse.usButtonData as AxisValue;
                        let wheel_ticks = wheel_delta / WHEEL_DELTA as AxisValue;

                        input_handler.run_axis_handler(
                            Axis::with_unit_scale(AxisId::MouseWheel, mods),
                            InputEvent::Axis(wheel_ticks)
                        );
                    }
                    RI_MOUSE_LEFT_BUTTON_DOWN => {
                        input_handler.run_action_handler(
                            Action::new(VirtualKey::MouseLeft, mods)?,
                            InputEvent::Pressed
                        );
                    }
                    RI_MOUSE_LEFT_BUTTON_UP => {
                        input_handler.run_action_handler(
                            Action::new(VirtualKey::MouseLeft, mods)?,
                            InputEvent::Released
                        );
                    }
                    RI_MOUSE_MIDDLE_BUTTON_DOWN => {
                        input_handler.run_action_handler(
                            Action::new(VirtualKey::MouseMiddle, mods)?,
                            InputEvent::Pressed
                        );
                    }
                    RI_MOUSE_MIDDLE_BUTTON_UP => {
                        input_handler.run_action_handler(
                            Action::new(VirtualKey::MouseMiddle, mods)?,
                            InputEvent::Released
                        );
                    }
                    RI_MOUSE_RIGHT_BUTTON_DOWN => {
                        input_handler.run_action_handler(
                            Action::new(VirtualKey::MouseRight, mods)?,
                            InputEvent::Pressed
                        );
                    }
                    RI_MOUSE_RIGHT_BUTTON_UP => {
                        input_handler.run_action_handler(
                            Action::new(VirtualKey::MouseRight, mods)?,
                            InputEvent::Released
                        );
                    }
                    RI_MOUSE_BUTTON_4_DOWN => {
                        input_handler.run_action_handler(
                            Action::new(VirtualKey::MouseX1, mods)?,
                            InputEvent::Pressed
                        );
                    }
                    RI_MOUSE_BUTTON_4_UP => {
                        input_handler.run_action_handler(
                            Action::new(VirtualKey::MouseX1, mods)?,
                            InputEvent::Released
                        );
                    }
                    RI_MOUSE_BUTTON_5_DOWN => {
                        input_handler.run_action_handler(
                            Action::new(VirtualKey::MouseX2, mods)?,
                            InputEvent::Pressed
                        );
                    }
                    RI_MOUSE_BUTTON_5_UP => {
                        input_handler.run_action_handler(
                            Action::new(VirtualKey::MouseX2, mods)?,
                            InputEvent::Released
                        );
                    }
                }
            }

            return Ok(Some(FALSE as LRESULT))
        }
        WM_DESTROY => {
            PostQuitMessage(0);
        },
        _ => {}
    }

    Ok(None)
}

fn vkey(key: USHORT, scan_code: USHORT, is_e0: bool) -> Option<VirtualKey> {
    // See https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes

    use VirtualKey::*;

    let key = match key as i32 {
        VK_CONTROL => if is_e0 {
            RightCtrl
        } else {
            LeftCtrl
        }
        VK_MENU => if is_e0 {
            RightAlt
        } else {
            LeftAlt
        }
        VK_SHIFT => match unsafe {
            MapVirtualKeyW(scan_code as UINT, MAPVK_VSC_TO_VK_EX) as i32
        } {
            VK_LSHIFT => LeftShift,
            VK_RSHIFT => RightShift,
            _ => unreachable!()
        }
        VK_RETURN => if is_e0 {
            NumPadEnter
        } else {
            Enter
        }
        VK_INSERT => if is_e0 {
            Insert
        } else {
            NumPad0
        }
        VK_DELETE => if is_e0 {
            Delete
        } else {
            Decimal
        }
        VK_HOME => if is_e0 {
            Home
        } else {
            NumPad7
        }
        VK_END => if is_e0 {
            End
        } else {
            NumPad1
        }
        VK_PRIOR => if is_e0 {
            PageUp
        } else {
            NumPad9
        }
        VK_NEXT => if is_e0 {
            PageDown
        } else {
            NumPad3
        }
        VK_LEFT => if is_e0 {
            Left
        } else {
            NumPad4
        }
        VK_RIGHT => if is_e0 {
            Right
        } else {
            NumPad6
        }
        VK_UP => if is_e0 {
            Up
        } else {
            NumPad8
        }
        VK_DOWN => if is_e0 {
            Down
        } else {
            NumPad2
        }
        VK_CLEAR => if is_e0 {
            Clear
        } else {
            NumPad5
        }
        VK_BACK => Backspace,
        VK_TAB => Tab,
        VK_PAUSE => Pause,
        VK_CAPITAL => CapsLock,
        VK_ESCAPE => Escape,
        VK_SPACE => Space,
        VK_LWIN => LeftWin,
        VK_RWIN => RightWin,
        0x30 => Digit0,
        0x31 => Digit1,
        0x32 => Digit2,
        0x33 => Digit3,
        0x34 => Digit4,
        0x35 => Digit5,
        0x36 => Digit6,
        0x37 => Digit7,
        0x38 => Digit8,
        0x39 => Digit9,
        0x41 => A,
        0x42 => B,
        0x43 => C,
        0x44 => D,
        0x45 => E,
        0x46 => F,
        0x47 => G,
        0x48 => H,
        0x49 => I,
        0x4A => J,
        0x4B => K,
        0x4C => L,
        0x4D => M,
        0x4E => N,
        0x4F => O,
        0x50 => P,
        0x51 => Q,
        0x52 => R,
        0x53 => S,
        0x54 => T,
        0x55 => U,
        0x56 => V,
        0x57 => W,
        0x58 => X,
        0x59 => Y,
        0x5A => Z,
        VK_MULTIPLY => Multiply,
        VK_ADD => Add,
        VK_SEPARATOR => Separator,
        VK_SUBTRACT => Substract,
        VK_DIVIDE => Divide,
        VK_F1 => F1,
        VK_F2 => F2,
        VK_F3 => F3,
        VK_F4 => F4,
        VK_F5 => F5,
        VK_F6 => F6,
        VK_F7 => F7,
        VK_F8 => F8,
        VK_F9 => F9,
        VK_F10 => F10,
        VK_F11 => F11,
        VK_F12 => F12,
        VK_F13 => F13,
        VK_F14 => F14,
        VK_F15 => F15,
        VK_F16 => F16,
        VK_F17 => F17,
        VK_F18 => F18,
        VK_F19 => F19,
        VK_F20 => F20,
        VK_F21 => F21,
        VK_F22 => F22,
        VK_F23 => F23,
        VK_F24 => F24,
        VK_NUMLOCK => NumLock,
        VK_SCROLL => ScrollLock,
        VK_OEM_1 => VirtualKey::Oem1,
        VK_OEM_PLUS => VirtualKey::OemPlus,
        VK_OEM_COMMA => VirtualKey::OemComma,
        VK_OEM_MINUS => VirtualKey::OemMinus,
        VK_OEM_PERIOD => VirtualKey::OemPeriod,
        VK_OEM_2 => VirtualKey::Oem2,
        VK_OEM_3 => VirtualKey::Oem3,
        VK_OEM_4 => VirtualKey::Oem4,
        VK_OEM_5 => VirtualKey::Oem5,
        VK_OEM_6 => VirtualKey::Oem6,
        VK_OEM_7 => VirtualKey::Oem7,
        VK_OEM_8 => VirtualKey::Oem8,
        _ => return None
    };

    Some(key)
}

// fn repeat_event(lparam: LPARAM) -> Option<InputEvent> {
//     let repeat_count = lparam & 0xFFFF;

//     if repeat_count == 1 {
//         None
//     } else {
//         Some(InputEvent::Repeat(repeat_count as u16))
//     }
// }

fn init_raw_input(hwnd: HWND) -> Result<()> {
    let mouse = RAWINPUTDEVICE {
        usUsagePage: GENERIC_DESKTOP_CONTROLS,
        usUsage: HID_USAGE_GENERIC_MOUSE,
        dwFlags: 0,
        hwndTarget: hwnd
    };

    let keyboard = RAWINPUTDEVICE {
        usUsagePage: GENERIC_DESKTOP_CONTROLS,
        usUsage: HID_USAGE_GENERIC_KEYBOARD,
        dwFlags: RIDEV_NOLEGACY,
        hwndTarget: hwnd
    };

    let mut devices = vec![mouse, keyboard];

    unsafe {
        let result = RegisterRawInputDevices(
            devices.as_mut_ptr(),
            devices.len() as UINT,
            std::mem::size_of::<RAWINPUTDEVICE>() as UINT
        );

        if result == FALSE {
            return Err(os::windows::last_error("raw input devices"));
        }
    }

    Ok(())
}