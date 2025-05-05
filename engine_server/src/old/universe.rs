use actix::{Actor, Context, Handler, Message};
use std::collections::HashMap;
use uuid::Uuid;

// --- Messages ---

// --- Actor ---

/// Manages the game universes and their simulation.
///
/// The UniverseActor is responsible for:
/// - Creating and maintaining multiple universes
/// - Managing simulation ticks and time progression
/// - Coordinating celestial bodies, physics, and events
/// - Handling the core game simulation loop
#[derive(Debug)]
pub struct UniverseActor {
    /// Map of active universes: universe_id (UUID) -> UniverseInfo
    universes: HashMap<Uuid, UniverseInfo>,
    /// Default tick rate for new universes (in milliseconds)
    default_tick_rate_ms: u64,
}

impl Default for UniverseActor {
    fn default() -> Self {
        Self::new()
    }
}

impl UniverseActor {
    /// Creates a new UniverseActor.
    pub fn new() -> Self {
        UniverseActor {
            universes: HashMap::new(),
            default_tick_rate_ms: 1000, // 1 second default tick rate
        }
    }

    /// Generates a random seed if none is provided
    fn generate_seed(&self) -> u64 {
        use rand::Rng;
        rand::thread_rng().gen()
    }
}

impl Actor for UniverseActor {
    type Context = Context<Self>;

    fn started(&mut self, _ctx: &mut Self::Context) {
        log::info!("UniverseActor started.");
    }

    fn stopped(&mut self, _ctx: &mut Self::Context) {
        log::info!("UniverseActor stopped.");
        // Potential universe state saving if needed
    }
}

// --- Handlers ---


// --- Tests ---

#[cfg(test)]
mod tests {
    use super::*;
    use actix::Actor;

    #[actix::test]
    async fn test_universe_actor_creation() {
        // Simple test to ensure the actor can be created.
        let universe_actor = UniverseActor::new();
        assert!(universe_actor.universes.is_empty());
    }
}
