//
// Created by Michael Desmedt on 15/06/2025.
//
#pragma once

namespace VoidArchitect
{
    namespace Resources
    {
        namespace Loaders
        {
            class VAMCompression
            {
            public:
                // Compression result structure
                struct CompressionResult
                {
                    VAArray<uint8_t> compressedData;
                    uint32_t originalSize;
                    uint32_t compressedSize;
                    bool success;

                    CompressionResult()
                        : originalSize(0),
                          compressedSize(0),
                          success(false)
                    {
                    }
                };

                // Compress data using LZ4
                static CompressionResult Compress(const uint8_t* data, uint32_t size);
                static CompressionResult Compress(const VAArray<uint8_t>& data);

                // Decompress data using LZ4
                static VAArray<uint8_t> Decompress(
                    const uint8_t* compressedData,
                    uint32_t compressedSize,
                    uint32_t originalSize);
                static VAArray<uint8_t> Decompress(
                    const VAArray<uint8_t>& compressedData,
                    uint32_t originalSize);

                // Calculate compression savings
                static float GetCompressionRatio(uint32_t originalSize, uint32_t compressedSize);
                static uint32_t GetCompressionSavings(
                    uint32_t originalSize,
                    uint32_t compressedSize);

                // Check if LZ4 is available (compile-time feature check)
                static bool IsLZ4Available();

            private:
                // Internal LZ4 integration - will use external LZ4 library when availabe
                static bool InitializeLZ4();
                static void ShutdownLZ4();

                static bool s_LZ4Initialized;
            };

            struct VAMCompressionSettings
            {
                bool enableCompression = true; // Enable/disable compression
                float minCompressionRatio = 0.1f; // Only compress if we save at least 10%
                uint32_t minSizeThreshold = 1024; // Only compress file larger than 1KiB

                // LZ4 specific settings (for futur expansion)
                int compressionLevel = 1; // LZ4 compression level (0-9)

                // Get default settings
                static VAMCompressionSettings Default()
                {
                    return VAMCompressionSettings{};
                }

                // Get settings optimized for speed
                static VAMCompressionSettings Fast()
                {
                    VAMCompressionSettings settings{};
                    settings.compressionLevel = 1;
                    settings.minCompressionRatio = 0.05f;
                    return settings;
                }

                // Get settings optimized for size
                static VAMCompressionSettings Small()
                {
                    VAMCompressionSettings settings{};
                    settings.compressionLevel = 9;
                    settings.minCompressionRatio = 0.02f;
                    settings.minSizeThreshold = 256;
                    return settings;
                }
            };
        } // Loaders
    } // Resources
} // VoidArchitect
