use {
    std::{
        collections::HashMap,
        fs::File,
        hash::Hash,
        marker::Unpin,
        fmt,
    },
    serde::{Serialize, Deserialize, de::DeserializeOwned},
    ron::de::from_reader,
    crate::{
        core::Result,
        io::*
    }
};

#[derive(Debug, PartialEq, Serialize, Deserialize, Clone)]
pub enum InputVariants {
    Axis(Vec<Axis>),
    Action(Vec<Action>),
}

impl Into<Vec<Input>> for InputVariants {
    fn into(self) -> Vec<Input> {
        match self {
            Self::Axis(axis) => axis
                .into_iter()
                .map(|axis| axis.into())
                .collect(),
            Self::Action(actions) => actions
                .into_iter()
                .map(|action| action.into())
                .collect()
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub enum Input {
    Axis(Axis),
    Action(Action)
}

impl Input {
    /// Transforms OS specific keys to general keys
    pub fn normalized(&self) -> Self {
        match self {
            Self::Axis(axis) => Self::Axis(axis.normalized()),
            Self::Action(action) => Self::Action(action.normalized())
        }
    }

    /// Splits `Ctrl`, `Alt` or `Shift` to left and right
    pub fn split_general_mod(&self) -> Option<(Input, Input)> {
        match self {
            Self::Axis(axis) => match axis.axis_id() {
                AxisId::Key(key) => key.split_general_mod()
                    .map(|(left, right)| {
                        macro_rules! make_input {
                            ($key:expr) => {
                                Self::Axis(
                                    Axis::new(
                                        AxisId::Key($key),
                                        axis.scale(),
                                        axis.mods()
                                    )
                                )
                            };
                        }

                        (
                            make_input![left],
                            make_input![right]
                        )
                    }),
                _ => None
            }
            Self::Action(action) => action.key()
                .split_general_mod()
                .map(|(left, right)| {
                    macro_rules! make_input {
                        ($key:expr) => {
                            Self::Action(
                                Action::new(
                                    $key,
                                    action.mods()
                                ).unwrap()
                            )
                        };
                    }

                    (
                        make_input![left],
                        make_input![right]
                    )
                })
        }
    }
}

impl From<Axis> for Input {
    fn from(axis: Axis) -> Self {
        Self::Axis(axis)
    }
}

impl From<Action> for Input {
    fn from(action: Action) -> Self {
        Self::Action(action)
    }
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub enum InputKind {
    Axis(AxisScale),
    Action
}

pub trait InputId: fmt::Debug + Clone + Unpin + Hash + Eq + Serialize + DeserializeOwned
{}

impl<T> InputId for T
where T: fmt::Debug + Clone + Unpin + Hash + Eq + Serialize + DeserializeOwned
{}

#[derive(Debug, Serialize, Deserialize)]
pub struct InputMap<Id: Hash + Eq> {
    input_map: HashMap<Id, InputVariants>
}

impl<Id: InputId> InputMap<Id> {
    pub fn load<P: AsRef<str>>(path: P) -> Result<Self> {
        let path = path.as_ref();

        let file = File::open(path)?;
        from_reader(file).map_err(|err| err.into())
    }

    pub fn hash_map(&self) -> &HashMap<Id, InputVariants> {
        &self.input_map
    }
}

#[derive(Debug, PartialEq)]
pub enum InputEvent {
    Pressed,
    Released,
    // Repeat(u16),
    // DoubleClick,
    Axis(AxisValue),
}

impl InputEvent {
    pub fn axis_value(&self) -> AxisValue {
        match self {
            Self::Axis(value) => *value,
            // Self::Repeat(value) => *value as AxisValue,
            Self::Pressed => 1.0,
            _ => 0.0
        }
    }
}