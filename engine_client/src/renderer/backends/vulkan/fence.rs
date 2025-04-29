pub(super) struct VulkanFence {
    handle: ash::vk::Fence,
    is_signaled: bool,
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
