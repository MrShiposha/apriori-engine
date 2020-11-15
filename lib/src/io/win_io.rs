use {
    std::sync::{RwLock},
    winapi::{
        shared::windef::HWND,
        um::winuser::*
    },
    lazy_static::lazy_static,
    crate::core::{Result, AssumeThreadSafe},
};

lazy_static! {
    static ref IS_IO_ACTIVE: RwLock<bool> = RwLock::new(true);
    pub(crate) static ref WINDOWS: RwLock<Vec<AssumeThreadSafe<HWND>>> = RwLock::new(vec![]);
}

pub fn execute() -> Result<()> {
    let mut msg: MSG = unsafe {
        std::mem::zeroed()
    };

    while is_active()? {
        for hwnd in WINDOWS.read()?.iter() {
            unsafe {
                if PeekMessageW(
                    &mut msg,
                    **hwnd,
                    0,
                    0,
                    PM_REMOVE
                ) > 0 {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
            }
        }
    }

    Ok(())
}

pub fn stop() -> Result<()> {
    let mut is_active = IS_IO_ACTIVE.write()?;
    *is_active = false;

    Ok(())
}

pub fn is_active() -> Result<bool> {
    IS_IO_ACTIVE.read()
        .map(|v| *v)
        .map_err(|err| err.into())
}