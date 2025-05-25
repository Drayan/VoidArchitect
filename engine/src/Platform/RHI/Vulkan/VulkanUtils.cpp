//
// Created by Michael Desmedt on 19/05/2025.
//
#include "VulkanUtils.hpp"

namespace VoidArchitect::Platform
{
    // NOTE See : https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#VkResult
    std::string VulkanGetResultString(const VkResult result, const bool extended)
    {
        switch (result)
        {
            default:
            case VK_SUCCESS:
                return extended ? "VK_SUCCESS" : "Command successfully completed.";
            case VK_NOT_READY:
                return extended ? "VK_NOT_READY" : "A fence or query has not yet completed.";
            case VK_TIMEOUT:
                return extended ? "VK_TIMEOUT"
                                : "A wait operation has not completed in the specified time.";
            case VK_EVENT_SET:
                return extended ? "VK_EVENT_SET" : "An event is signaled.";
            case VK_EVENT_RESET:
                return extended ? "VK_EVENT_RESET" : "An event is unsignaled.";
            case VK_INCOMPLETE:
                return extended ? "VK_INCOMPLETE" : "A return array was too small for the result.";
            case VK_ERROR_OUT_OF_HOST_MEMORY:
                return extended ? "VK_ERROR_OUT_OF_HOST_MEMORY"
                                : "A host memory allocation has failed.";
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:
                return extended ? "VK_ERROR_OUT_OF_DEVICE_MEMORY"
                                : "A device memory allocation has failed.";
            case VK_ERROR_INITIALIZATION_FAILED:
                return extended ? "VK_ERROR_INITIALIZATION_FAILED"
                                : "Initialization of an object could not be completed for "
                                  "implementation-specific reasons.";
            case VK_ERROR_DEVICE_LOST:
                return extended ? "VK_ERROR_DEVICE_LOST"
                                : "The logical or physical device has been lost.";
            case VK_ERROR_MEMORY_MAP_FAILED:
                return extended ? "VK_ERROR_MEMORY_MAP_FAILED"
                                : "Mapping of a memory object has failed.";
            case VK_ERROR_LAYER_NOT_PRESENT:
                return extended ? "VK_ERROR_LAYER_NOT_PRESENT"
                                : "A requested layer is not present or could not be loaded.";
            case VK_ERROR_EXTENSION_NOT_PRESENT:
                return extended ? "VK_ERROR_EXTENSION_NOT_PRESENT"
                                : "A requested extension is not supported.";
            case VK_ERROR_FEATURE_NOT_PRESENT:
                return extended ? "VK_ERROR_FEATURE_NOT_PRESENT"
                                : "A requested feature is not supported.";
            case VK_ERROR_INCOMPATIBLE_DRIVER:
                return extended
                           ? "VK_ERROR_INCOMPATIBLE_DRIVER"
                           : "The requested version of Vulkan is not supported by the driver or is "
                             "otherwise incompatible for implementation-specific reasons.";
            case VK_ERROR_TOO_MANY_OBJECTS:
                return extended ? "VK_ERROR_TOO_MANY_OBJECTS"
                                : "Too many objects of the type have already been created.";
            case VK_ERROR_FORMAT_NOT_SUPPORTED:
                return extended ? "VK_ERROR_FORMAT_NOT_SUPPORTED"
                                : "A requested format is not supported on this device.";
            case VK_ERROR_FRAGMENTED_POOL:
                return extended ? "VK_ERROR_FRAGMENTED_POOL"
                                : "A pool allocation has failed due to fragmentation of the pool's "
                                  "memory.";
            case VK_ERROR_UNKNOWN:
                return extended ? "VK_ERROR_UNKNOWN" : "An unknown error has occurred.";
            // Provided by VK_VERSION_1_1
            case VK_ERROR_OUT_OF_POOL_MEMORY:
                return extended ? "VK_ERROR_OUT_OF_POOL_MEMORY"
                                : "A pool memory allocation has failed.";
            // Provided by VK_VERSION_1_1
            case VK_ERROR_INVALID_EXTERNAL_HANDLE:
                return extended ? "VK_ERROR_INVALID_EXTERNAL_HANDLE"
                                : "An external handle is not a valid handle of the specified type.";
            // Provided by VK_VERSION_1_2
            case VK_ERROR_FRAGMENTATION:
                return extended ? "VK_ERROR_FRAGMENTATION"
                                : "A descriptor pool creation has failed due to fragmentation.";
            // Provided by VK_VERSION_1_2
            case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
                return extended ? "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS"
                                : "An opaque capture address is not a valid pointer for the "
                                  "specified device.";
            // Provided by VK_VERSION_1_3
            case VK_PIPELINE_COMPILE_REQUIRED:
                return extended ? "VK_PIPELINE_COMPILE_REQUIRED"
                                : "A pipeline creation or update will fail due to a pipeline "
                                  "compile required for a pipeline config that was not specified "
                                  "at pipeline creation.";
            // Provided by VK_VERSION_1_4
            case VK_ERROR_NOT_PERMITTED:
                return extended ? "VK_ERROR_NOT_PERMITTED"
                                : "The requested operation is not permitted for the current device "
                                  "state.";
            // Provided by VK_KHR_surface
            case VK_ERROR_SURFACE_LOST_KHR:
                return extended ? "VK_ERROR_SURFACE_LOST_KHR" : "A surface is no longer available.";
            // Provided by VK_KHR_surface
            case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
                return extended ? "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR"
                                : "The requested window is already connected to a VkSurfaceKHR, or "
                                  "to some other non-Vulkan API.";
            // Provided by VK_KHR_swapchain
            case VK_SUBOPTIMAL_KHR:
                return extended ? "VK_SUBOPTIMAL_KHR"
                                : "A swapchain no longer matches the surface properties exactly, "
                                  "but can still be used to present to the surface successfully.";
            // Provided by VK_KHR_swapchain
            case VK_ERROR_OUT_OF_DATE_KHR:
                return extended ? "VK_ERROR_OUT_OF_DATE_KHR"
                                : "A surface has changed in such a way that it is no longer "
                                  "compatible with the swapchain, and further presentation "
                                  "requests using the swapchain will fail. Applications must query "
                                  "the new surface properties and recreate their swapchain if they "
                                  "wish to continue presenting to the surface.";
            // Provided by VK_KHR_display_swapchain
            case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
                return extended ? "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR"
                                : "The display used by a swapchain does not use the same "
                                  "presentable image layout, or is incompatible in a way that "
                                  "prevents sharing an image.";
            // Provided by VK_EXT_debug_report
            case VK_ERROR_VALIDATION_FAILED_EXT:
                return extended ? "VK_ERROR_VALIDATION_FAILED_EXT"
                                : "A validation layer found an error.";
            // Provided by VK_NV_glsl_shader
            case VK_ERROR_INVALID_SHADER_NV:
                return extended
                           ? "VK_ERROR_INVALID_SHADER_NV"
                           : "The shader binary was compiled with an unsupported version of GLSL.";
            // Provided by VK_KHR_video_queue
            case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:
                return extended ? "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR"
                                : "The requested image usage is not supported.";
            // Provided by VK_KHR_video_queue
            case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR:
                return extended ? "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR"
                                : "The requested picture layout is not supported.";
            // Provided by VK_KHR_video_queue
            case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR:
                return extended ? "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR"
                                : "The requested profile operation is not supported.";
            // Provided by VK_KHR_video_queue
            case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:
                return extended ? "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR"
                                : "The requested profile format is not supported.";
            // Provided by VK_KHR_video_queue
            case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR:
                return extended ? "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR"
                                : "The requested profile codec is not supported.";
            // Provided by VK_KHR_video_queue
            case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR:
                return extended ? "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR"
                                : "The requested video standard version is not supported.";
            // Provided by VK_EXT_image_drm_format_modifier
            case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
                return extended ? "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT"
                                : "The DRM format modifier plane layout is not supported.";
            // Provided by VK_EXT_full_screen_exclusive
            case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
                return extended ? "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT"
                                : "The display is in full-screen exclusive mode and cannot present "
                                  "to it.";
            // Provided by VK_KHR_deferred_host_operations
            case VK_THREAD_IDLE_KHR:
                return extended ? "VK_THREAD_IDLE_KHR" : "The thread is idle.";
            // Provided by VK_KHR_deferred_host_operations
            case VK_THREAD_DONE_KHR:
                return extended ? "VK_THREAD_DONE_KHR" : "The thread has completed.";
            // Provided by VK_KHR_deferred_host_operations
            case VK_OPERATION_DEFERRED_KHR:
                return extended ? "VK_OPERATION_DEFERRED_KHR" : "The operation is deferred.";
            // Provided by VK_KHR_deferred_host_operations
            case VK_OPERATION_NOT_DEFERRED_KHR:
                return extended ? "VK_OPERATION_NOT_DEFERRED_KHR"
                                : "The operation is not deferred.";
            // Provided by VK_KHR_video_encode_queue
            case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR:
                return extended ? "VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR"
                                : "The requested video standard parameters are not supported.";
            // Provided by VK_EXT_image_compression_control
            case VK_ERROR_COMPRESSION_EXHAUSTED_EXT:
                return extended ? "VK_ERROR_COMPRESSION_EXHAUSTED_EXT"
                                : "The compression operation has exhausted the available "
                                  "compression resources.";
            // Provided by VK_EXT_shader_object
            case VK_INCOMPATIBLE_SHADER_BINARY_EXT:
                return extended ? "VK_INCOMPATIBLE_SHADER_BINARY_EXT"
                                : "The shader binary is incompatible with the implementation.";
            // Provided by VK_KHR_pipeline_binary
            case VK_PIPELINE_BINARY_MISSING_KHR:
                return extended ? "VK_PIPELINE_BINARY_MISSING_KHR"
                                : "The pipeline binary is missing.";
            // Provided by VK_KHR_pipeline_binary
            case VK_ERROR_NOT_ENOUGH_SPACE_KHR:
                return extended ? "VK_ERROR_NOT_ENOUGH_SPACE_KHR"
                                : "The pipeline binary is too large.";
        }
    }

    // NOTE See : https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#VkResult
    bool VulkanCheckResult(const VkResult result)
    {
        switch (result)
        {
            case VK_SUCCESS:
            case VK_NOT_READY:
            case VK_TIMEOUT:
            case VK_EVENT_SET:
            case VK_EVENT_RESET:
            case VK_INCOMPLETE:
            case VK_SUBOPTIMAL_KHR:
            case VK_THREAD_IDLE_KHR:
            case VK_THREAD_DONE_KHR:
            case VK_OPERATION_DEFERRED_KHR:
            case VK_OPERATION_NOT_DEFERRED_KHR:
            case VK_PIPELINE_COMPILE_REQUIRED:
            case VK_PIPELINE_BINARY_MISSING_KHR:
            case VK_INCOMPATIBLE_SHADER_BINARY_EXT:
                return true;
            default:
                return false;
        }
    }
} // namespace VoidArchitect::Platform
