#![cfg_attr(all(target_os = "windows", target_arch = "x86"), feature(abi_thiscall))]
#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

impl From<egui::Pos2> for egui_Pos2 {
    fn from(pos: egui::Pos2) -> Self {
        Self { x: pos.x, y: pos.y }
    }
}

impl From<egui_Pos2> for egui::Pos2 {
    fn from(pos: egui_Pos2) -> Self {
        Self { x: pos.x, y: pos.y }
    }
}

impl From<egui_Pos2> for egui::Vec2 {
    fn from(pos: egui_Pos2) -> Self {
        Self { x: pos.x, y: pos.y }
    }
}

impl From<egui::Rect> for egui_Rect {
    fn from(r: egui::Rect) -> Self {
        Self {
            min: r.min.into(),
            max: r.max.into(),
        }
    }
}

impl From<egui_Rect> for egui::Rect {
    fn from(r: egui_Rect) -> Self {
        Self {
            min: r.min.into(),
            max: r.max.into(),
        }
    }
}

impl From<epaint::Color32> for egui_Color32 {
    fn from(c: epaint::Color32) -> Self {
        Self {
            r: c.r(),
            g: c.g(),
            b: c.b(),
            a: c.a(),
        }
    }
}

impl From<epaint::Vertex> for egui_Vertex {
    fn from(v: epaint::Vertex) -> Self {
        Self {
            pos: v.pos.into(),
            uv: v.uv.into(),
            color: v.color.into(),
        }
    }
}

impl From<epaint::TextureId> for egui_TextureId {
    fn from(v: epaint::TextureId) -> Self {
        Self {
            id: if let epaint::TextureId::Managed(managed) = v {
                managed
            } else {
                0
            },
        }
    }
}

mod input;
mod renderer;
pub use input::*;
pub use renderer::*;

#[cfg(feature = "standalone_test")]
#[macro_use]
extern crate lazy_static;

#[cfg(feature = "standalone_test")]
mod color_test;

#[cfg(feature = "standalone_test")]
mod standalone_test;
