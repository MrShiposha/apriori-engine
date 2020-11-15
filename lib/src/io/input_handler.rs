use {
    std::collections::HashMap,
    crate::io::*,
};

pub struct InputHandler<Id: InputId> {
    inputs: HashMap<Input, Id>,
    handlers: HashMap<Id, Box<dyn FnMut(Id, InputEvent, InputKind)>>,

    #[cfg(target_os = "windows")]
    pub(crate) aux: WindowsInputAuxInfo,
}

impl<Id: InputId> InputHandler<Id> {
    const LOG_TARGET: &'static str = "InputHandler";

    pub fn new() -> Self {
        Self::default()
    }

    pub fn handle(&mut self, input_id: Id) -> InputHandlerAdder<Id> {
        InputHandlerAdder::new(input_id, self)
    }

    pub fn update_inputs(&mut self, input_map: &InputMap<Id>) {
        for (id, variants) in input_map.hash_map().iter() {
            let inputs: Vec<Input> = variants.clone().into();

            for input in inputs {
                let input = input.normalized();
                match input.split_general_mod() {
                    Some((left, right)) => {
                        self.insert_input(left, id);
                        self.insert_input(right, id);
                    },
                    None => {
                        self.insert_input(input, id);
                    }
                }
            }
        }
    }

    fn insert_input(&mut self, input: Input, new_id: &Id) {
        if let Some(old_id) = self.inputs.insert(input.clone(), new_id.clone()) {
            log::error! {
                target: Self::LOG_TARGET,
                "{:#?} has duplicate id (old = {:#?}, new = {:#?})",
                input, old_id, new_id
            };
        }
    }

    pub fn run_action_handler(&mut self, action: Action, event: InputEvent) {
        match self.inputs.get(&action.clone().into()) {
            Some(id ) => {
                if let Some(handler) = self.handlers.get_mut(id) {
                    handler(id.clone(), event, InputKind::Action);
                }
            },
            None => if let InputEvent::Pressed = event {
                if let Some((Input::Axis(axis), id)) = self.inputs.get_key_value(&Input::Axis(action.into())) {
                    if let Some(handler) = self.handlers.get_mut(id) {
                        handler(id.clone(), event, InputKind::Axis(axis.scale()));
                    }
                }
            }
        }
    }

    pub fn run_axis_handler(&mut self, axis: Axis, event: InputEvent) {
        let scale = axis.scale();
        if let Some(id) = self.inputs.get(&axis.into()) {
            if let Some(handler) = self.handlers.get_mut(id) {
                handler(id.clone(), event, InputKind::Axis(scale));
            }
        }
    }
}

impl<Id: InputId> Default for InputHandler<Id> {
    fn default() -> Self {
        Self {
            inputs: Default::default(),
            handlers: Default::default(),

            #[cfg(target_os = "windows")]
            aux: WindowsInputAuxInfo::new(),
        }
    }
}

impl<Id: InputId> From<InputMap<Id>> for InputHandler<Id> {
    fn from(input_map: InputMap<Id>) -> Self {
        let mut handler = Self::new();

        handler.update_inputs(&input_map);

        handler
    }
}

pub struct InputHandlerAdder<'h, Id: InputId> {
    input_id: Id,
    handler: &'h mut InputHandler<Id>
}

impl<'h, Id: InputId> InputHandlerAdder<'h, Id> {
    const LOG_TARGET: &'static str = "InputHandlerAdder";

    fn new(input_id: Id, handler: &'h mut InputHandler<Id>) -> Self {
        Self {
            input_id,
            handler,
        }
    }

    pub fn with<H>(self, new_handler: H) -> &'h mut InputHandler<Id>
    where
        H: FnMut(Id, InputEvent, InputKind) + 'static
    {
        self.handler.handlers.insert(self.input_id, Box::new(new_handler));
        self.handler
    }

    pub fn axis<H>(self, mut new_handler: H) -> &'h mut InputHandler<Id>
    where
        H: FnMut(AxisValue) + 'static
    {
        self.with(move |id, event, kind| {
            match kind {
                InputKind::Axis(scale) => new_handler(event.axis_value() * scale),
                _ => log::error! {
                    target: Self::LOG_TARGET,
                    "{:#?} handler type mismatch, handler expects axis while input id is action",
                    id
                }
            }
        })
    }

    pub fn action<H>(self, mut new_handler: H) -> &'h mut InputHandler<Id>
    where
        H: FnMut(InputEvent) + 'static
    {
        self.with(move |id, event, kind| {
            match kind {
                InputKind::Action => new_handler(event),
                _ => log::error! {
                    target: Self::LOG_TARGET,
                    "{:#?} handler type mismatch, handler expects action while input id is axis",
                    id
                }
            }
        })
    }
}

#[cfg(target_os = "windows")]
pub(crate) struct WindowsInputAuxInfo {
    pub mods: KeyMods
}

#[cfg(target_os = "windows")]
impl WindowsInputAuxInfo {
    fn new() -> Self {
        Self {
            mods: KeyMods::empty()
        }
    }
}
