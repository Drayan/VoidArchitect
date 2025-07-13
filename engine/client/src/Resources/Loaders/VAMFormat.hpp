//
// Created by Michael Desmedt on 15/06/2025.
//
#pragma once

#include <VoidArchitect/Engine/Common/Core.hpp>
#include <VoidArchitect/Engine/Common/Math/Vec2.hpp>
#include <VoidArchitect/Engine/Common/Math/Vec3.hpp>
#include <VoidArchitect/Engine/Common/Math/Vec4.hpp>

namespace VoidArchitect::Resources::Loaders
{
    // VAM = "Void Architect Mesh" - Custom binary mesh format

    // Current format version
    static constexpr uint32_t VAM_VERSION = 1;
    static constexpr uint8_t VAM_MAGIC[4] = {'V', 'A', 'M', '\0'};

    enum class VAMFlags : uint32_t
    {
        None = 0,
        Compressed = BIT(0), // LZ4 compression (not implemented)
        // Reserved bits 1-31 for future features
    };

    // Main header - 80 bytes, 16-bytes aligned
    struct alignas(16) VAMHeader
    {
        uint8_t magic[4]; // VAM\0
        uint32_t version; // Format version (1)
        uint32_t flags; // VAMFlags bitfield
        uint64_t sourceTimestamp; // Source file modification time

        // Counts
        uint32_t vertexCount;
        uint32_t indexCount;
        uint32_t submeshCount;
        uint32_t materialCount;
        uint32_t stringTableSize; // Size in bytes

        // Section offsets (for validation and seeking)
        uint32_t stringTableOffset;
        uint32_t verticesOffset;
        uint32_t indicesOffset;
        uint32_t submeshesOffset;
        uint32_t materialsOffset;

        // Compression info
        uint32_t uncompressedSize; // Original size before compression (0 if not compressed)
        uint32_t compressionRatio; // Compression ratio * 1000 (e.g., 750 = 75.0% compression)

        // Original section sizes (for compressed format parsing)
        uint32_t originalStringTableSize;
        uint32_t originalVerticesSize;
        uint32_t originalIndicesSize;
        uint32_t originalSubmeshesSize;
        uint32_t originalMaterialsSize;
        uint32_t originalBindingsSize;

        // Validation helpers
        [[nodiscard]] bool IsValid() const
        {
            return memcmp(magic, VAM_MAGIC, 4) == 0 && version == VAM_VERSION;
        }

        [[nodiscard]] bool IsCompressed() const
        {
            return (flags & static_cast<uint32_t>(VAMFlags::Compressed)) != 0;
        }

        [[nodiscard]] float GetCompressionRatio() const
        {
            return static_cast<float>(compressionRatio) / 1000.0f;
        }

        void SetCompressionRatio(const float ratio)
        {
            compressionRatio = static_cast<uint32_t>(ratio * 1000.0f);
        }
    };

    // Vertex data - 48 bytes, aligned for SIMD performance
    struct alignas(16) VAMVertex
    {
        Math::Vec3 position; // 12 bytes
        Math::Vec3 normal; // 12 bytes
        Math::Vec2 uv0; // 8 bytes
        Math::Vec4 tangent; // 16 bytes

        VAMVertex() = default;

        VAMVertex(
            const Math::Vec3& position,
            const Math::Vec3& normal,
            const Math::Vec2& uv0,
            const Math::Vec4& tangent)
            : position(position),
              normal(normal),
              uv0(uv0),
              tangent(tangent)
        {
        }
    };

    // SubMesh descriptor - 32 bytes, aligned
    struct alignas(16) VAMSubMeshDescriptor
    {
        uint32_t nameOffset; // Offset in the string table
        uint32_t materialIndex; // Index in the materials array (not handle !)
        uint32_t indexOffset;
        uint32_t indexCount;
        uint32_t vertexOffset;
        uint32_t vertexCount;
        uint64_t reserved; // For future use

        VAMSubMeshDescriptor() = default;

        VAMSubMeshDescriptor(
            const uint32_t nameOff,
            const uint32_t matIdx,
            const uint32_t idxOff,
            const uint32_t idxCnt,
            const uint32_t vtxOff,
            const uint32_t vtxCnt)
            : nameOffset(nameOff),
              materialIndex(matIdx),
              indexOffset(idxOff),
              indexCount(idxCnt),
              vertexOffset(vtxOff),
              vertexCount(vtxCnt),
              reserved{}
        {
        }
    };

    // Resource binding for materials - matches engine ResourceBinding
    struct VAMResourceBinding
    {
        uint32_t type; // ResourceBindingType as uint32_t
        uint32_t binding;
        uint32_t stage; // ShaderStage as uint32_t
        uint32_t reserved; // For future use and alignment
    };

    // Material template - variable size
    struct VAMMAterialTemplate
    {
        uint32_t nameOffset; // Offset in the string table
        uint32_t renderStateClassOffset; // RenderState class name
        Math::Vec4 diffuseColor; // 16 bytes, aligned

        // Texture name offsets in string table
        uint32_t diffuseTextureOffset;
        uint32_t specularTextureOffset;
        uint32_t normalTextureOffset;

        uint32_t bindingCount; // Number of resource bindings

        VAMMAterialTemplate() = default;

        VAMMAterialTemplate(
            const uint32_t nameOff,
            const uint32_t stateOff,
            const Math::Vec4& color,
            const uint32_t diffOff,
            const uint32_t specOff,
            const uint32_t normOff,
            const uint32_t bindingCount)
            : nameOffset(nameOff),
              renderStateClassOffset(stateOff),
              diffuseColor(color),
              diffuseTextureOffset(diffOff),
              specularTextureOffset(specOff),
              normalTextureOffset(normOff),
              bindingCount(bindingCount)
        {
        }
    };

    // String table entry helper
    struct VAMStringEntry
    {
        uint32_t offset;
        std::string data;
    };

    // Memory layout verification
    static_assert(sizeof(VAMHeader) == 96, "VAMHeader must be exactly 96 bytes");
    static_assert(sizeof(VAMVertex) == 48, "VAMVertex must be exactly 48 bytes");
    static_assert(
        sizeof(VAMSubMeshDescriptor) == 32,
        "VAMSubMeshDescriptor must be exactly 32 bytes");
    static_assert(alignof(VAMHeader) == 16, "VAMHeader must be 16-byte aligned");
    static_assert(alignof(VAMVertex) == 16, "VAMVertex must be 16-byte aligned");
    static_assert(
        alignof(VAMSubMeshDescriptor) == 16,
        "VAMSubMeshDescriptor must be 16-byte aligned");
}
