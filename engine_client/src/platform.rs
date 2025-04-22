use winit::event_loop::{ActiveEventLoop, EventLoop};

pub struct PlatformSystem {
    event_loop: Option<EventLoop<()>>,
}

impl PlatformSystem {
    pub fn new() -> Self {
        PlatformSystem { event_loop: None }
    }

    pub fn initialize(&mut self) {
        // Initialize the event loop
        self.event_loop = Some(EventLoop::new().unwrap());
        println!("PlatformSystem initialized");
    }
}
