use std::{
    marker::PhantomData,
    sync::{Arc, Mutex, RwLock},
};

/// This is just a simple test app helping to try and experiment with the engine.
use void_architect_ecs::{Component, Registry, component::ComponentBundle};

struct VASDLWindowWrapper {
    sdl_window: sdl3::video::Window,
}
unsafe impl Send for VASDLWindowWrapper {}
unsafe impl Sync for VASDLWindowWrapper {}

#[derive(Component)]
struct Window {
    width: u32,
    height: u32,
    title: String,
    sdl_window: VASDLWindowWrapper,
}

fn main() {
    env_logger::Builder::new().filter_level(log::LevelFilter::max()).init();
    log::info!("Starting test app");

    // Create a ECS Registry
    let mut registry = Registry::new();
    registry.register_component::<Window>();

    let sdl = sdl3::init().unwrap();
    let video_subsystem = sdl.video().unwrap();
    let mut window = video_subsystem
        .window("Test App", 800, 600)
        .position_centered()
        .resizable()
        .build()
        .unwrap();
    window.show();

    // Create a Window component and add it to the registry
    let window_component = Window {
        width: 800,
        height: 600,
        title: "Test App".to_string(),
        sdl_window: VASDLWindowWrapper { sdl_window: window },
    };
    let entity = registry.spawn();
    registry.insert(entity, (window_component,)).unwrap();
    log::info!("Spawned entity with Window component: {:?}", entity);

    let mut should_quit = false;
    loop {
        // Poll for events
        for event in sdl.event_pump().unwrap().poll_iter() {
            use sdl3::event::Event;
            // Handle events here
            match event {
                Event::Quit { .. } => {
                    log::info!("Received quit event");
                    should_quit = true;
                }
                Event::Window { win_event, .. } => {
                    match win_event {
                        sdl3::event::WindowEvent::Resized(width, height) => {
                            log::info!("Window resized to {}x{}", width, height);
                            // Update the Window component in the registry
                            if let Some(window) = registry.get_mut::<Window>(entity) {
                                window.width = width as u32;
                                window.height = height as u32;
                            }
                        }
                        _ => {}
                    }
                }
                Event::KeyDown { keycode, .. } => {
                    log::info!("Key down: {:?}", keycode);
                    if let Some(keycode) = keycode {
                        match keycode {
                            sdl3::keyboard::Keycode::Escape => {
                                log::info!("Escape key pressed, quitting");
                                should_quit = true;
                            }
                            //TEMP: Print window component on key press as a debug check
                            sdl3::keyboard::Keycode::F1 => {
                                if let Some(window) = registry.get::<Window>(entity) {
                                    log::debug!(
                                        "Winodw component width: {}, height: {}",
                                        window.width,
                                        window.height
                                    );
                                }
                            }
                            _ => {
                                log::info!("Key pressed: {:?}", keycode);
                                // Handle other keys here
                            }
                        }
                    }
                }
                _ => {}
            }
        }

        if should_quit {
            log::info!("Quitting main loop");
            break;
        }
    }
    log::info!("Exiting test app");
}
