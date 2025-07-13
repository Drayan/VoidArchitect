//
// Created by Michael Desmedt on 15/06/2025.
//
#include "VamCompression.hpp"

#include <VoidArchitect/Engine/Common/Logger.hpp>

#include <lz4.h>

namespace VoidArchitect::Resources::Loaders
{
    bool VAMCompression::s_LZ4Initialized = false;

    bool VAMCompression::IsLZ4Available()
    {
#ifdef LZ4_VERSION_MAJOR
        return true;
#else
        return false;
#endif
    }

    bool VAMCompression::InitializeLZ4()
    {
        if (s_LZ4Initialized)
        {
            return true;
        }

        if (!IsLZ4Available())
        {
            VA_ENGINE_WARN("[VAMCompression] LZ4 is not available, compression is disabled.");
            return false;
        }

        s_LZ4Initialized = true;
        VA_ENGINE_TRACE("[VAMCompression] LZ4 compression initialized.");
        return true;
    }

    void VAMCompression::ShutdownLZ4()
    {
        s_LZ4Initialized = false;
    }

    VAMCompression::CompressionResult VAMCompression::Compress(const uint8_t* data, uint32_t size)
    {
        CompressionResult result;
        result.originalSize = size;

        if (!InitializeLZ4())
        {
            // LZ4 not available - return uncompressed data
            result.compressedData.resize(size);
            memcpy(result.compressedData.data(), data, size);
            result.compressedSize = size;
            result.success = true;

            VA_ENGINE_TRACE(
                "[VAMCompression] LZ4 not available, storing uncompressed ({} bytes).",
                size);
            return result;
        }

#ifdef LZ4_VERSION_MAJOR
        //LZ4 is available - use real compression
        auto maxCompressedSize = LZ4_compressBound(static_cast<int>(size));
        result.compressedData.resize(maxCompressedSize);

        auto compressedSize = LZ4_compress_default(
            reinterpret_cast<const char*>(data),
            reinterpret_cast<char*>(result.compressedData.data()),
            static_cast<int>(size),
            maxCompressedSize);

        if (compressedSize <= 0)
        {
            VA_ENGINE_ERROR("[VAMCompression] LZ4 compression failed.");
            result.success = false;
            return result;
        }

        // Resize to actual compressed size
        result.compressedData.resize(compressedSize);
        result.compressedSize = static_cast<uint32_t>(compressedSize);
        result.success = true;

        auto ratio = GetCompressionRatio(size, result.compressedSize);
        VA_ENGINE_TRACE(
            "[VAMCompression] Compressed {} bytes to {} bytes ({:.1f}% savings).",
            size,
            result.compressedSize,
            ratio * 100.0f);
#else
        // Fallback: no compression
        result.compressedData.resize(size); memcpy(result.compressedData.data(), data, size); result
           .compressedSize = size; result.success = true; VA_ENGINE_TRACE(
            "[VAMCompression] No compression available, storing uncompressed ({} bytes).",
            size);
#endif

        return result;
    }

    VAMCompression::CompressionResult VAMCompression::Compress(const VAArray<uint8_t>& data)
    {
        return Compress(data.data(), static_cast<uint32_t>(data.size()));
    }

    VAArray<uint8_t> VAMCompression::Decompress(
        const uint8_t* compressedData,
        uint32_t compressedSize,
        uint32_t originalSize)
    {
        VAArray<uint8_t> result;

        if (!InitializeLZ4())
        {
            result.resize(compressedSize);
            memcpy(result.data(), compressedData, compressedSize);

            VA_ENGINE_TRACE("[VAMCompression] No decompression needed ({} bytes).", compressedSize);
            return result;
        }

#ifdef LZ4_VERSION_MAJOR
        // LZ4 decompression
        result.resize(originalSize);

        auto decompressedSize = LZ4_decompress_safe(
            reinterpret_cast<const char*>(compressedData),
            reinterpret_cast<char*>(result.data()),
            static_cast<int>(compressedSize),
            static_cast<int>(originalSize));

        if (decompressedSize <= 0 || static_cast<uint32_t>(decompressedSize) != originalSize)
        {
            VA_ENGINE_ERROR(
                "[VAMCompression] LZ4 decompression failed (expected {}, got {}).",
                originalSize,
                decompressedSize);
            result.clear();
            return result;
        }

        VA_ENGINE_TRACE(
            "[VAMCompression] Decompressed {} bytes to {} bytes.",
            compressedSize,
            decompressedSize);
#else
        //Fallback : no decompression
        result.resize(compressedSize); memcpy(result.data(), compressedData, compressedSize);
        VA_ENGINE_TRACE(
            "[VAMCompression] No decompression available, using raw data ({} bytes).",
            compressedSize)
#endif
        return result;
    }

    VAArray<uint8_t> VAMCompression::Decompress(
        const VAArray<uint8_t>& compressedData,
        uint32_t originalSize)
    {
        return Decompress(
            compressedData.data(),
            static_cast<uint32_t>(compressedData.size()),
            originalSize);
    }

    float VAMCompression::GetCompressionRatio(uint32_t originalSize, uint32_t compressedSize)
    {
        if (originalSize == 0) return 0.0f;

        return 1.0f - (static_cast<float>(compressedSize) / static_cast<float>(originalSize));
    }

    uint32_t VAMCompression::GetCompressionSavings(uint32_t originalSize, uint32_t compressedSize)
    {
        if (compressedSize >= originalSize) return 0;

        return originalSize - compressedSize;
    }
}
