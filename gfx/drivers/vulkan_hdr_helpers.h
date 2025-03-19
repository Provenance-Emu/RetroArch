/* Helper functions for HDR support in Vulkan renderer
 * These functions are designed to work with MoltenVK on iOS/tvOS
 */

#ifndef __VULKAN_HDR_HELPERS_H
#define __VULKAN_HDR_HELPERS_H

#include <vulkan/vulkan.h>
#include "../common/vulkan_common.h"

#ifdef VULKAN_HDR_SWAPCHAIN

/* Helper function to update HDR UBO when the buffer cannot be mapped */
bool vulkan_update_hdr_ubo(
      vk_t *vk,
      VkCommandBuffer cmd,
      float max_nits,
      float paper_white_nits,
      float contrast,
      bool expand_gamut,
      float inverse_tonemap,
      float hdr10);

#endif /* VULKAN_HDR_SWAPCHAIN */

#endif /* __VULKAN_HDR_HELPERS_H */
