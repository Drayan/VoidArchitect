/*!
Build script to recursively compile all GLSL shaders in assets/shaders to SPIR-V using shaderc.
Outputs .spv files to assets/shaders/compiled, preserving the original filename with .spv appended.
*/

use shaderc::{Compiler, ShaderKind};
use std::fs;
use std::path::{Path, PathBuf};

/// Recursively collect all .glsl files in a directory.
fn collect_glsl_files(dir: &Path, files: &mut Vec<PathBuf>) {
    if let Ok(entries) = fs::read_dir(dir) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_dir() {
                collect_glsl_files(&path, files);
            } else if let Some(ext) = path.extension() {
                if ext == "glsl" {
                    files.push(path);
                }
            }
        }
    }
}

/// Determine ShaderKind from file name (expects name.stage.glsl).
fn shader_kind_from_filename(filename: &str) -> Option<ShaderKind> {
    let stage = filename.rsplitn(3, '.').nth(0);
    match stage {
        Some("vert") => Some(ShaderKind::Vertex),
        Some("frag") => Some(ShaderKind::Fragment),
        Some("comp") => Some(ShaderKind::Compute),
        Some("geom") => Some(ShaderKind::Geometry),
        Some("tesc") => Some(ShaderKind::TessControl),
        Some("tese") => Some(ShaderKind::TessEvaluation),
        _ => None,
    }
}

fn main() {
    let shader_dir = Path::new("../assets/shaders");
    let out_dir = Path::new("../assets/shaders/compiled");

    // Ensure output directory exists
    fs::create_dir_all(out_dir).expect("Failed to create compiled shaders directory");

    let mut glsl_files = Vec::new();
    collect_glsl_files(shader_dir, &mut glsl_files);

    let compiler = Compiler::new().expect("Failed to create shader compiler");

    for src_path in glsl_files {
        let filename = src_path.file_name().unwrap().to_string_lossy();
        let kind = match shader_kind_from_filename(&filename) {
            Some(k) => k,
            None => {
                eprintln!(
                    "Skipping {:?}: cannot determine shader stage from filename",
                    src_path
                );
                continue;
            }
        };

        let src = fs::read_to_string(&src_path)
            .unwrap_or_else(|_| panic!("Failed to read shader: {:?}", src_path));

        println!("Compiling shader: {:?}", src_path);

        let binary_result = compiler
            .compile_into_spirv(&src, kind, &filename, "main", None)
            .unwrap_or_else(|e| panic!("Failed to compile {:?}: {}", src_path, e));

        // Replace the last extension with .spv (e.g., shader.frag.glsl -> shader.frag.spv)
        let mut spv_file_stem = src_path.file_stem().unwrap().to_os_string();
        if let Some(parent) = src_path.parent() {
            // If there are multiple extensions, reconstruct the stem with all but the last extension
            let mut stem_path = parent.join(&spv_file_stem);
            while let Some(ext) = stem_path.extension() {
                if ext == "glsl" {
                    stem_path = stem_path.with_extension("");
                } else {
                    break;
                }
            }
            spv_file_stem = stem_path.file_name().unwrap().to_os_string();
        }
        let spv_path = out_dir.join({
            let mut stem = spv_file_stem.clone();
            stem.push(".spv");
            stem
        });
        fs::write(&spv_path, binary_result.as_binary_u8())
            .unwrap_or_else(|_| panic!("Failed to write SPIR-V: {:?}", spv_path));
        println!("cargo:rerun-if-changed={}", src_path.display());
    }
}
