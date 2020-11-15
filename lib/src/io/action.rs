use {
    serde::{Serialize, Deserialize},
    crate::{
        core::{Result, Error},
        io::*
    }
};

#[derive(Debug, PartialEq, Eq, Hash, Clone, Serialize, Deserialize)]
pub struct Action {
    key: VirtualKey,
    mods: KeyMods
}

impl Action {
    pub fn new(key: VirtualKey, mods: KeyMods) -> Result<Self> {
        if key.as_key_mods()
            .map(|mod_key| mods.contains(mod_key))
            .unwrap_or(false) {
            return Err(Error::KeyAndModifierMatch(key));
        }

        let action = Self {
            key,
            mods
        };

        Ok(action)
    }

    /// Transforms OS specific keys to general keys
    pub fn normalized(&self) -> Self {
        Self {
            key: self.key.normalized(),
            ..*self
        }
    }

    pub fn key(&self) -> VirtualKey {
        self.key
    }

    pub fn mods(&self) -> KeyMods {
        self.mods
    }
}
