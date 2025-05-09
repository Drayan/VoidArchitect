use std::f32::consts::{PI, TAU};

use actix::{Actor, Addr, AsyncContext, Handler, Message};
use nalgebra::{Quaternion, UnitQuaternion, Vector3};
use tokio::time::{Duration, Instant};

use super::session::SessionActor;

// --- Actor ---
pub struct UniverseActor {
    // For now, we will just keep the state of the poor lonely cube in the universe's state.
    position: Vector3<f32>,
    rotation: UnitQuaternion<f32>,
    scale: Vector3<f32>,

    //NOTE: We'll just use the simple solution now, which is to keep a list of all the sessions's
    // subscriptions to the universe. We'll send the updates to all of them.
    // In the future, we may want to use a more sophisticated solution, like an event bus.
    // Or even a hierarchical and spatial partitioning of the universe, each with its own event bus.
    // But for now, we'll just keep it simple.
    subscriptions: Vec<Addr<SessionActor>>,
    elapsed_time: Duration,
    last_update: Instant,
}

impl UniverseActor {
    /// Creates a new UniverseActor.
    pub fn new() -> Self {
        //TODO: Retrieve the cube's transform from the PersistenceActor
        UniverseActor {
            position: Vector3::new(0.0, 0.0, 0.0),
            rotation: UnitQuaternion::identity(),
            scale: Vector3::new(1.0, 1.0, 1.0),

            subscriptions: Vec::new(),
            last_update: Instant::now(),
            elapsed_time: Duration::new(0, 0),
        }
    }
}

impl Actor for UniverseActor {
    type Context = actix::Context<Self>;

    fn started(&mut self, ctx: &mut Self::Context) {
        log::info!("UniverseActor started.");

        // Internal timer to send ticks.
        self.last_update = Instant::now();
        ctx.run_interval(Duration::from_secs_f32(1.0 / 60.0), |actor, ctx| {
            let delta_time = actor.last_update.elapsed().as_secs_f32();
            actor.last_update = Instant::now();
            actor.elapsed_time += Duration::from_secs_f32(delta_time);

            ctx.notify(UniverseTick { delta_time });
        });
    }

    fn stopped(&mut self, _ctx: &mut Self::Context) {
        log::info!("UniverseActor stopped.");
        // Potential universe state saving if needed
    }
}

// --- Messages ---
/// Message SessionActor should send to UniverseActor to subscribe to updates
#[derive(Message)]
#[rtype(result = "()")]
pub struct SubscribeToUniverse {
    pub session_addr: Addr<SessionActor>,
}

/// Message SessionActor should send to UniverseActor to unsubscribe from updates
#[derive(Message)]
#[rtype(result = "()")]
pub struct UnsubscribeFromUniverse {
    pub session_addr: Addr<SessionActor>,
}

/// Message sent by UniverseActor to all subscribed sessions to notify them of updates
#[derive(Message, Debug)]
#[rtype(result = "()")]
pub struct UniverseUpdate {
    pub position: Vector3<f32>,
    pub rotation: UnitQuaternion<f32>,
    pub scale: Vector3<f32>,
}

#[derive(Message)]
#[rtype(result = "()")]
struct UniverseTick {
    pub delta_time: f32,
}

// --- Handlers ---
//TODO: Implement a "tick" message to update the universe's state
// and send updates to all subscribed sessions
impl Handler<SubscribeToUniverse> for UniverseActor {
    type Result = ();

    fn handle(&mut self, msg: SubscribeToUniverse, _ctx: &mut Self::Context) {
        log::info!("Session subscribed to universe: {:?}", msg.session_addr);
        self.subscriptions.push(msg.session_addr);
    }
}

impl Handler<UnsubscribeFromUniverse> for UniverseActor {
    type Result = ();

    fn handle(&mut self, msg: UnsubscribeFromUniverse, _ctx: &mut Self::Context) {
        log::info!("Session unsubscribed from universe: {:?}", msg.session_addr);
        self.subscriptions.retain(|addr| addr != &msg.session_addr);
    }
}

impl Handler<UniverseTick> for UniverseActor {
    type Result = ();

    fn handle(&mut self, msg: UniverseTick, _ctx: &mut Self::Context) {
        // Update the universe's state based on the delta time
        // log::trace!("Universe tick: delta_time = {}", msg.delta_time);

        // For now, we'll just update the cube's position with a simple sine wave
        let amplitude = 1.0;
        let frequency = 0.5;
        self.position.y =
            (self.elapsed_time.as_secs_f32() * frequency * TAU).sin() * amplitude;
        // And rotate the cube around the Y axis
        self.rotation *=
            nalgebra::UnitQuaternion::from_axis_angle(&Vector3::y_axis(), msg.delta_time);

        // Send updates to all subscribed sessions
        for session_addr in &self.subscriptions {
            session_addr.do_send(UniverseUpdate {
                position: self.position,
                rotation: self.rotation,
                scale: self.scale,
            });
        }
    }
}
