use {
    std::{
        collections::HashSet,
    },
    serde::{
        ser::{Serialize, Serializer, SerializeSeq},
        de::{self, Deserialize, Deserializer, Visitor, SeqAccess}
    },
    bitflags::bitflags,
    itertools::Itertools,
};

#[derive(Debug, PartialEq, Eq, Hash, Clone, Copy, serde::Serialize, serde::Deserialize)]
pub enum VirtualKey {
    MouseLeft,
    MouseMiddle,
    MouseRight,
    MouseX1,
    MouseX2,
    Backspace,
    Tab,
    Enter,
    NumPadEnter,
    Shift,
    Ctrl,
    Alt,
    Cmd,

    OsCtrl,

    Pause,
    CapsLock,
    Escape,
    Space,

    PageUp,
    PageDown,
    End,
    Home,

    Up,
    Down,
    Left,
    Right,

    Insert,
    Delete,

    LeftWin,
    RightWin,

    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    Digit0,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
    NumPad0,
    NumPad1,
    NumPad2,
    NumPad3,
    NumPad4,
    NumPad5,
    NumPad6,
    NumPad7,
    NumPad8,
    NumPad9,
    Clear,
    Multiply,
    Add,
    Separator,
    Substract,
    Decimal,
    Divide,

    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    F13,
    F14,
    F15,
    F16,
    F17,
    F18,
    F19,
    F20,
    F21,
    F22,
    F23,
    F24,

    NumLock,
    ScrollLock,

    LeftShift,
    RightShift,

    LeftCtrl,
    RightCtrl,

    LeftAlt,
    RightAlt,

    // Original Equipment Manufacturer.
    // Used for miscellaneous characters; it can vary by keyboard.
    Oem1, // For the US standard keyboard, the ';:' key
    OemPlus, // For any country/region, the '+' key
    OemComma, // For any country/region, the ','
    OemMinus, // For any country/region, the '-' key
    OemPeriod, // For any country/region, the '.' key
    Oem2, // For the US standard keyboard, the '/?' key
    Oem3, // For the US standard keyboard, the '`~' key
    Oem4, // For the US standard keyboard, the '[{' key
    Oem5, // For the US standard keyboard, the '\|' key
    Oem6, // For the US standard keyboard, the ']}' key
    Oem7, // For the US standard keyboard, the 'single-quote/double-quote' key
    Oem8, // it can vary by keyboard
}

impl VirtualKey {
    pub fn is_general_mod(&self) -> bool {
        self.split_general_mod().is_some()
    }

    /// Splits `Ctrl`, `Alt` or `Shift` into left and right.
    pub fn split_general_mod(&self) -> Option<(Self, Self)> {
        match self {
            Self::Ctrl => Some((Self::LeftCtrl, Self::RightCtrl)),
            Self::Shift => Some((Self::LeftShift, Self::RightShift)),
            Self::Alt => Some((Self::LeftAlt, Self::RightAlt)),
            _ => None
        }
    }

    pub fn as_general_mod(&self) -> Option<Self> {
        match self {
            Self::Ctrl
            | Self::LeftCtrl
            | Self::RightCtrl => Some(Self::Ctrl),

            Self::Cmd => Some(Self::Cmd),

            #[cfg(target_os = "windows")]
            Self::OsCtrl => Some(Self::Ctrl),

            #[cfg(target_os = "macos")]
            Self::OsCtrl => Some(Self::Cmd),

            Self::Shift
            | Self::LeftShift
            | Self::RightShift => Some(Self::Shift),

            Self::Alt
            | Self::LeftAlt
            | Self::RightAlt => Some(Self::Alt),

            _ => None
        }
    }

    pub fn as_os_ctrl(&self) -> Option<Self> {
        match self.as_general_mod() {
            #[cfg(target_os = "windows")]
            Some(Self::Ctrl) => Some(Self::OsCtrl),

            #[cfg(target_os = "macos")]
            Some(Self::Cmd) => Some(Self::OsCtrl),

            _ => None
        }
    }

    /// Transforms OS specific keys into general keys
    pub fn normalized(&self) -> Self {
        match self {
            #[cfg(target_os = "windows")]
            Self::OsCtrl => Self::Ctrl,

            #[cfg(target_os = "macos")]
            Self::OsCtrl => Self::Cmd,

            _ => self.clone()
        }
    }

    pub fn as_key_mods(&self) -> Option<KeyMods> {
        self.as_general_mod()
            .map(|gen_mod| match gen_mod {
                Self::Ctrl => KeyMods::CTRL,
                Self::Cmd => KeyMods::CMD,

                #[cfg(target_os = "windows")]
                Self::OsCtrl => KeyMods::CTRL,

                #[cfg(target_os = "macos")]
                Self::OsCtrl => KeyMods::CMD,

                Self::Shift => KeyMods::SHIFT,
                Self::Alt => KeyMods::ALT,

                _ => unreachable!()
            })
    }
}

pub type KeyModsUnderlyingType = u32;

bitflags! {
    pub struct KeyMods: KeyModsUnderlyingType {
        const CTRL  = 0x1;
        const CMD   = 0x2;
        const SHIFT = 0x4;
        const ALT   = 0x8;
    }
}

impl Serialize for KeyMods {
    fn serialize<S>(&self, serializer: S) -> std::result::Result<<S as Serializer>::Ok, <S as Serializer>::Error>
    where S: Serializer {
        macro_rules! vec_mods {
            (let $vec:ident = { $(($mods:expr, $repr:expr)),+ $(,)? }) => {
                let mut $vec = vec![];

                $(
                    if self.contains($mods) {
                        $vec.push($repr);
                    }
                )+

                let $vec = $vec;
            };
        }

        vec_mods! {
            let mods = {
                (KeyMods::CTRL, VirtualKey::Ctrl),
                (KeyMods::CMD, VirtualKey::Cmd),
                (KeyMods::SHIFT, VirtualKey::Shift),
                (KeyMods::ALT, VirtualKey::Alt),
            }
        }

        let mut seq = serializer.serialize_seq(Some(mods.len()))?;

        for key_mod in mods {
            seq.serialize_element(&key_mod)?;
        }

        seq.end()
    }
}

impl<'de> Deserialize<'de> for KeyMods {
    fn deserialize<D>(deserializer: D) -> std::result::Result<Self, <D as Deserializer<'de>>::Error>
    where D: Deserializer<'de> {
        deserializer.deserialize_seq(KeyModsVisitor)
    }
}

struct KeyModsVisitor;

impl<'de> Visitor<'de> for KeyModsVisitor {
    type Value = KeyMods;

    fn visit_seq<A>(self, mut seq: A) -> Result<Self::Value, A::Error>
    where
        A: SeqAccess<'de>, {
        let mut mods = KeyMods::empty();

        let mut duplicated = HashSet::new();

        while let Some(key) = seq.next_element::<VirtualKey>()? {
            let key_mod = key.as_key_mods()
                .ok_or(de::Error::custom(format!("{:#?} is not modifier key", key)))?;

            if mods.contains(key_mod) {
                duplicated.insert(key);
            } else {
                mods |= key_mod;
            }
        }

        if duplicated.is_empty() {
            Ok(mods)
        } else {
            Err(
                de::Error::custom(
                    format!(
                        "duplicated modifiers ({})",
                        duplicated.iter()
                            .map(|key_mod| format!("{:#?}", key_mod))
                            .join(",")
                    )
                )
            )
        }
    }

    fn expecting(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "[<key modifier>, ...]")
    }
}