use crate::{device, fence_mut};

use super::VulkanRendererBackend;

pub(super) struct VulkanFence {
    handle: ash::vk::Fence,
    is_signaled: bool,
}

pub(super) struct VulkanFenceOperations<'backend> {
    backend: &'backend mut VulkanRendererBackend,
}

pub(super) trait VulkanFenceContext {
    fn fence(&mut self) -> VulkanFenceOperations<'_>;
}

impl VulkanFenceContext for VulkanRendererBackend {
    fn fence(&mut self) -> VulkanFenceOperations<'_> {
        VulkanFenceOperations { backend: self }
    }
}

impl VulkanFence {
    pub fn new(device: &ash::Device, is_signaled: bool) -> Result<Self, String> {
        // Create a fence with the specified signaled state

        let fence = Self {
            is_signaled,
            handle: {
                let create_info = ash::vk::FenceCreateInfo {
                    flags: if is_signaled {
                        ash::vk::FenceCreateFlags::SIGNALED
                    } else {
                        ash::vk::FenceCreateFlags::empty()
                    },
                    ..Default::default()
                };

                unsafe {
                    device
                        .create_fence(&create_info, None)
                        .map_err(|err| format!("Failed to create fence: {:?}", err))?
                }
            },
        };

        Ok(fence)
    }

    pub fn destroy(&self, device: &ash::Device) {
        if self.handle == ash::vk::Fence::null() {
            return;
        }

        unsafe {
            device.destroy_fence(self.handle, None);
        }
    }

    pub fn wait(&mut self, device: &ash::Device, timeout: u64) -> Result<bool, String> {
        if self.is_signaled {
            return Ok(true);
        }

        match unsafe { device.wait_for_fences(&[self.handle], true, timeout) } {
            Ok(_) => {
                self.is_signaled = true;
                return Ok(true);
            }
            Err(ash::vk::Result::TIMEOUT) => {
                log::warn!("Fence wait timed out");
                return Ok(false);
            }
            Err(err) => return Err(format!("Failed to wait for fence: {:?}", err)),
        };
    }

    pub fn reset(&mut self, device: &ash::Device) -> Result<(), String> {
        if !self.is_signaled {
            return Ok(());
        }

        let result = unsafe { device.reset_fences(&[self.handle]) };
        match result {
            Ok(_) => {}
            Err(err) => return Err(format!("Failed to reset fence: {:?}", err)),
        }

        self.is_signaled = false;
        Ok(())
    }

    pub fn is_signaled(&self) -> bool {
        self.is_signaled
    }

    pub fn handle(&self) -> ash::vk::Fence {
        self.handle
    }
}

impl<'backend> VulkanFenceOperations<'backend> {
    pub fn wait_for_previous_frame_to_complete(&mut self) -> Result<(), String> {
        if self.backend.images_in_flight[self.backend.image_index].is_some() {
            let fence_index = self.backend.images_in_flight[self.backend.image_index]
                .ok_or("Failed to get fence index")?;
            self.wait_for_image_fence(fence_index)?;
        }

        Ok(())
    }

    pub fn wait_for_image_fence(&mut self, fence_index: usize) -> Result<(), String> {
        let fence = fence_mut!(self.backend, fence_index);
        let device = device!(self.backend);

        fence.wait(device, u64::MAX)?;
        Ok(())
    }

    pub fn update_image_sync(&mut self) -> Result<(), String> {
        let current_fence_index = self.backend.current_frame;
        let image_index = self.backend.image_index;

        self.backend.images_in_flight[image_index] = Some(current_fence_index);

        // Reset the fence for the *current* frame, which will be used next time this frame index comes up.
        let device = device!(self.backend);

        self.backend.in_flight_fences[current_fence_index].reset(device)?;
        Ok(())
    }
}
