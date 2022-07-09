use super::*;
use egui::CursorIcon;
use egui::Event;
use egui::Modifiers;
use std::ffi::CStr;
use std::slice::from_raw_parts;

const SCROLL_SPEED: f32 = 50.0;

fn to_egui_pointer_button(btn: egui_PointerButton) -> egui::PointerButton {
    match btn {
        egui_PointerButton_Primary => egui::PointerButton::Primary,
        egui_PointerButton_Secondary => egui::PointerButton::Secondary,
        egui_PointerButton_Middle => egui::PointerButton::Middle,
        _ => panic!("Invalid enum value"),
    }
}

impl From<egui_ModifierKeys> for egui::Modifiers {
    fn from(mk: egui_ModifierKeys) -> Self {
        Self {
            alt: mk.alt,
            ctrl: mk.ctrl,
            shift: mk.shift,
            mac_cmd: mk.macCmd,
            command: mk.command,
        }
    }
}

fn sdl_to_egui_key(key_code: SDL_KeyCode) -> Result<egui::Key, ()> {
    match key_code {
        SDL_KeyCode_SDLK_RETURN => Ok(egui::Key::Enter),
        SDL_KeyCode_SDLK_ESCAPE => Ok(egui::Key::Escape),
        SDL_KeyCode_SDLK_BACKSPACE => Ok(egui::Key::Backspace),
        SDL_KeyCode_SDLK_TAB => Ok(egui::Key::Tab),
        SDL_KeyCode_SDLK_SPACE => Ok(egui::Key::Space),
        SDL_KeyCode_SDLK_0 => Ok(egui::Key::Num0),
        SDL_KeyCode_SDLK_1 => Ok(egui::Key::Num1),
        SDL_KeyCode_SDLK_2 => Ok(egui::Key::Num2),
        SDL_KeyCode_SDLK_3 => Ok(egui::Key::Num3),
        SDL_KeyCode_SDLK_4 => Ok(egui::Key::Num4),
        SDL_KeyCode_SDLK_5 => Ok(egui::Key::Num5),
        SDL_KeyCode_SDLK_6 => Ok(egui::Key::Num6),
        SDL_KeyCode_SDLK_7 => Ok(egui::Key::Num7),
        SDL_KeyCode_SDLK_8 => Ok(egui::Key::Num8),
        SDL_KeyCode_SDLK_9 => Ok(egui::Key::Num9),
        SDL_KeyCode_SDLK_a => Ok(egui::Key::A),
        SDL_KeyCode_SDLK_b => Ok(egui::Key::B),
        SDL_KeyCode_SDLK_c => Ok(egui::Key::C),
        SDL_KeyCode_SDLK_d => Ok(egui::Key::D),
        SDL_KeyCode_SDLK_e => Ok(egui::Key::E),
        SDL_KeyCode_SDLK_f => Ok(egui::Key::F),
        SDL_KeyCode_SDLK_g => Ok(egui::Key::G),
        SDL_KeyCode_SDLK_h => Ok(egui::Key::H),
        SDL_KeyCode_SDLK_i => Ok(egui::Key::I),
        SDL_KeyCode_SDLK_j => Ok(egui::Key::J),
        SDL_KeyCode_SDLK_k => Ok(egui::Key::K),
        SDL_KeyCode_SDLK_l => Ok(egui::Key::L),
        SDL_KeyCode_SDLK_m => Ok(egui::Key::M),
        SDL_KeyCode_SDLK_n => Ok(egui::Key::N),
        SDL_KeyCode_SDLK_o => Ok(egui::Key::O),
        SDL_KeyCode_SDLK_p => Ok(egui::Key::P),
        SDL_KeyCode_SDLK_q => Ok(egui::Key::Q),
        SDL_KeyCode_SDLK_r => Ok(egui::Key::R),
        SDL_KeyCode_SDLK_s => Ok(egui::Key::S),
        SDL_KeyCode_SDLK_t => Ok(egui::Key::T),
        SDL_KeyCode_SDLK_u => Ok(egui::Key::U),
        SDL_KeyCode_SDLK_v => Ok(egui::Key::V),
        SDL_KeyCode_SDLK_w => Ok(egui::Key::W),
        SDL_KeyCode_SDLK_x => Ok(egui::Key::X),
        SDL_KeyCode_SDLK_y => Ok(egui::Key::Y),
        SDL_KeyCode_SDLK_z => Ok(egui::Key::Z),
        SDL_KeyCode_SDLK_INSERT => Ok(egui::Key::Insert),
        SDL_KeyCode_SDLK_HOME => Ok(egui::Key::Home),
        SDL_KeyCode_SDLK_PAGEUP => Ok(egui::Key::PageUp),
        SDL_KeyCode_SDLK_DELETE => Ok(egui::Key::Delete),
        SDL_KeyCode_SDLK_END => Ok(egui::Key::End),
        SDL_KeyCode_SDLK_PAGEDOWN => Ok(egui::Key::PageDown),
        SDL_KeyCode_SDLK_RIGHT => Ok(egui::Key::ArrowRight),
        SDL_KeyCode_SDLK_LEFT => Ok(egui::Key::ArrowLeft),
        SDL_KeyCode_SDLK_DOWN => Ok(egui::Key::ArrowDown),
        SDL_KeyCode_SDLK_UP => Ok(egui::Key::ArrowUp),
        SDL_KeyCode_SDLK_KP_ENTER => Ok(egui::Key::Enter),
        SDL_KeyCode_SDLK_KP_1 => Ok(egui::Key::Num1),
        SDL_KeyCode_SDLK_KP_2 => Ok(egui::Key::Num2),
        SDL_KeyCode_SDLK_KP_3 => Ok(egui::Key::Num3),
        SDL_KeyCode_SDLK_KP_4 => Ok(egui::Key::Num4),
        SDL_KeyCode_SDLK_KP_5 => Ok(egui::Key::Num5),
        SDL_KeyCode_SDLK_KP_6 => Ok(egui::Key::Num6),
        SDL_KeyCode_SDLK_KP_7 => Ok(egui::Key::Num7),
        SDL_KeyCode_SDLK_KP_8 => Ok(egui::Key::Num8),
        SDL_KeyCode_SDLK_KP_9 => Ok(egui::Key::Num9),
        SDL_KeyCode_SDLK_KP_0 => Ok(egui::Key::Num0),
        _ => Err(()),
    }
}

pub fn to_egui_cursor_icon(i: CursorIcon) -> egui_CursorIcon {
    match i {
        CursorIcon::Default => egui_CursorIcon_Default,
        CursorIcon::None => egui_CursorIcon_None,
        CursorIcon::ContextMenu => egui_CursorIcon_ContextMenu,
        CursorIcon::Help => egui_CursorIcon_Help,
        CursorIcon::PointingHand => egui_CursorIcon_PointingHand,
        CursorIcon::Progress => egui_CursorIcon_Progress,
        CursorIcon::Wait => egui_CursorIcon_Wait,
        CursorIcon::Cell => egui_CursorIcon_Cell,
        CursorIcon::Crosshair => egui_CursorIcon_Crosshair,
        CursorIcon::Text => egui_CursorIcon_Text,
        CursorIcon::VerticalText => egui_CursorIcon_VerticalText,
        CursorIcon::Alias => egui_CursorIcon_Alias,
        CursorIcon::Copy => egui_CursorIcon_Copy,
        CursorIcon::Move => egui_CursorIcon_Move,
        CursorIcon::NoDrop => egui_CursorIcon_NoDrop,
        CursorIcon::NotAllowed => egui_CursorIcon_NotAllowed,
        CursorIcon::Grab => egui_CursorIcon_Grab,
        CursorIcon::Grabbing => egui_CursorIcon_Grabbing,
        CursorIcon::AllScroll => egui_CursorIcon_AllScroll,
        CursorIcon::ResizeHorizontal => egui_CursorIcon_ResizeHorizontal,
        CursorIcon::ResizeNeSw => egui_CursorIcon_ResizeNeSw,
        CursorIcon::ResizeNwSe => egui_CursorIcon_ResizeNwSe,
        CursorIcon::ResizeVertical => egui_CursorIcon_ResizeVertical,
        CursorIcon::ResizeEast => egui_CursorIcon_ResizeEast,
        CursorIcon::ResizeSouthEast => egui_CursorIcon_ResizeSouthEast,
        CursorIcon::ResizeSouth => egui_CursorIcon_ResizeSouth,
        CursorIcon::ResizeSouthWest => egui_CursorIcon_ResizeSouthWest,
        CursorIcon::ResizeWest => egui_CursorIcon_ResizeWest,
        CursorIcon::ResizeNorthWest => egui_CursorIcon_ResizeNorthWest,
        CursorIcon::ResizeNorth => egui_CursorIcon_ResizeNorth,
        CursorIcon::ResizeNorthEast => egui_CursorIcon_ResizeNorthEast,
        CursorIcon::ResizeColumn => egui_CursorIcon_ResizeColumn,
        CursorIcon::ResizeRow => egui_CursorIcon_ResizeRow,
        CursorIcon::ZoomIn => egui_CursorIcon_ZoomIn,
        CursorIcon::ZoomOut => egui_CursorIcon_ZoomOut,
    }
}

#[derive(Debug)]
pub enum TranslationError {
    Text(std::str::Utf8Error),
}

impl From<std::str::Utf8Error> for TranslationError {
    fn from(e: std::str::Utf8Error) -> Self {
        TranslationError::Text(e)
    }
}

impl std::fmt::Display for TranslationError {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        match self {
            TranslationError::Text(text) => text.fmt(f),
        }
    }
}

pub fn translate_raw_input(input: &egui_Input) -> Result<egui::RawInput, TranslationError> {
    let mut events = Vec::new();

    unsafe {
        let in_events = from_raw_parts(input.inputEvents, input.numInputEvents);
        for in_event in in_events {
            let out_event = match in_event.common.type_ {
                egui_InputEventType_PointerMoved => {
                    let event = &in_event.pointerMoved;
                    Some(Event::PointerMoved(event.pos.into()))
                }
                egui_InputEventType_PointerButton => {
                    let event = &in_event.pointerButton;
                    Some(Event::PointerButton {
                        pos: event.pos.into(),
                        button: to_egui_pointer_button(event.button),
                        pressed: event.pressed,
                        modifiers: event.modifiers.into(),
                    })
                }
                egui_InputEventType_Scroll => {
                    let event = &in_event.scroll;
                    let virtual_delta_x = event.delta.x * SCROLL_SPEED;
                    let virtual_delta_y = event.delta.y * SCROLL_SPEED;
                    Some(Event::Scroll(egui::vec2(virtual_delta_x, virtual_delta_y)))
                }
                egui_InputEventType_Text => {
                    let event = &in_event.text;
                    let text = CStr::from_ptr(event.text).to_str()?.to_owned();
                    Some(Event::Text(text))
                }
                egui_InputEventType_Key => {
                    let event = &in_event.key;
                    match sdl_to_egui_key(event.key) {
                        Ok(key) => Some(Event::Key {
                            key,
                            pressed: event.pressed,
                            modifiers: event.modifiers.into(),
                        }),
                        Err(_) => None,
                    }
                }
                egui_InputEventType_CompositionStart => Some(Event::CompositionStart),
                egui_InputEventType_CompositionUpdate => {
                    let event = &in_event.compositionUpdate;
                    let text = CStr::from_ptr(event.text).to_str()?.to_owned();
                    Some(Event::CompositionUpdate(text))
                }
                egui_InputEventType_CompositionEnd => {
                    let event = &in_event.compositionEnd;
                    let text = CStr::from_ptr(event.text).to_str()?.to_owned();
                    Some(Event::CompositionEnd(text))
                }
                egui_InputEventType_Copy => Some(Event::Copy),
                egui_InputEventType_Cut => Some(Event::Cut),
                egui_InputEventType_Paste => {
                    let event = &in_event.paste;
                    let str = CStr::from_ptr(event.str_).to_str()?.to_owned();
                    Some(Event::Paste(str))
                }
                _ => {
                    panic!("Invalid input event type")
                }
            };

            if let Some(event) = out_event {
                events.push(event);
            }
        }
    }

    Ok(egui::RawInput {
        dropped_files: Vec::new(),
        hovered_files: Vec::new(),
        events: events,
        modifiers: Modifiers::default(),
        time: Some(input.time),
        predicted_dt: input.predictedDeltaTime,
        screen_rect: Some(input.screenRect.into()),
        pixels_per_point: Some(input.pixelsPerPoint),
        max_texture_side: None,
    })
}

pub struct InputTranslator {
    egui_translator: *mut gfx_EguiInputTranslator,
}

impl Drop for InputTranslator {
    fn drop(&mut self) {
        unsafe {
            gfx_EguiInputTranslator_destroy(self.egui_translator);
        }
    }
}

impl InputTranslator {
    pub fn new() -> Self {
        unsafe {
            Self {
                egui_translator: gfx_EguiInputTranslator_create(),
            }
        }
    }

    pub fn as_mut_ptr(self: &Self) -> *mut gfx_EguiInputTranslator {
        self.egui_translator
    }

    pub fn translate(
        self: &Self,
        window: *mut gfx_Window,
        sdl_events: *const u8,
        time: f64,
        delta_time: f32,
    ) -> *const egui_Input {
        unsafe {
            gfx_EguiInputTranslator_translateFromInputEvents(
                self.egui_translator,
                sdl_events,
                window,
                time,
                delta_time,
            )
        }
    }
}
