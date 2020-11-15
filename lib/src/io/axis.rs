use {
    std::{
        cmp::{PartialEq, Eq},
        hash::{Hash, Hasher}
    },
    serde::{Serialize, Deserialize},
    crate::io::*
};

pub type AxisScale = f32;
pub type AxisValue = f32;

#[derive(Debug, PartialEq, Eq, Hash, Clone, Copy, Serialize, Deserialize)]
pub enum AxisId {
    Key(VirtualKey),
    MousePositionX,
    MousePositionY,
    MouseWheel,
}

impl AxisId {
    /// Transforms OS specific keys to general keys
    pub fn normalized(&self) -> Self {
        match self {
            Self::Key(key) => Self::Key(key.normalized()),
            _ => self.clone()
        }
    }
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct Axis {
    axis_id: AxisId,
    scale: AxisScale,
    mods: KeyMods,
}

impl Axis {
    pub fn new(axis_id: AxisId, scale: AxisScale, mods: KeyMods) -> Self {
        Self {
            axis_id,
            scale,
            mods,
        }
    }

    pub fn with_unit_scale(axis_id: AxisId, mods: KeyMods) -> Self {
        Self::new(axis_id, 1.0, mods)
    }

    /// Transforms OS specific keys to general keys
    pub fn normalized(&self) -> Self {
        Self {
            axis_id: self.axis_id.normalized(),
            ..*self
        }
    }

    pub fn axis_id(&self) -> AxisId {
        self.axis_id
    }

    pub fn scale(&self) -> AxisScale {
        self.scale
    }

    pub fn mods(&self) -> KeyMods {
        self.mods
    }
}

impl From<Action> for Axis {
    fn from(action: Action) -> Self {
        Self::new(
            AxisId::Key(action.key()),
            1.0,
            action.mods()
        )
    }
}

impl PartialEq for Axis {
    fn eq(&self, other: &Self) -> bool {
        self.axis_id.eq(&other.axis_id)
        && self.mods.eq(&other.mods)
    }
}

impl Eq for Axis {}

impl Hash for Axis {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.axis_id.hash(state);
        self.mods.hash(state);
    }
}