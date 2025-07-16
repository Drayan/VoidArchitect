//
// Created by Michael Desmedt on 15/07/2025.
//
#pragma once

#include "VoidArchitect/Engine/Common/Core.hpp"

#include <shared_mutex>
#include <yaml-cpp/yaml.h>

namespace VoidArchitect
{
    namespace Math
    {
        class Vec2;
        class Vec3;
        class Vec4;
        class Quat;
    }

    constexpr auto MAX_PORT_NUMBER = 65535;
    constexpr auto MIN_PORT_NUMBER = 1;

    /// @brief Thread-safe, YAML-native multi-file configuration management system
  ///
  /// `ConfigSystem` provides type-safe access to configuration values loaded
  /// from multiple YAML files with full hierarchical support, intelligent merging,
  /// source tracking, and thread-safe operations. Built on yaml-cpp for native
  /// YAML parsing and structure preservation.
  ///
  /// **Key features:**
  /// - Multi-file loading with intelligent merging (last file wins on conflicts)
  /// - Source tracking for each configuration key
  /// - Thread-safe read operations with shared_mutex
  /// - Native YAML structure support (arrays, nested objects)
  /// - Type-safe getters/setters with explicit error handling via TryGetXXX pattern
  /// - Hierarchical configuration via dot notation (e.g., "graphics.vsync")
  /// - Strongly-typed array support (string, int, bool, float, port arrays)
  /// - Math types support (Vec2, Vec3, Vec4, Quat)
  /// - Intelligent saving options (merged, origin files, modified keys only)
  ///
  /// **Thread Safety:**
  /// - All TryGetXXX methods are thread-safe for concurrent reads
  /// - LoadFromFile/LoadFromFiles require exclusive access (main thread preferred)
  /// - TrySetXXX methods require exclusive access (main thread only)
  /// - Designed for configuration loading in background jobs with main-thread finalization
  ///
  /// **Multi-file workflow:**
  /// @code
  /// // Load base configuration + user overrides
  /// config->LoadFromFiles({
  ///     "config/base.yaml",      // Base engine settings
  ///     "config/graphics.yaml",  // Graphics defaults
  ///     "config/user.yaml"       // User preferences (wins on conflicts)
  /// });
  ///
  /// // Runtime modifications via console/UI
  /// config->TrySetBool("graphics.debug_wireframe", true);
  /// config->TrySetVec3("camera.position", Math::Vec3(0, 5, -10));
  ///
  /// // Smart saving strategies
  /// config->SaveModifiedKeys("config/user_changes.yaml");  // Only new stuff
  /// config->SaveToOriginFiles();                           // Respect original structure
  /// config->SaveToFile("config/merged_snapshot.yaml");     // Everything merged
  /// @endcode
  ///
  /// **Configuration file examples:**
  /// @code{.yaml}
  /// # base.yaml - Engine defaults
  /// logging:
  ///   level: "info"
  /// graphics:
  ///   resolution: [1920, 1080]  # Vec2 as int array
  ///   vsync: false
  ///
  /// # graphics.yaml - Graphics-specific settings
  /// graphics:
  ///   vsync: true               # Overrides base.yaml
  ///   api: "vulkan"             # New setting
  ///   camera_position: [0.0, 5.0, -10.0]  # Vec3
  ///   clear_color: [0.2, 0.3, 0.4, 1.0]   # Vec4
  ///   camera_rotation: [15.0, 45.0, 0.0]  # Quat (Euler degrees)
  ///
  /// # user.yaml - User overrides
  /// graphics:
  ///   debug_wireframe: true     # User-specific setting
  /// @endcode
  ///
  /// @see g_ConfigSystem Global instance for system-wide access
    class VA_API ConfigSystem
    {
    public:
        ConfigSystem() = default;
        ~ConfigSystem() = default;

        // Non-copyable and non-movable
        ConfigSystem(const ConfigSystem&) = delete;
        ConfigSystem& operator=(const ConfigSystem&) = delete;
        ConfigSystem(ConfigSystem&&) = delete;
        ConfigSystem& operator=(ConfigSystem&&) = delete;

        // === File Operations ===

        /// @brief Load configuration from a YAML file
        /// @param filePath Path to the YAML configuration file
        /// @return True, if the file was loaded successfully, false otherwise
        ///
        /// Loads configuration values from a YAML file, **merging** with any existing
        /// configuration. The merge behaviour is:
        /// - Keys present in file but not in memory: **Added**
        /// - Keys present in memory but not in file: **Preserved**
        /// - Keys present in both: **File wins** (overrides memory)
        ///
        /// **Thread Safety:** Main thread only - requires exclusive access
        ///
        /// **Error handling:**
        /// - File not found: Returns false, logs warning
        /// - Parse error: Returns false, logs error with YAML details
        /// - Partial success: Returns true, logs warnings for problematic sections
        ///
        /// **Example usage:**
        /// @code
        /// if (!g_ConfigSystem->LoadFromFile("config/client.yaml")) {
        ///     VA_ENGINE_WARN("Failed to load client configuration, using defaults");
        /// }
        /// @endcode
        bool LoadFromFile(const std::string& filePath);

        /// @brief Load configuration from multiple YAML files in sequence
        /// @param filePaths Array of file paths to load in order
        /// @return True if at least one file was loaded successfully, false if all failed
        ///
        /// Loads multiple configuration files sequentially, merging each with the existing
        /// configuration. Files are processed in array order, with later files taking
        /// precedence in case of key conflicts. Each file's source is tracked for
        /// intelligent saving operations.
        ///
        /// **Thread Safety:** Main thread only - requires exclusive access
        ///
        /// **Merge behaviour: **
        /// - Files are loaded in specified order
        /// - Later files override earlier files on key conflicts
        /// - Each key remembers its source file for SaveToOriginFiles()
        ///
        /// **Example usage:**
        /// @code
        /// VAArray<std::string> configFiles = {
        ///     "config/base.yaml",      // Base settings
        ///     "config/graphics.yaml",  // Graphics defaults
        ///     "config/user.yaml"       // User overrides
        /// };
        /// if (config->LoadFromFiles(configFiles)) {
        ///     VA_ENGINE_INFO("Multi-file configuration loaded successfully");
        /// }
        /// @endcode
        bool LoadFromFiles(const VAArray<std::string>& filePaths);

        /// @brief Save current configuration to YAML file
        /// @param filePath Path where to save the configuration file
        /// @return True, if the file was saved successfully, false otherwise
        ///
        /// Exports the current configuration state to a YAML file with proper
        /// formatting and structure preservation. Useful for saving modified
        /// configuration or creating configuration templates.
        ///
        /// **Thread Safety: ** Main thread only - requires shared read access
        ///
        /// @warning This method overwrites the file, thus removing all comments and formatting.
        bool SaveToFile(const std::string& filePath) const;

        /// @brief Save configuration back to original source files
        /// @return True, if all files were saved successfully, false otherwise
        ///
        /// Saves each configuration key back to its original source file, preserving
        /// the multi-file structure. Creates a complete reconstruction of the original
        /// file layout with current values (including modifications).
        ///
        /// **Thread Safety: ** Main thread only - requires shared read access
        ///
        /// **Behaviour: **
        /// - Keys are grouped by their source file
        /// - Each source file is reconstructed with its keys
        /// - Modified keys retain their modifications
        /// - Files maintain their original structure where possible
        ///
        /// **Example: **
        /// @code
        /// // After loading base.yaml, graphics.yaml, user.yaml
        /// config->TrySetBool("graphics.vsync", true);  // Modify a graphics setting
        /// config->SaveToOriginFiles();                 // Saves to graphics.yaml
        /// @endcode
        ///
        /// @warning This method overwrites the file, thus removing all comments and formatting.
        bool SaveToOriginFiles() const;

        /// @brief Save only modified keys to a specific file
        /// @param filePath Target file for modified keys
        /// @return True, if the file was saved successfully, false otherwise
        ///
        /// Saves only configuration keys that have been modified since loading
        /// to the specified file. Useful for creating user override files that
        /// contain only the changes from defaults.
        ///
        /// **Thread Safety: ** Main thread only - requires shared read access
        ///
        /// **Use case:** User preferences and runtime modifications
        /// @code
        /// // Load defaults, user modifies via console/UI
        /// config->TrySetBool("graphics.debug_wireframe", true);
        /// config->TrySetFloat("audio.master_volume", 0.8f);
        /// config->SaveModifiedKeys("config/user_overrides.yaml");
        /// @endcode
        ///
        /// @warning This method overwrites the file, thus removing all comments and formatting.
        bool SaveModifiedKeys(const std::string& filePath) const;

        /// @brief Save a specific key to a designated file
        /// @param key Configuration key to save
        /// @param filePath Target file for the key
        /// @return True if the key was saved successfully, false otherwise
        ///
        /// **Thread Safety: ** Main thread only - requires shared read access
        ///
        /// Saves a single configuration key to a specific file, useful for
        /// targeted configuration management and key migration between files.
        ///
        /// @warning This method overwrites the file, thus removing all comments and formatting.
        bool SaveKeyToFile(const std::string& key, const std::string& filePath) const;

        // === Type-Safe Getters ===

        /// @brief Get string configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param outValue Reference to store the retrieved value
        /// @return True if key exists and value retrieved successfully, false otherwise
        ///
        /// **Thread Safety: ** Safe for concurrent access from any thread
        ///
        /// **Example: **
        /// @code
        /// std::string api;
        /// if (config->TryGetString("graphics.api", api))
        /// {
        ///     renderer->SetAPI(api);
        /// }
        /// else
        /// {
        ///     VA_ENGINE_WARN("Graphics API not configured, using default");
        /// }
        /// @endcode
        bool TryGetString(const std::string& key, std::string& outValue) const;

        /// @brief Get integer configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param outValue Reference to store the retrieved value
        /// @return True if key exists and value converted successfully, false otherwise
        ///
        /// **Thread Safety: ** Safe for concurrent access from any thread
        ///
        /// Handles YAML integer conversion with validation. Logs warnings for
        /// invalid integer values (e.g. non-numeric strings, out-of-range values).
        bool TryGetInt(const std::string& key, int32_t& outValue) const;

        /// @brief Get port number configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param outValue Reference to store the retrieved value
        /// @return True if key exists and value is valid port (1-65 535), false otherwise
        ///
        /// **Thread Safety: ** Safe for concurrent access from any thread
        ///
        /// Validates that the integer value is within the valid port range (1-65 535).
        /// Logs warnings for out-of-range values.
        bool TryGetPort(const std::string& key, uint16_t& outValue) const;

        /// @brief Get boolean configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param outValue Reference to store the retrieved value
        /// @return True if the key exists and value converted successfully, false otherwise
        ///
        /// **Thread Safety: ** Safe for concurrent access from any thread
        ///
        /// Supports multiple boolean representations from YAML:
        /// - YAML native: true/false, True/False, TRUE/FALSE
        /// - String values: "yes"/"no", "on"/"off", "enabled"/"disabled"
        /// - Numeric: 1/0
        bool TryGetBool(const std::string& key, bool& outValue) const;

        /// @brief Get floating-point configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param outValue Reference to store the retrieved value
        /// @return True if key exists and value converted successfully, false otherwise
        ///
        /// **Thread Safety: ** Safe for concurrent access from any thread
        ///
        /// Handles YAML numeric conversion with validation. Logs warnings for
        /// invalid floating-point values.
        bool TryGetFloat(const std::string& key, float& outValue) const;

        /// @brief Get Vec2 configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param outValue Reference to store the retrieved Vec2
        /// @return True if key exists and value converted successfully, false otherwise
        ///
        /// **Thread Safety: ** Safe for concurrent access from any thread
        ///
        /// **YAML format: ** Array of 2 floats: `[x, y]`
        /// @code{.yaml}
        /// position: [1.0, 2.5]
        /// @endcode
        bool TryGetVec2(const std::string& key, Math::Vec2& outValue) const;

        /// @brief Get Vec3 configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param outValue Reference to store the retrieved Vec3
        /// @return True if key exists and value converted successfully, false otherwise
        ///
        /// **Thread Safety: ** Safe for concurrent access from any thread
        ///
        /// **YAML format: ** Array of 3 floats: `[x, y, z]`
        /// @code{.yaml}
        /// position: [1.0, 2.5, -3.0]
        /// @endcode
        bool TryGetVec3(const std::string& key, Math::Vec3& outValue) const;

        /// @brief Get Vec4 configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param outValue Reference to store the retrieved Vec4
        /// @return True if the key exists and value converted successfully, false otherwise
        ///
        /// **Thread Safety: ** Safe for concurrent access from any thread
        ///
        /// **YAML format: ** Array of 4 floats: `[x, y, z, w]`
        /// @code{.yaml}
        /// colour: [1.0, 0.5, 0.2, 1.0]
        /// @endcode
        bool TryGetVec4(const std::string& key, Math::Vec4& outValue) const;

        /// @brief Get Quaternion configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param outValue Reference to store the retrieved Quaternion
        /// @return True if key exists and value converted successfully, false otherwise
        ///
        /// **Thread Safety: ** Safe for concurrent access from any thread
        ///
        /// **YAML format: ** Array of 3 floats for Euler angles (pitch, yaw, roll): `[x, y, z]`
        /// @code{.yaml}
        /// rotation: [0.0, 45.0, 0.0]  # pitch=0°, yaw=45°, roll=0°
        /// @endcode
        bool TryGetQuat(const std::string& key, Math::Quat& outValue) const;

        // === Strongly-Typed Array Getters ===

        /// @brief Get string array configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param outArray Reference to store the retrieved array
        /// @return True if key exists and is a valid string array, false otherwise
        ///
        /// **Thread Safety: ** Safe for concurrent access from any thread
        ///
        /// **YAML Example: **
        /// @code{.yaml}
        /// servers:
        ///   - "server1.example.com"
        ///   - "server2.example.com"
        /// @endcode
        ///
        /// **Usage: **
        /// @code
        /// VAArray<std::string> servers;
        /// if (config->TryGetStringArray("servers", servers)) {
        ///     for (const auto& server: servers) {
        ///         ConnectToServer(server);
        ///     }
        /// }
        /// @endcode
        bool TryGetStringArray(const std::string& key, VAArray<std::string>& outArray) const;

        /// @brief Get integer array configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param outArray Reference to store the retrieved array
        /// @return True if a key exists and all elements are valid integers, false otherwise
        ///
        /// **Thread Safety: ** Safe for concurrent access from any thread
        ///
        /// Validates that all array elements can be converted to integers.
        /// Returns false if any element conversion fails.
        bool TryGetIntArray(const std::string& key, VAArray<int32_t>& outArray) const;

        /// @brief Get floating-point array configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param outArray Reference to store the retrieved array
        /// @return True if a key exists and all elements are valid floats, false otherwise
        ///
        /// **Thread Safety: ** Safe for concurrent access from any thread
        ///
        /// Validates that all array elements can be converted to floating-point numbers.
        bool TryGetFloatArray(const std::string& key, VAArray<float>& outArray) const;

        /// @brief Get boolean array configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param outArray Reference to store the retrieved array
        /// @return True if a key exists and all elements are valid booleans, false otherwise
        ///
        /// **Thread Safety: ** Safe for concurrent access from any thread
        ///
        /// Applies the same boolean conversion rules as TryGetBool() to each array element.
        bool TryGetBoolArray(const std::string& key, VAArray<bool>& outArray) const;

        /// @brief Get port array configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param outArray Reference to store the retrieved array
        /// @return True if the key exists and all elements are valid ports (1-65,535), false otherwise
        ///
        /// **Thread Safety: ** Safe for concurrent access from any thread
        ///
        /// Validates that all array elements are valid port numbers within range 1-65,535.
        /// Returns false if any element is out of range.
        ///
        /// **YAML Example: **
        /// @code{.yaml}
        /// server_ports: [8080, 8081, 8082]
        /// @endcode
        bool TryGetPortArray(const std::string& key, VAArray<uint16_t>& outArray) const;

        // === Setters ===

        /// @brief Set string configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param value Value to set
        /// @return True if value was set successfully, false otherwise
        ///
        /// **Thread Safety: ** Main thread only - requires exclusive access
        ///
        /// Creates nested structure as needed for dot notation keys.
        bool TrySetString(const std::string& key, const std::string& value);

        /// @brief Set integer configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param value Value to set
        /// @return True if value was set successfully, false otherwise
        ///
        /// **Thread Safety: ** Main thread only - requires exclusive access
        bool TrySetInt(const std::string& key, int32_t value);

        /// @brief Set port configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param value Port value to set (validated to be in range 1-65,535)
        /// @return True if value was set successfully, false otherwise
        ///
        /// **Thread Safety: ** Main thread only - requires exclusive access
        bool TrySetPort(const std::string& key, uint16_t value);

        /// @brief Set boolean configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param value Value to set
        /// @return True if value was set successfully, false otherwise
        ///
        /// **Thread Safety: ** Main thread only - requires exclusive access
        bool TrySetBool(const std::string& key, bool value);

        /// @brief Set floating-point configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param value Value to set
        /// @return True if value was set successfully, false otherwise
        ///
        /// **Thread Safety: ** Main thread only - requires exclusive access
        bool TrySetFloat(const std::string& key, float value);

        /// @brief Set Vec2 configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param value Vec2 value to set
        /// @return True if value was set successfully, false otherwise
        ///
        /// **Thread Safety: ** Main thread only - requires exclusive access
        ///
        /// **Storage format: ** YAML array [x, y]
        bool TrySetVec2(const std::string& key, const Math::Vec2& value);

        /// @brief Set Vec3 configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param value Vec3 value to set
        /// @return True if value was set successfully, false otherwise
        ///
        /// **Thread Safety: ** Main thread only - requires exclusive access
        ///
        /// **Storage format: ** YAML array [x, y, z]
        bool TrySetVec3(const std::string& key, const Math::Vec3& value);

        /// @brief Set Vec4 configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param value Vec4 value to set
        /// @return True if value was set successfully, false otherwise
        ///
        /// **Thread Safety: ** Main thread only - requires exclusive access
        ///
        /// **Storage format: ** YAML array [x, y, z, w]
        bool TrySetVec4(const std::string& key, const Math::Vec4& value);

        /// @brief Set Quaternion configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param value Quaternion value to set
        /// @return True if value was set successfully, false otherwise
        ///
        /// **Thread Safety: ** Main thread only - requires exclusive access
        ///
        /// **Storage format: ** YAML array [pitch, yaw, roll] in degrees
        bool TrySetQuat(const std::string& key, const Math::Quat& value);

        // === Array Setters ===

        /// @brief Set string array configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param value Array of strings to set
        /// @return True if value was set successfully, false otherwise
        ///
        /// **Thread Safety: ** Main thread only - requires exclusive access
        bool TrySetStringArray(const std::string& key, const VAArray<std::string>& value);

        /// @brief Set integer array configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param value Array of integers to set
        /// @return True if value was set successfully, false otherwise
        ///
        /// **Thread Safety: ** Main thread only - requires exclusive access
        bool TrySetIntArray(const std::string& key, const VAArray<int32_t>& value);

        /// @brief Set a boolean array configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param value Array of booleans to set
        /// @return True if value was set successfully, false otherwise
        ///
        /// **Thread Safety: ** Main thread only - requires exclusive access
        bool TrySetBoolArray(const std::string& key, const VAArray<bool>& value);

        /// @brief Set float array configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param value Array of floats to set
        /// @return True if value was set successfully, false otherwise
        ///
        /// **Thread Safety: ** Main thread only - requires exclusive access
        bool TrySetFloatArray(const std::string& key, const VAArray<float>& value);

        /// @brief Set port array configuration value
        /// @param key Configuration key (supports dot notation)
        /// @param value Array of ports to set (all validated to be in range 1-65,535)
        /// @return True if value was set successfully, false otherwise
        ///
        /// **Thread Safety: ** Main thread only - requires exclusive access
        bool TrySetPortArray(const std::string& key, const VAArray<uint16_t>& value);

        // === Utility Methods ===

        /// @brief Check if a configuration key exists
        /// @param key Configuration key to check (supports dot notation)
        /// @return True if the key exists, false otherwise
        ///
        /// **Thread Safety: ** Safe for concurrent access from any thread
        ///
        /// Useful for conditional configuration loading and feature detection.
        bool HasKey(const std::string& key) const;

        /// @brief Remove a configuration key
        /// @param key Configuration key to remove (supports dot notation)
        /// @return True if the key was removed, false if the key didn't exist
        ///
        /// **Thread Safety: ** Main thread only - requires exclusive access
        ///
        /// Removes the specified key and its value from the configuration.
        /// Useful for dynamic configuration management and in-game console operations.
        /// If removing a nested key results in empty parent objects, the parent
        /// structure is preserved (not automatically cleaned up).
        ///
        /// **Example: **
        /// @code
        /// if (config->RemoveKey("graphics.experimental_feature")) {
        ///     VA_ENGINE_INFO("Experimental feature disabled");
        /// }
        /// @endcode
        bool RemoveKey(const std::string& key);

        /// @brief Clear all configuration values
        ///
        /// **Thread Safety: ** Main thread only - requires exclusive access
        ///
        /// Removes all loaded configuration. Useful for testing or
        /// complete configuration reload scenarios.
        void Clear();

        /// @brief Get count of top-level configuration entries
        /// @return Number of top-level configuration keys
        ///
        /// **Thread Safety:** Safe for concurrent access from any thread
        ///
        /// Note: Returns count of top-level keys only, not total number of
        /// nested values throughout the entire configuration tree.
        size_t GetCount() const;

        /// @brief Get a list of loaded configuration files
        /// @return Array of file paths that have been loaded
        ///
        /// **Thread Safety: ** Safe for concurrent access from any thread
        ///
        /// Returns the files loaded via LoadFromFile() or LoadFromFiles()
        /// in the order they were loaded. Useful for debugging and
        /// configuration introspection.
        VAArray<std::string> GetLoadedFiles() const;

        /// @brief Get a list of modified configuration keys
        /// @return Array of keys that have been modified since loading
        ///
        /// **Thread Safety: ** Safe for concurrent access from any thread
        ///
        /// Returns all configuration keys that have been modified via
        /// TrySetXXX methods since the last LoadFromFile operation.
        /// Useful for configuration debugging and selective saving.
        VAArray<std::string> GetModifiedKeys() const;

        /// @brief Get the source file for a specific configuration key
        /// @param key Configuration key to query
        /// @return Source file path, or empty string if key not found
        ///
        /// **Thread Safety: ** Safe for concurrent access from any thread
        ///
        /// Returns the file path where the specified key was originally loaded from.
        /// Useful for configuration debugging and understanding key precedence.
        std::string GetKeySourceFile(const std::string& key) const;

    private:
        /// @brief Configuration entry with source tracking and modification state
        ///
        /// Each configuration key is stored with metadata about its origin
        /// and modification status for intelligent saving operations.
        struct ConfigEntry
        {
            YAML::Node value; ///< Actual configuration value
            std::string sourceFile; ///< File path where this key originated
            bool modified = false; ///< Whether this key has been modified since the last load

            ConfigEntry() = default;
            ConfigEntry(const YAML::Node& val, std::string source);
        };

        // === Private Helper Methods ===

        /// @brief Navigate to a configuration entry using a dot notation path
        /// @param key Dot-separated key path (e.g., "graphics.resolution")
        /// @return Pointer to ConfigEntry if found, nullptr otherwise
        ///
        /// **Internal helper for all getter methods**
        ///
        /// Performs direct lookup in the flat key storage using dot notation.
        /// Thread-safe when called with the appropriate lock held by calling method.
        const ConfigEntry* GetEntryAtPath(const std::string& key) const;

        /// @brief Set a configuration entry at the specified path
        /// @param key Dot-separated key path
        /// @param value YAML node to set at the path
        /// @param sourceFile Source file for tracking (empty for programmatic sets)
        /// @param markModified Whether to mark the entry as modified
        /// @return True if the value was set successfully, false otherwise
        ///
        /// **Internal helper for all setter methods**
        ///
        /// Creates or updates a configuration entry with proper source tracking
        /// and modification flags. Thread-safe when called with the appropriate
        /// exclusive lock held by calling method.
        bool SetEntryAtPath(
            const std::string& key,
            const YAML::Node& value,
            const std::string& sourceFile = "",
            bool markModified = true);

        /// @brief Convert YAML node tree to flat key-value entries
        /// @param node YAML node to flatten
        /// @param sourceFile Source file for all entries
        /// @param prefix Current key prefix for recursion
        ///
        /// **Internal helper for LoadFromFile operations**
        ///
        /// Recursively traverses a YAML node tree and converts it to flat
        /// dot-notation keys stored in m_ConfigEntries. Handles nested
        /// objects and arrays appropriately.
        void FlattenYamlNode(
            const YAML::Node& node,
            const std::string& sourceFile,
            const std::string& prefix = "");

        /// @brief Reconstruct the YAML tree from flat entries for a specific source file
        /// @param sourceFile File to reconstruct
        /// @return YAML node tree containing all keys from the specified source
        ///
        /// **Internal helper for SaveToOriginFiles**
        ///
        /// Rebuilds a hierarchical YAML structure from flat entries that
        /// originated from the specified source file.
        YAML::Node ReconstructYamlForFile(const std::string& sourceFile) const;

        /// @brief Split the dot notation key into path components
        /// @param key Configuration key with potential dot notation
        /// @return Vector of key path parts
        ///
        /// **Internal utility for path navigation**
        ///
        /// Converts "graphics.resolution" -> ["graphics", "resolution"]
        /// Handles edge cases like empty components and escaped dots if needed.
        static VAArray<std::string> SplitKeyPath(const std::string& key);

        /// @brief Template helper for strongly typed array conversion
        /// @tparam T Target array element type
        /// @param key Configuration key for error reporting
        /// @param node YAML node containing the array
        /// @param outArray Reference to store the converted array
        /// @return True if all elements converted successfully, false otherwise
        ///
        /// **Internal template method for array type conversion**
        ///
        /// Provides a common implementation for all TryGetXXXArray methods with
        /// type-specific validation and conversion logic. Handles YAML exceptions
        /// and provides detailed error logging.
        template <typename T>
        bool ConvertArrayFromNode(
            const std::string& key,
            const YAML::Node& node,
            VAArray<T>& outArray) const;

        /// @brief Set a YAML node value from a flat key path
        /// @param rootNode Root YAML node to modify
        /// @param key Dot-notation key path
        /// @param value Value to set
        ///
        /// **Internal helper for YAML reconstruction**
        ///
        /// Creates the necessary nested structure and sets the value at the final path.
        /// Used by SaveToFile operations to rebuild hierarchical YAML from flat entries.
        void SetYamlNodeFromPath(
            YAML::Node& rootNode,
            const std::string& key,
            const YAML::Node& value) const;

        void SetYamlNodeFromPath(
            YAML::Node node,
            const VAArray<std::string>& path,
            size_t index,
            const YAML::Node& value) const;

        // === Private Fields ===

        /// @brief Thread-safe mutex for configuration access
        ///
        /// Uses shared_mutex to allow multiple concurrent readers while
        /// ensuring exclusive access for writers (LoadFromFile, Clear).
        ///
        /// - Shared lock: All TryGetXXX methods, HasKey, GetCount
        /// - Exclusive lock: LoadFromFile, Clear, SaveToFile
        mutable std::shared_mutex m_Mutex;

        /// @brief Flat storage for configuration entries with source tracking
        ///
        /// Maps dot-notation keys to ConfigEntry structures containing the value,
        /// source file, and modification status. This enables intelligent saving
        /// while maintaining the same hierarchical access patterns.
        VAHashMap<std::string, ConfigEntry> m_ConfigEntries;

        /// @brief List of loaded configuration files in load order
        ///
        /// Tracks all files that have been loaded to support `SaveToOriginFiles()`
        /// and provide debugging information about configuration sources.
        VAArray<std::string> m_LoadedFiles;
    };

    /// @brief Global configuration system instance
    ///
    /// Initialized during application startup and available throughout
    /// the application lifetime for both client and server applications.
    extern VA_API std::unique_ptr<ConfigSystem> g_ConfigSystem;
}
