//
// Created by Michael Desmedt on 15/07/2025.
//
#include "ConfigSystem.hpp"

#include "VoidArchitect/Engine/Common/Logger.hpp"
#include "VoidArchitect/Engine/Common/Math/Constants.hpp"
#include "VoidArchitect/Engine/Common/Math/Quat.hpp"
#include "VoidArchitect/Engine/Common/Math/Vec2.hpp"
#include "VoidArchitect/Engine/Common/Math/Vec3.hpp"
#include "VoidArchitect/Engine/Common/Math/Vec4.hpp"

#include <yaml-cpp/yaml.h>

namespace VoidArchitect
{
    // Global ConfigSystem instance
    std::unique_ptr<ConfigSystem> g_ConfigSystem = nullptr;

    //=========================================================================
    // File Operations
    //=========================================================================

    bool ConfigSystem::LoadFromFile(const std::string& filePath)
    {
        VA_ENGINE_INFO("[ConfigSystem] Loading config from file '{}'.", filePath);

        // Acquire exclusive lock for configuration modification
        const std::unique_lock lock(m_Mutex);

        try
        {
            // Load and parse YAML file
            auto fileNode = YAML::LoadFile(filePath);

            // Flatten the YAML structure and store with source tracking
            FlattenYamlNode(fileNode, filePath);

            // Track this file as loaded
            if (std::ranges::find(m_LoadedFiles, filePath) == m_LoadedFiles.end())
            {
                m_LoadedFiles.push_back(filePath);
            }

            VA_ENGINE_INFO(
                "[ConfigSystem] Successfully loaded configuration from {} ({} total entries)",
                filePath,
                m_ConfigEntries.size());

            return true;
        }
        catch (const YAML::BadFile& e)
        {
            VA_ENGINE_WARN(
                "[ConfigSystem] Failed to load config from file '{}': {}",
                filePath,
                e.what());
            return false;
        }
        catch (const YAML::ParserException& e)
        {
            VA_ENGINE_ERROR(
                "[ConfigSystem] YAML parsing error in {}: {} (line: {}, column: {})",
                filePath,
                e.what(),
                e.mark.line,
                e.mark.column);
            return false;
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[ConfigSystem] Unexpected error loading {}: {}", filePath, e.what());
            return false;
        }
    }

    bool ConfigSystem::LoadFromFiles(const VAArray<std::string>& filePaths)
    {
        VA_ENGINE_INFO("[ConfigSystem] Loading config from {} files.", filePaths.size());

        bool anySuccess = false;
        for (const auto& filePath : filePaths)
        {
            if (LoadFromFile(filePath))
            {
                anySuccess = true;
            }
        }

        if (anySuccess)
        {
            VA_ENGINE_INFO(
                "[ConfigSystem] Successfully loaded configuration from {} files.",
                filePaths.size());
        }
        else
        {
            VA_ENGINE_ERROR(
                "[ConfigSystem] Failed to load configuration from {} files.",
                filePaths.size());
        }

        return anySuccess;
    }

    bool ConfigSystem::SaveToFile(const std::string& filePath) const
    {
        VA_ENGINE_INFO("[ConfigSystem] Saving config to file '{}'.", filePath);

        // Acquire shared lock for reading configuration
        const std::shared_lock lock(m_Mutex);

        try
        {
            std::ofstream file(filePath);
            if (!file.is_open())
            {
                VA_ENGINE_ERROR("[ConfigSystem] Failed to open file for writing: {}", filePath);
                return false;
            }

            // Reconstruct the full YAML tree from all entries
            YAML::Node rootNode(YAML::NodeType::Map);
            for (const auto& [key, entry] : m_ConfigEntries)
            {
                SetYamlNodeFromPath(rootNode, key, entry.value);
            }

            // Write YAML content with proper formatting
            file << "# VoidArchitect Configuration File" << std::endl;
            file << rootNode << std::endl;

            VA_ENGINE_INFO("[ConfigSystem] Successfully saved configuration to {}.", filePath);
            return true;
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR(
                "[ConfigSystem] Failed to save config to file '{}': {}",
                filePath,
                e.what());
            return false;
        }
    }

    bool ConfigSystem::SaveToOriginFiles() const
    {
        VA_ENGINE_INFO("[ConfigSystem] Saving config to origin files.");

        // Acquire shared lock for reading configuration
        const std::shared_lock lock(m_Mutex);

        bool allSuccess = true;

        // Group keys by source file
        VAHashMap<std::string, VAArray<std::string>> fileToKeys;
        for (const auto& [key, entry] : m_ConfigEntries)
        {
            if (!entry.sourceFile.empty())
            {
                fileToKeys[entry.sourceFile].push_back(key);
            }
        }

        // Save each file
        for (const auto& [filePath, keys] : fileToKeys)
        {
            try
            {
                auto fileNode = ReconstructYamlForFile(filePath);

                std::ofstream file(filePath);
                if (!file.is_open())
                {
                    VA_ENGINE_ERROR("[ConfigSystem] Failed to open file for writing: {}", filePath);
                    allSuccess = false;
                    continue;
                }

                file << "# VoidArchitect Configuration File" << std::endl;
                file << fileNode << std::endl;

                VA_ENGINE_TRACE(
                    "[ConfigSystem] Saved {} keys to file '{}'.",
                    keys.size(),
                    filePath);
            }
            catch (const std::exception& e)
            {
                VA_ENGINE_ERROR(
                    "[ConfigSystem] Failed to save config to file '{}': {}",
                    filePath,
                    e.what());
                allSuccess = false;
            }
        }

        if (allSuccess)
        {
            VA_ENGINE_INFO("[ConfigSystem] Successfully saved configuration to origin files.");
        }
        else
        {
            VA_ENGINE_WARN("[ConfigSystem] Failed to save configuration to origin files.");
        }

        return allSuccess;
    }

    bool ConfigSystem::SaveModifiedKeys(const std::string& filePath) const
    {
        VA_ENGINE_INFO("[ConfigSystem] Saving modified keys to file '{}'.", filePath);

        // Acquire shared lock for reading configuration
        const std::shared_lock lock(m_Mutex);

        try
        {
            YAML::Node modifiedNode(YAML::NodeType::Map);
            size_t modifiedCount = 0;

            // Collect only modified keys
            for (const auto& [key, entry] : m_ConfigEntries)
            {
                if (entry.modified)
                {
                    SetYamlNodeFromPath(modifiedNode, key, entry.value);
                    modifiedCount++;
                }
            }

            if (modifiedCount == 0)
            {
                VA_ENGINE_INFO("[ConfigSystem] No modified keys to save.");
                return true;
            }

            std::ofstream file(filePath);
            if (!file.is_open())
            {
                VA_ENGINE_ERROR("[ConfigSystem] Failed to open file for writing: {}", filePath);
                return false;
            }

            file << "# VoidArchitect Configuration File" << std::endl;
            file << modifiedNode << std::endl;

            VA_ENGINE_INFO(
                "[ConfigSystem] Saved {} modified keys to file '{}'.",
                modifiedCount,
                filePath);
            return true;
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[ConfigSystem] Failed to save modified keys: {}", e.what());
            return false;
        }
    }

    bool ConfigSystem::SaveKeyToFile(const std::string& key, const std::string& filePath) const
    {
        VA_ENGINE_INFO("[ConfigSystem] Saving key '{}' to file '{}'.", key, filePath);

        // Acquire shared lock for reading configuration
        const std::shared_lock lock(m_Mutex);

        const auto* entry = GetEntryAtPath(key);
        if (!entry)
        {
            VA_ENGINE_WARN("[ConfigSystem] Key '{}' not found.", key);
            return false;
        }

        try
        {
            YAML::Node singleKeyNode(YAML::NodeType::Map);
            SetYamlNodeFromPath(singleKeyNode, key, entry->value);

            std::ofstream file(filePath);
            if (!file.is_open())
            {
                VA_ENGINE_ERROR("[ConfigSystem] Failed to open file for writing: {}", filePath);
                return false;
            }

            file << "# VoidArchitect Configuration File" << std::endl;
            file << singleKeyNode << std::endl;

            VA_ENGINE_INFO("[ConfigSystem] Saved key '{}' to file '{}'.", key, filePath);
            return true;
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR(
                "[ConfigSystem] Failed to save key '{}' to file '{}': {}",
                key,
                filePath,
                e.what());
            return false;
        }
    }

    //=========================================================================
    // Type-Safe Getters
    //=========================================================================

    bool ConfigSystem::TryGetString(const std::string& key, std::string& outValue) const
    {
        const std::shared_lock lock(m_Mutex);

        const auto* entry = GetEntryAtPath(key);
        if (!entry || !entry->value.IsScalar())
        {
            VA_ENGINE_WARN("[ConfigSystem] Key '{}' not found or not a string.", key);
            return false;
        }

        try
        {
            outValue = entry->value.as<std::string>();
            return true;
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR(
                "[ConfigSystem] Failed to convert key '{}' to string: {}",
                key,
                e.what());
            return false;
        }
    }

    bool ConfigSystem::TryGetInt(const std::string& key, int32_t& outValue) const
    {
        const std::shared_lock lock(m_Mutex);

        const auto* entry = GetEntryAtPath(key);
        if (!entry || !entry->value.IsScalar())
        {
            VA_ENGINE_WARN("[ConfigSystem] Key '{}' not found or not an integer.", key);
            return false;
        }

        try
        {
            outValue = entry->value.as<int32_t>();
            return true;
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR(
                "[ConfigSystem] Failed to convert key '{}' to integer: {}",
                key,
                e.what());
            return false;
        }
    }

    bool ConfigSystem::TryGetPort(const std::string& key, uint16_t& outValue) const
    {
        const std::shared_lock lock(m_Mutex);

        const auto* entry = GetEntryAtPath(key);
        if (!entry || !entry->value.IsScalar())
        {
            VA_ENGINE_WARN("[ConfigSystem] Key '{}' not found or not a port.", key);
            return false;
        }

        try
        {
            auto portValue = entry->value.as<int32_t>();
            if (portValue < MIN_PORT_NUMBER || portValue > MAX_PORT_NUMBER)
            {
                VA_ENGINE_ERROR(
                    "[ConfigSystem] Port value '{}' for key '{}' is out of range.",
                    portValue,
                    key);
                return false;
            }

            outValue = static_cast<uint16_t>(portValue);
            return true;
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[ConfigSystem] Failed to convert key '{}' to port: {}", key, e.what());
            return false;
        }
    }

    bool ConfigSystem::TryGetBool(const std::string& key, bool& outValue) const
    {
        const std::shared_lock lock(m_Mutex);

        const auto* entry = GetEntryAtPath(key);
        if (!entry || !entry->value.IsScalar())
        {
            VA_ENGINE_WARN("[ConfigSystem] Key '{}' not found or not a boolean.", key);
            return false;
        }

        try
        {
            // Try YAML native boolean conversion first
            if (entry->value.Tag() == "tag:yaml.org,2002:bool")
            {
                outValue = entry->value.as<bool>();
                return true;
            }

            // Fallback to string-based boolean conversion for flexibility
            auto stringValue = entry->value.as<std::string>();
            std::ranges::transform(
                stringValue,
                stringValue.begin(),
                [](unsigned char c) { return std::tolower(c); });

            if (stringValue == "true" || stringValue == "yes" || stringValue == "on" || stringValue
                == "enabled" || stringValue == "1")
            {
                outValue = true;
                return true;
            }
            else if (stringValue == "false" || stringValue == "no" || stringValue == "off" ||
                stringValue == "disabled" || stringValue == "0")
            {
                outValue = false;
                return true;
            }
            else
            {
                VA_ENGINE_ERROR(
                    "[ConfigSystem] Failed to convert key '{}' to boolean: {}",
                    key,
                    stringValue);
                return false;
            }
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR(
                "[ConfigSystem] Failed to convert key '{}' to boolean: {}",
                key,
                e.what());
            return false;
        }
    }

    bool ConfigSystem::TryGetFloat(const std::string& key, float& outValue) const
    {
        const std::shared_lock lock(m_Mutex);

        const auto* entry = GetEntryAtPath(key);
        if (!entry || !entry->value.IsScalar())
        {
            VA_ENGINE_WARN("[ConfigSystem] Key '{}' not found or not a float.", key);
            return false;
        }

        try
        {
            outValue = entry->value.as<float>();
            return true;
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR(
                "[ConfigSystem] Failed to convert key '{}' to float: {}",
                key,
                e.what());
            return false;
        }
    }

    bool ConfigSystem::TryGetVec2(const std::string& key, Math::Vec2& outValue) const
    {
        const std::shared_lock lock(m_Mutex);

        const auto* entry = GetEntryAtPath(key);
        if (!entry || !entry->value.IsSequence() || entry->value.size() != 2)
        {
            VA_ENGINE_WARN("[ConfigSystem] Key '{}' not found or not a vector.", key);
            return false;
        }

        try
        {
            auto x = entry->value[0].as<float>();
            auto y = entry->value[1].as<float>();

            outValue = Math::Vec2(x, y);
            return true;
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR(
                "[ConfigSystem] Failed to convert key '{}' to vector: {}",
                key,
                e.what());
            return false;
        }
    }

    bool ConfigSystem::TryGetVec3(const std::string& key, Math::Vec3& outValue) const
    {
        const std::shared_lock lock(m_Mutex);

        const auto* entry = GetEntryAtPath(key);
        if (!entry || !entry->value.IsSequence() || entry->value.size() != 3)
        {
            VA_ENGINE_WARN("[ConfigSystem] Key '{}' not found or not a vector.", key);
            return false;
        }

        try
        {
            auto x = entry->value[0].as<float>();
            auto y = entry->value[1].as<float>();
            auto z = entry->value[2].as<float>();

            outValue = Math::Vec3(x, y, z);
            return true;
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR(
                "[ConfigSystem] Failed to convert key '{}' to vector: {}",
                key,
                e.what());
            return false;
        }
    }

    bool ConfigSystem::TryGetVec4(const std::string& key, Math::Vec4& outValue) const
    {
        const std::shared_lock lock(m_Mutex);

        const auto* entry = GetEntryAtPath(key);
        if (!entry || !entry->value.IsSequence() || entry->value.size() != 4)
        {
            VA_ENGINE_WARN("[ConfigSystem] Key '{}' not found or not a vector.", key);
            return false;
        }

        try
        {
            auto x = entry->value[0].as<float>();
            auto y = entry->value[1].as<float>();
            auto z = entry->value[2].as<float>();
            auto w = entry->value[3].as<float>();

            outValue = Math::Vec4(x, y, z, w);
            return true;
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR(
                "[ConfigSystem] Failed to convert key '{}' to vector: {}",
                key,
                e.what());
            return false;
        }
    }

    bool ConfigSystem::TryGetQuat(const std::string& key, Math::Quat& outValue) const
    {
        const std::shared_lock lock(m_Mutex);

        const auto* entry = GetEntryAtPath(key);
        if (!entry || !entry->value.IsSequence() || entry->value.size() != 3)
        {
            VA_ENGINE_WARN("[ConfigSystem] Key '{}' not found or not a quaternion.", key);
            return false;
        }

        try
        {
            // Read as Euler angles in degrees
            auto pitch = entry->value[0].as<float>();
            auto yaw = entry->value[1].as<float>();
            auto roll = entry->value[2].as<float>();

            // Convert degrees to radians
            outValue = Math::Quat::FromEuler(
                pitch * Math::DEG2RAD,
                yaw * Math::DEG2RAD,
                roll * Math::DEG2RAD);
            return true;
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR(
                "[ConfigSystem] Failed to convert key '{}' to quaternion: {}",
                key,
                e.what());
            return false;
        }
    }

    bool ConfigSystem::TryGetStringArray(
        const std::string& key,
        VAArray<std::string>& outArray) const
    {
        const std::shared_lock lock(m_Mutex);

        const auto* entry = GetEntryAtPath(key);
        if (!entry || !entry->value.IsSequence())
        {
            VA_ENGINE_WARN("[ConfigSystem] Key '{}' not found or not an array.", key);
            return false;
        }

        return ConvertArrayFromNode(key, entry->value, outArray);
    }

    bool ConfigSystem::TryGetIntArray(const std::string& key, VAArray<int32_t>& outArray) const
    {
        const std::shared_lock lock(m_Mutex);

        const auto* entry = GetEntryAtPath(key);
        if (!entry || !entry->value.IsSequence())
        {
            VA_ENGINE_WARN("[ConfigSystem] Key '{}' not found or not an array.", key);
            return false;
        }

        return ConvertArrayFromNode(key, entry->value, outArray);
    }

    bool ConfigSystem::TryGetFloatArray(const std::string& key, VAArray<float>& outArray) const
    {
        const std::shared_lock lock(m_Mutex);

        const auto* entry = GetEntryAtPath(key);
        if (!entry || !entry->value.IsSequence())
        {
            VA_ENGINE_WARN("[ConfigSystem] Key '{}' not found or not an array.", key);
            return false;
        }

        return ConvertArrayFromNode(key, entry->value, outArray);
    }

    bool ConfigSystem::TryGetBoolArray(const std::string& key, VAArray<bool>& outArray) const
    {
        const std::shared_lock lock(m_Mutex);

        const auto* entry = GetEntryAtPath(key);
        if (!entry || !entry->value.IsSequence())
        {
            VA_ENGINE_WARN("[ConfigSystem] Key '{}' not found or not an array.", key);
            return false;
        }

        return ConvertArrayFromNode(key, entry->value, outArray);
    }

    bool ConfigSystem::TryGetPortArray(const std::string& key, VAArray<uint16_t>& outArray) const
    {
        const std::shared_lock lock(m_Mutex);

        const auto* entry = GetEntryAtPath(key);
        if (!entry || !entry->value.IsSequence())
        {
            VA_ENGINE_WARN("[ConfigSystem] Key '{}' not found or not an array.", key);
            return false;
        }

        return ConvertArrayFromNode(key, entry->value, outArray);
    }

    //=========================================================================
    // Type-Safe Setters
    //=========================================================================

    bool ConfigSystem::TrySetString(const std::string& key, const std::string& value)
    {
        const std::unique_lock lock(m_Mutex);

        try
        {
            YAML::Node valueNode;
            valueNode = value;
            return SetEntryAtPath(key, valueNode, "", true);
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[ConfigSystem] Failed to set key '{}' to string: {}", key, e.what());
            return false;
        }
    }

    bool ConfigSystem::TrySetInt(const std::string& key, int32_t value)
    {
        const std::unique_lock lock(m_Mutex);

        try
        {
            YAML::Node valueNode;
            valueNode = value;
            return SetEntryAtPath(key, valueNode, "", true);
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[ConfigSystem] Failed to set key '{}' to integer: {}", key, e.what());
            return false;
        }
    }

    bool ConfigSystem::TrySetPort(const std::string& key, uint16_t value)
    {
        if (value < MIN_PORT_NUMBER || value > MAX_PORT_NUMBER)
        {
            VA_ENGINE_ERROR(
                "[ConfigSystem] Port value '{}' for key '{}' is out of range.",
                value,
                key);
            return false;
        }

        const std::unique_lock lock(m_Mutex);

        try
        {
            YAML::Node valueNode;
            valueNode = value;
            return SetEntryAtPath(key, valueNode, "", true);
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[ConfigSystem] Failed to set key '{}' to port: {}", key, e.what());
            return false;
        }
    }

    bool ConfigSystem::TrySetBool(const std::string& key, bool value)
    {
        const std::unique_lock lock(m_Mutex);

        try
        {
            YAML::Node valueNode;
            valueNode = value;
            return SetEntryAtPath(key, valueNode, "", true);
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[ConfigSystem] Failed to set key '{}' to boolean: {}", key, e.what());
            return false;
        }
    }

    bool ConfigSystem::TrySetFloat(const std::string& key, float value)
    {
        const std::unique_lock lock(m_Mutex);

        try
        {
            YAML::Node valueNode;
            valueNode = value;
            return SetEntryAtPath(key, valueNode, "", true);
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[ConfigSystem] Failed to set key '{}' to float: {}", key, e.what());
            return false;
        }
    }

    bool ConfigSystem::TrySetVec2(const std::string& key, const Math::Vec2& value)
    {
        const std::unique_lock lock(m_Mutex);

        try
        {
            YAML::Node arrayNode;
            arrayNode.push_back(value.X());
            arrayNode.push_back(value.Y());
            return SetEntryAtPath(key, arrayNode, "", true);
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[ConfigSystem] Failed to set key '{}' to Vec2: {}", key, e.what());
            return false;
        }
    }

    bool ConfigSystem::TrySetVec3(const std::string& key, const Math::Vec3& value)
    {
        const std::unique_lock lock(m_Mutex);

        try
        {
            YAML::Node arrayNode;
            arrayNode.push_back(value.X());
            arrayNode.push_back(value.Y());
            arrayNode.push_back(value.Z());
            return SetEntryAtPath(key, arrayNode, "", true);
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[ConfigSystem] Failed to set key '{}' to Vec3: {}", key, e.what());
            return false;
        }
    }

    bool ConfigSystem::TrySetVec4(const std::string& key, const Math::Vec4& value)
    {
        const std::unique_lock lock(m_Mutex);

        try
        {
            YAML::Node arrayNode;
            arrayNode.push_back(value.X());
            arrayNode.push_back(value.Y());
            arrayNode.push_back(value.Z());
            arrayNode.push_back(value.W());
            return SetEntryAtPath(key, arrayNode, "", true);
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[ConfigSystem] Failed to set key '{}' to Vec4: {}", key, e.what());
            return false;
        }
    }

    bool ConfigSystem::TrySetQuat(const std::string& key, const Math::Quat& value)
    {
        const std::unique_lock lock(m_Mutex);

        try
        {
            // Convert to Euler angles in degrees for human-read storage
            const auto euler = value.ToEuler();

            YAML::Node arrayNode;
            arrayNode.push_back(euler.X() * Math::RAD2DEG);
            arrayNode.push_back(euler.Y() * Math::RAD2DEG);
            arrayNode.push_back(euler.Z() * Math::RAD2DEG);
            return SetEntryAtPath(key, arrayNode, "", true);
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[ConfigSystem] Failed to set key '{}' to Quat: {}", key, e.what());
            return false;
        }
    }

    //=========================================================================
    // Array Setters
    //=========================================================================

    bool ConfigSystem::TrySetStringArray(const std::string& key, const VAArray<std::string>& value)
    {
        const std::unique_lock lock(m_Mutex);

        try
        {
            YAML::Node arrayNode;
            for (const auto& str : value)
            {
                arrayNode.push_back(str);
            }
            return SetEntryAtPath(key, arrayNode, "", true);
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[ConfigSystem] Failed to set key '{}' to array: {}", key, e.what());
            return false;
        }
    }

    bool ConfigSystem::TrySetIntArray(const std::string& key, const VAArray<int32_t>& value)
    {
        const std::unique_lock lock(m_Mutex);

        try
        {
            YAML::Node arrayNode;
            for (const auto& i : value)
            {
                arrayNode.push_back(i);
            }
            return SetEntryAtPath(key, arrayNode, "", true);
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[ConfigSystem] Failed to set key '{}' to array: {}", key, e.what());
            return false;
        }
    }

    bool ConfigSystem::TrySetBoolArray(const std::string& key, const VAArray<bool>& value)
    {
        const std::unique_lock lock(m_Mutex);

        try
        {
            YAML::Node arrayNode;
            for (const bool b : value)
            {
                arrayNode.push_back(b);
            }
            return SetEntryAtPath(key, arrayNode, "", true);
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[ConfigSystem] Failed to set key '{}' to array: {}", key, e.what());
            return false;
        }
    }

    bool ConfigSystem::TrySetFloatArray(const std::string& key, const VAArray<float>& value)
    {
        const std::unique_lock lock(m_Mutex);

        try
        {
            YAML::Node arrayNode;
            for (const auto& f : value)
            {
                arrayNode.push_back(f);
            }
            return SetEntryAtPath(key, arrayNode, "", true);
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[ConfigSystem] Failed to set key '{}' to array: {}", key, e.what());
            return false;
        }
    }

    bool ConfigSystem::TrySetPortArray(const std::string& key, const VAArray<uint16_t>& value)
    {
        const std::unique_lock lock(m_Mutex);

        try
        {
            YAML::Node arrayNode;
            for (const auto& p : value)
            {
                arrayNode.push_back(p);
            }
            return SetEntryAtPath(key, arrayNode, "", true);
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[ConfigSystem] Failed to set key '{}' to array: {}", key, e.what());
            return false;
        }
    }

    //=========================================================================
    // Utility Methods
    //=========================================================================

    bool ConfigSystem::HasKey(const std::string& key) const
    {
        const std::shared_lock lock(m_Mutex);
        return GetEntryAtPath(key) != nullptr;
    }

    bool ConfigSystem::RemoveKey(const std::string& key)
    {
        const std::unique_lock lock(m_Mutex);

        auto it = m_ConfigEntries.find(key);
        if (it == m_ConfigEntries.end())
        {
            VA_ENGINE_WARN("[ConfigSystem] Key '{}' not found.", key);
            return false;
        }

        m_ConfigEntries.erase(it);
        return true;
    }

    void ConfigSystem::Clear()
    {
        const std::unique_lock lock(m_Mutex);

        size_t count = m_ConfigEntries.size();
        m_ConfigEntries.clear();
        m_LoadedFiles.clear();

        VA_ENGINE_INFO("[ConfigSystem] Cleared configuration ({} entries removed)", count);
    }

    size_t ConfigSystem::GetCount() const
    {
        const std::shared_lock lock(m_Mutex);
        return m_ConfigEntries.size();
    }

    VAArray<std::string> ConfigSystem::GetLoadedFiles() const
    {
        const std::shared_lock lock(m_Mutex);
        return m_LoadedFiles;
    }

    VAArray<std::string> ConfigSystem::GetModifiedKeys() const
    {
        const std::shared_lock lock(m_Mutex);

        VAArray<std::string> modifiedKeys;
        for (const auto& [key, entry] : m_ConfigEntries)
        {
            if (entry.modified)
            {
                modifiedKeys.push_back(key);
            }
        }
        return modifiedKeys;
    }

    std::string ConfigSystem::GetKeySourceFile(const std::string& key) const
    {
        const std::shared_lock lock(m_Mutex);

        const ConfigEntry* entry = GetEntryAtPath(key);
        return entry ? entry->sourceFile : "";
    }

    ConfigSystem::ConfigEntry::ConfigEntry(const YAML::Node& val, std::string source)
        : value(val),
          sourceFile(std::move(source))
    {
    }

    const ConfigSystem::ConfigEntry* ConfigSystem::GetEntryAtPath(const std::string& key) const
    {
        const auto it = m_ConfigEntries.find(key);
        return (it != m_ConfigEntries.end()) ? &it->second : nullptr;
    }

    bool ConfigSystem::SetEntryAtPath(
        const std::string& key,
        const YAML::Node& value,
        const std::string& sourceFile,
        bool markModified)
    {
        try
        {
            auto it = m_ConfigEntries.find(key);
            if (it != m_ConfigEntries.end())
            {
                // Update existing entry
                it->second.value = value;
                if (markModified)
                {
                    it->second.modified = true;
                }

                // Update the source file only if provided
                if (!sourceFile.empty())
                {
                    it->second.sourceFile = sourceFile;
                }
            }
            else
            {
                // Create a new entry
                ConfigEntry newEntry(value, sourceFile);
                newEntry.modified = markModified;
                m_ConfigEntries[key] = std::move(newEntry);
            }

            return true;
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR("[ConfigSystem] Failed to set key '{}' to value: {}", key, e.what());
            return false;
        }
    }

    void ConfigSystem::FlattenYamlNode(
        const YAML::Node& node,
        const std::string& sourceFile,
        const std::string& prefix)
    {
        if (node.IsMap())
        {
            for (const auto& pair : node)
            {
                auto key = pair.first.as<std::string>();
                auto fullKey = prefix.empty() ? key : prefix + "." += key;
                if (pair.second.IsMap() || pair.second.IsSequence())
                {
                    FlattenYamlNode(pair.second, sourceFile, fullKey);
                }
                else
                {
                    // Store scalar value with source tracking
                    SetEntryAtPath(fullKey, pair.second, sourceFile, false);
                }
            }
        }
        else if (node.IsSequence() || node.IsScalar())
        {
            // Store the entire sequence as a single entry, or the scalar immediately
            SetEntryAtPath(prefix, node, sourceFile, false);
        }
    }

    YAML::Node ConfigSystem::ReconstructYamlForFile(const std::string& sourceFile) const
    {
        YAML::Node fileNode(YAML::NodeType::Map);

        for (const auto& [key, entry] : m_ConfigEntries)
        {
            if (entry.sourceFile == sourceFile)
            {
                SetYamlNodeFromPath(fileNode, key, entry.value);
            }
        }

        return fileNode;
    }

    VAArray<std::string> ConfigSystem::SplitKeyPath(const std::string& key)
    {
        VAArray<std::string> components;

        if (key.empty())
        {
            return components;
        }

        std::stringstream ss(key);
        std::string component;

        while (std::getline(ss, component, '.'))
        {
            if (!component.empty())
            {
                components.push_back(component);
            }
        }

        return components;
    }

    template <typename T>
    bool ConfigSystem::ConvertArrayFromNode(
        const std::string& key,
        const YAML::Node& node,
        VAArray<T>& outArray) const
    {
        try
        {
            outArray.clear();
            outArray.reserve(node.size());

            for (size_t i = 0; i < node.size(); ++i)
            {
                const auto& element = node[i];

                if constexpr (std::is_same_v<T, std::string>)
                {
                    if (!element.IsScalar())
                    {
                        VA_ENGINE_WARN(
                            "[ConfigSystem] Non-scalar element at index {} in array '{}'.",
                            i,
                            key);
                        return false;
                    }
                    outArray.push_back(element.as<std::string>());
                }
                else if constexpr (std::is_same_v<T, int32_t>)
                {
                    if (!element.IsScalar())
                    {
                        VA_ENGINE_WARN(
                            "[ConfigSystem] Non-scalar element at index {} in array '{}'.",
                            i,
                            key);
                        return false;
                    }
                    outArray.push_back(element.as<int32_t>());
                }
                else if constexpr (std::is_same_v<T, bool>)
                {
                    if (!element.IsScalar())
                    {
                        VA_ENGINE_WARN(
                            "[ConfigSystem] Non-scalar element at index {} in array '{}'.",
                            i,
                            key);
                        return false;
                    }

                    // Apply the same boolean conversion logic as TryGetBool
                    if (element.Tag() == "tag:yaml.org,2002:bool")
                    {
                        outArray.push_back(element.as<bool>());
                    }
                    else
                    {
                        auto stringValue = element.as<std::string>();
                        std::ranges::transform(stringValue, stringValue.begin(), ::tolower);

                        if (stringValue == "true" || stringValue == "yes" || stringValue == "on" ||
                            stringValue == "enabled" || stringValue == "1")
                        {
                            outArray.push_back(true);
                        }
                        else if (stringValue == "false" || stringValue == "no" || stringValue ==
                            "off" || stringValue == "disabled" || stringValue == "0")
                        {
                            outArray.push_back(false);
                        }
                        else
                        {
                            VA_ENGINE_WARN(
                                "[ConfigSystem] Invalid boolean value at index {} in array '{}' : '{}'.",
                                i,
                                key,
                                stringValue);
                            return false;
                        }
                    }
                }
                else if constexpr (std::is_same_v<T, float>)
                {
                    if (!element.IsScalar())
                    {
                        VA_ENGINE_WARN(
                            "[ConfigSystem] Non-scalar element at index {} in array '{}'.",
                            i,
                            key);
                        return false;
                    }
                    outArray.push_back(element.as<float>());
                }
                else if constexpr (std::is_same_v<T, uint16_t>)
                {
                    if (!element.IsScalar())
                    {
                        VA_ENGINE_WARN(
                            "[ConfigSystem] Non-scalar element at index {} in array '{}'.",
                            i,
                            key);
                        return false;
                    }

                    auto portValue = element.as<int32_t>();
                    if (portValue < MIN_PORT_NUMBER || portValue > MAX_PORT_NUMBER)
                    {
                        VA_ENGINE_WARN(
                            "[ConfigSystem] Port value out of range at index {} in array '{}' : {} (must be 1-65535).",
                            i,
                            key,
                            portValue);
                        return false;
                    }
                    outArray.push_back(static_cast<uint16_t>(portValue));
                }
            }

            return true;
        }
        catch (const std::exception& e)
        {
            VA_ENGINE_ERROR(
                "[ConfigSystem] Failed to convert array at key '{}': {}",
                key,
                e.what());
            return false;
        }
    }

    void ConfigSystem::SetYamlNodeFromPath(
        YAML::Node& rootNode,
        const std::string& key,
        const YAML::Node& value) const
    {
        const auto pathComponents = SplitKeyPath(key);
        if (pathComponents.empty())
        {
            return;
        }

        if (!rootNode.IsDefined())
        {
            rootNode = YAML::Node(YAML::NodeType::Map);
        }

        SetYamlNodeFromPath(rootNode, pathComponents, 0, value);
    }

    void ConfigSystem::SetYamlNodeFromPath(
        YAML::Node node,
        const VAArray<std::string>& path,
        const size_t index,
        const YAML::Node& value) const
    {
        if (index == path.size() - 1)
        {
            node[path[index]] = value;
        }
        else
        {
            const auto& component = path[index];
            if (!node[component].IsDefined())
            {
                node[component] = YAML::Node(YAML::NodeType::Map);
            }

            SetYamlNodeFromPath(node[component], path, index + 1, value);
        }
    }

    // Explicit template instantiations for supported array types
    template bool ConfigSystem::ConvertArrayFromNode<std::string>(
        const std::string& key,
        const YAML::Node& node,
        VAArray<std::string>& outArray) const;
    template bool ConfigSystem::ConvertArrayFromNode<int32_t>(
        const std::string& key,
        const YAML::Node& node,
        VAArray<int32_t>& outArray) const;
    template bool ConfigSystem::ConvertArrayFromNode<bool>(
        const std::string& key,
        const YAML::Node& node,
        VAArray<bool>& outArray) const;
    template bool ConfigSystem::ConvertArrayFromNode<float>(
        const std::string& key,
        const YAML::Node& node,
        VAArray<float>& outArray) const;
    template bool ConfigSystem::ConvertArrayFromNode<uint16_t>(
        const std::string& key,
        const YAML::Node& node,
        VAArray<uint16_t>& outArray) const;
}
