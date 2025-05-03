use std::fs;
use std::io;
use std::path::Path;

fn main() {
    // This is a build script that runs before the main program.
    // It is used to compile the protobuf files into Rust code.
    // The generated code will be placed in the src/protos directory.
    println!("cargo:rerun-if-changed=src/messages");
    println!("cargo:rerun-if-changed=build.rs");

    // Chemin vers le répertoire des fichiers proto
    let proto_dir = Path::new("src/messages");

    // Trouver tous les fichiers .proto
    let proto_files = find_proto_files(proto_dir)
        .unwrap_or_else(|e| panic!("Failed to find proto files: {}", e));

    if proto_files.is_empty() {
        println!("No proto files found in {}", proto_dir.display());
        return;
    }

    // Afficher les fichiers trouvés
    println!("Found {} proto files:", proto_files.len());
    for file in &proto_files {
        println!("  {}", file.display());
    }

    // Convertir les chemins en chaînes de caractères pour prost_build
    let proto_paths: Vec<String> =
        proto_files.iter().map(|p| p.to_string_lossy().to_string()).collect();

    // Compiler les fichiers protobuf
    prost_build::compile_protos(&proto_paths, &["src/messages"])
        .unwrap_or_else(|e| panic!("Failed to compile protos: {}", e));
}

// Fonction pour trouver récursivement tous les fichiers .proto
fn find_proto_files(dir: &Path) -> io::Result<Vec<std::path::PathBuf>> {
    let mut proto_files = Vec::new();

    if dir.is_dir() {
        for entry in fs::read_dir(dir)? {
            let entry = entry?;
            let path = entry.path();

            if path.is_dir() {
                // Récursion dans les sous-répertoires
                let mut sub_files = find_proto_files(&path)?;
                proto_files.append(&mut sub_files);
            } else if let Some(ext) = path.extension() {
                // Ajouter les fichiers .proto
                if ext == "proto" {
                    proto_files.push(path);
                }
            }
        }
    }

    Ok(proto_files)
}
