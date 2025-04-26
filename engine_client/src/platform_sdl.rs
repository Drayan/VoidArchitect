use std::{
    sync::Weak,
    sync::{Arc, RwLock},
};

pub(super) struct WindowHandle {
    pub window: Weak<RwLock<sdl3::video::Window>>,
}

pub(super) struct PlatformLayer {
    title: String,
    window_width: u32,
    window_height: u32,
    window: Option<Arc<RwLock<sdl3::video::Window>>>,
    sdl_context: Option<sdl3::Sdl>,
}

impl PlatformLayer {
    pub fn new() -> Self {
        Self {
            title: "Default Title".to_string(),
            window_width: 1280,
            window_height: 720,
            window: None,
            sdl_context: None,
        }
    }

    pub fn initialize(&mut self, title: &str, width: u32, height: u32) {
        self.title = title.to_string();
        self.window_width = width;
        self.window_height = height;

        let sdl_context = sdl3::init().unwrap();
        let video_subsystem = sdl_context.video().unwrap();

        let mut window = video_subsystem
            .window(&self.title, self.window_width, self.window_height)
            .vulkan() // TODO: Should be configurable
            .metal_view()
            .resizable()
            .position_centered()
            .build()
            .unwrap();

        window.show();

        self.window = Some(Arc::new(RwLock::new(window)));
        self.sdl_context = Some(sdl_context);
    }

    pub fn run(&self) -> Result<bool, String> {
        let mut context = match self.sdl_context {
            Some(ref context) => context,
            None => return Err("SDL context not initialized".to_string()),
        };
        let mut event_pump = match context.event_pump() {
            Ok(pump) => pump,
            Err(_) => return Err("Failed to get event pump".to_string()),
        };

        // Poll for events
        let mut should_quit = false;
        event_pump.poll_iter().for_each(|event| match event {
            sdl3::event::Event::Quit { .. } => {
                log::info!("Received quit event");
                should_quit = true;
            }
            // Handle window events
            sdl3::event::Event::Window { win_event, .. } => {
                if let sdl3::event::WindowEvent::CloseRequested { .. } = win_event {
                    log::info!("Received window close event");
                    should_quit = true;
                }
                if let sdl3::event::WindowEvent::Resized(width, height) = win_event {
                    log::info!("Window resized to {}x{}", width, height);
                    if let Some(window) = &self.window {
                        //TODO: Send the resize event
                    }
                }
            }

            // Handle keyboard events
            sdl3::event::Event::KeyDown {
                keycode, keymod, ..
            } => {
                // log::info!("Key down: {:?} with modifier: {:?}", keycode, keymod);
                //TODO: Handle key down events
            }
            sdl3::event::Event::KeyUp {
                keycode, keymod, ..
            } => {
                // log::info!("Key up: {:?} with modifier: {:?}", keycode, keymod);
                //TODO: Handle key up events
            }

            // Handle mouse events
            sdl3::event::Event::MouseButtonDown {
                mouse_btn, x, y, ..
            } => {
                // log::info!("Mouse button down: {:?} at ({}, {})", mouse_btn, x, y);
                //TODO: Handle mouse button down events
            }
            sdl3::event::Event::MouseButtonUp {
                mouse_btn, x, y, ..
            } => {
                // log::info!("Mouse button up: {:?} at ({}, {})", mouse_btn, x, y);
                //TODO: Handle mouse button up events
            }
            sdl3::event::Event::MouseMotion {
                mousestate,
                x,
                y,
                xrel,
                yrel,
                ..
            } => {
                // log::info!(
                //     "Mouse motion: state: {:?} at ({}, {}) with relative ({}, {})",
                //     mousestate,
                //     x,
                //     y,
                //     xrel,
                //     yrel
                // );
                //TODO: Handle mouse motion events
            }
            sdl3::event::Event::MouseWheel {
                x, y, direction, ..
            } => {
                // log::info!("Mouse wheel: ({}, {}) with direction {:?}", x, y, direction);
                //TODO: Handle mouse wheel events
            }

            _ => {}
        });

        Ok(should_quit)
    }

    pub fn shutdown(&mut self) {
        if let Some(window) = &self.window {
            window.write().unwrap().hide();
        }
        self.window = None;
        self.sdl_context = None;
    }

    pub fn get_window_handle(&self) -> Option<WindowHandle> {
        if let Some(window) = &self.window {
            Some(WindowHandle {
                window: Arc::downgrade(window),
            })
        } else {
            None
        }
    }
}
