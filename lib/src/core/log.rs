use {
    std::ffi::{c_void, CStr},
    libc::c_char,
    log::{log, error},
    printf::printf
};

const LOG_TARGET: &'static str = "apriori-log";

#[no_mangle]
extern "C" fn ffi_log(level: *const c_char, target: *const c_char, format: *const c_char, args: *mut c_void) {
    macro_rules! as_str {
        ($str:expr) => {
            unsafe {
                match CStr::from_ptr($str).to_str() {
                    Ok(value) => value,
                    Err(err) => {
                        error! {
                            target: LOG_TARGET,
                            "FFI log error while converting \"{}\" to &str -- {}",
                            stringify![$str],
                            err
                        };

                        return;
                    }
                }
            }
        };
    }

    let level: log::Level = match as_str![level].parse() {
        Ok(lvl) => lvl,
        Err(err) => {
                error! {
                target: LOG_TARGET,
                "FFI log error while parsing log level -- {}",
                err
            };

            return;
        }
    };

    let target = as_str![target];

    let message = unsafe {
        printf(format, args)
    };

    log! {
        target: target,
        level,
        "{}", message
    };
}
