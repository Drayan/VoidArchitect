//! Vulkan backend implementation for the renderer.
//!
//! This module provides the `RendererBackendVulkan` struct, which implements the `RendererBackend`
//! trait using the Vulkan API via the `ash` crate. It manages Vulkan instance, device, surface,
//! and related resources, and provides methods for initialization, frame rendering, resizing,
//! and shutdown. Debug utilities are enabled in debug builds.

use framebuffer::VulkanFramebuffer;

mod commands;
mod device;
mod fence;
mod framebuffer;
mod image;
mod renderpass;
mod swapchain;

// Vulkan backend/context split modules
pub mod backend;
pub mod context;
pub use backend::RendererBackendVulkan;
use context::VulkanContext;
