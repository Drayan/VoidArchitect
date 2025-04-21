pub mod serialization {
    pub mod protos {
        include!(concat!(env!("OUT_DIR"), "/serialization.protos.rs"));
    }
}
