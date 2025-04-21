fn main() {
    // This is a build script that runs before the main program.
    // It is used to compile the protobuf files into Rust code.
    // The generated code will be placed in the src/protos directory.
    println!("cargo:rerun-if-changed=src/serialization/protos");
    println!("cargo:rerun-if-changed=build.rs");

    // Compile the protobuf files
    prost_build::compile_protos(
        &["src/serialization/protos/hello.proto"],
        &["src/serialization/protos"],
    )
    .unwrap_or_else(|e| panic!("Failed to compile protos: {}", e));
}
