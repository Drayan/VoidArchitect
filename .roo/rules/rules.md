### 🔄 Project Awareness & Context
- **Always read `PLANNING.md`** at the start of a new conversation to understand the project's architecture, goals, style, and constraints.
- **Check `TASKS.md`** before starting a new task. If the task isn’t listed, add it with a brief description and today's date.
- **Use consistent naming conventions, file structure, and architecture patterns** as described in `PLANNING.md`.

### 🧱 Code Structure & Modularity
- **Never create a file longer than 500 lines of code.** If a file approaches this limit, refactor by splitting it into modules or helper files.
- **Organize code into clearly separated modules**, grouped by feature or responsibility.
- **Use clear, consistent imports** (prefer relative imports within packages).

### 🧪 Testing & Reliability
- **Always create Rust unit tests for new features** (functions, classes, routes, etc).
- **After updating any logic**, check whether existing unit tests need to be updated. If so, do it.
- **Unit tests in source files for:**
  - Should test the "private" API
  - Pure functions and algorithms
  - Simple structure validations
  - Isolated behavior tests
  - Invariant checks
  - Private function
  ```rust
  // Example: src/procedural/noise.rs
  pub fn perlin(x: f32, y: f32, z: f32) -> f32 { /* ... */ }

  #[cfg(test)]
  mod tests {
    use super::*;

    #[test]
    fn perlin_range_check() {
      for i in 0..100 {
        let value = perlin(i as f32 * 0.1, 0.0, 0.0);
        assert!(value >= -1.0 && value <= 1.0);
      }
    }
  }
  ```
- **Integration tests in `./tests/` for:**
  - Should test the public API only (no private)
  - Integration tests between components
  - Tests requiring complex setup
  - End-to-end procedural generation tests
  - User scenarios
  - Performance tests
  ```rust
  // Example: tests/galaxy_generation_tests.rs
  use void_architect::procedural::{GalaxyGenerator, GalaxySettings}; // Assuming GalaxySettings exists
  use void_architect::celestial::{Galaxy, Star}; // Assuming these types exist

  #[test]
  fn test_complete_galaxy_generation() {
    let mut generator = GalaxyGenerator::new(42, GalaxySettings::spiral()); // Assuming spiral() exists
    let galaxy = generator.generate();

    assert!(galaxy.star_count() > 1000); // Assuming star_count() exists
    assert!(galaxy.has_habitable_planets()); // Assuming has_habitable_planets() exists

    // Verify spiral structure
    let stars = galaxy.get_stars(); // Assuming get_stars() exists
    // Statistical tests on distribution...
  }
  ```
- **Use specialized tools:**
  - `proptest` for random generation and property-based testing.
  - `criterion` for performance benchmarks.
  - `mockall` for mocks when necessary.

### ✅ Task Completion
- **Mark completed tasks in `TASKS.md`** immediately after finishing them.
- Add new sub-tasks or TODOs discovered during development to `TASKS.md` under a “Discovered During Work” section.

### 📎 Style & Conventions
- **Use Rust** as the primary language.
- **Follow Rust conventions** and format with `rustfmt`.
- **Use rustdoc format** when writing documentation for functions, modules, structs, etc.

### 📚 Documentation & Explainability
- **Update `README.md`** when new features are added, dependencies change, or setup steps are modified.
- **Comment non-obvious code** and ensure everything is understandable to a mid-level developer.
- When writing complex logic, **add an inline `# Reason:` comment** explaining the why, not just the what.

### 🧠 AI Behavior Rules
- **Never assume missing context. Ask questions if uncertain.**
- **Never hallucinate libraries or functions** – only use known, verified Rust crates.
- **Always confirm file paths and module names** exist before referencing them in code or tests.
- **Never delete or overwrite existing code** unless explicitly instructed to or if part of a task from `TASKS.md`.

### 📚 Documentation & Explainability
- **Update `README.md`** when new features are added, dependencies change, or setup steps are modified.
- **Comment non-obvious code** and ensure everything is understandable to a mid-level developer.
- When writing complex logic, **add an inline `# Reason:` comment** explaining the why, not just the what.
- `README.md`, `PLANNING.md`, and `TASKS.md` should be updated when changes are made to the project structure, dependencies, or setup instructions.
- Every comments and files should be written in English.

### 🧠 AI Behavior Rules
- **Never assume missing context. Ask questions if uncertain.**
- **Never hallucinate libraries or functions** – only use known, verified Rust crates.
- **Always confirm file paths and module names** exist before referencing them in code or tests.
- **Never delete or overwrite existing code** unless explicitly instructed to or if part of a task from `TASKS.md`.
