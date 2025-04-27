//! Build script to compile GLSL shaders to SPIR-V using shaderc.
//! Outputs .spv files alongside the source shaders.

use shaderc::{Compiler, ShaderKind};
use std::fs;
use std::path::Path;

fn main() {
    let shader_dir = "../assets/shaders";
    let out_dir = "../assets/shaders";
    let shaders = [
        ("triangle.vert", ShaderKind::Vertex),
        ("triangle.frag", ShaderKind::Fragment),
    ];

    let compiler = Compiler::new().expect("Failed to create shader compiler");

    for (filename, kind) in shaders.iter() {
        let src_path = Path::new(shader_dir).join(filename);
        let src = fs::read_to_string(&src_path)
            .unwrap_or_else(|_| panic!("Failed to read shader: {:?}", src_path));
        let binary_result = compiler
            .compile_into_spirv(&src, *kind, filename, "main", None)
            .unwrap_or_else(|e| panic!("Failed to compile {:?}: {}", src_path, e));

        let spv_path = Path::new(out_dir).join(format!("{}.spv", filename));
        fs::write(&spv_path, binary_result.as_binary_u8())
            .unwrap_or_else(|_| panic!("Failed to write SPIR-V: {:?}", spv_path));
        println!("cargo:rerun-if-changed={}", src_path.display());
    }
}
