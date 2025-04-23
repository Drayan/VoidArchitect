mod backends;

use crate::{platform::WindowHandle, renderer::backends::RendererBackendVulkan};

pub trait RendererBackend {
    fn initialize(&mut self, window: WindowHandle) -> Result<(), String>;
    fn shutdown(&mut self) -> Result<(), String>;

    fn begin_frame(&mut self) -> Result<(), String>;
    fn end_frame(&mut self) -> Result<(), String>;
    fn resize(&mut self, width: u32, height: u32) -> Result<(), String>;
}

pub struct RendererFrontend {
    backend: Box<dyn RendererBackend>,
}

impl RendererFrontend {
    pub fn new() -> Self {
        Self {
            //TODO: This should be configurable
            backend: Box::new(RendererBackendVulkan::new()),
        }
    }

    pub fn initialize(&mut self, window: WindowHandle) -> Result<(), String> {
        self.backend.initialize(window)
    }

    pub fn shutdown(&mut self) -> Result<(), String> {
        self.backend.shutdown()
    }

    pub fn begin_frame(&mut self) -> Result<(), String> {
        self.backend.begin_frame()
    }

    pub fn end_frame(&mut self) -> Result<(), String> {
        self.backend.end_frame()
    }

    pub fn resize(&mut self, width: u32, height: u32) -> Result<(), String> {
        self.backend.resize(width, height)
    }
}
