/**
 * Looking Glass
 * Copyright © 2017-2024 The Looking Glass Authors
 * https://looking-glass.io
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "cursor.h"
#include "common/locking.h"
#include "common/util.h"

#include "vulkan_util.h"

#include <string.h>

struct CursorPos
{
  float x, y;
};

struct CursorSize
{
  float w, h;
};

struct Vulkan_Cursor
{
  LG_Lock           lock;
  LG_RendererCursor type;
  int               width;
  int               height;
  int               stride;
  uint8_t         * data;
  size_t            dataSize;
  bool              update;

  // cursor state
  bool              visible;
  LG_RendererRotate rotate;

  _Atomic(struct CursorPos)  pos;
  _Atomic(struct CursorPos)  hs;
  _Atomic(struct CursorSize) size;

  struct VkPhysicalDeviceMemoryProperties * memoryProperties;
  VkDevice         device;
  VkCommandBuffer  commandBuffer;
  VkDescriptorPool descriptorPool;
  VkDescriptorSet  descriptorSet;
  VkPipelineLayout pipelineLayout;

  VkBuffer         uniformBuffer;
  VkDeviceMemory   uniformBufferMemory;
  void           * uniformBufferMap;

  VkImage          image;
  VkImageView      imageView;
  VkDeviceMemory   imageMemory;
  uint32_t         imageSize;
  VkBuffer         stagingBuffer;
  VkDeviceMemory   stagingMemory;
  void           * stagingMap;
};

static void freeUniformBuffer(Vulkan_Cursor * this)
{
  if (this->uniformBuffer)
  {
    vkDestroyBuffer(this->device, this->uniformBuffer, NULL);
    this->uniformBuffer = NULL;
  }

  if (this->uniformBufferMap)
  {
    vkUnmapMemory(this->device, this->uniformBufferMemory);
    this->uniformBufferMap = NULL;
  }

  if (this->uniformBufferMemory)
  {
    vkFreeMemory(this->device, this->uniformBufferMemory, NULL);
    this->uniformBufferMemory = NULL;
  }
}

static void freeImage(Vulkan_Cursor * this)
{
  if (this->imageView)
  {
    vkDestroyImageView(this->device, this->imageView, NULL);
    this->imageView = NULL;
  }

  if (this->image)
  {
    vkDestroyImage(this->device, this->image, NULL);
    this->image = NULL;
    this->imageSize = 0;
  }

  if (this->imageMemory)
  {
    vkFreeMemory(this->device, this->imageMemory, NULL);
    this->imageMemory = NULL;
  }

  if (this->stagingMap)
  {
    vkUnmapMemory(this->device, this->stagingMemory);
    this->stagingMap = NULL;
  }

  if (this->stagingBuffer)
  {
    vkDestroyBuffer(this->device, this->stagingBuffer, NULL);
    this->stagingBuffer = NULL;
  }

  if (this->stagingMemory)
  {
    vkFreeMemory(this->device, this->stagingMemory, NULL);
    this->stagingMemory = NULL;
  }
}

static bool createImage(Vulkan_Cursor * this)
{
  // Over-size the image to avoid reallocations when the shape changes
  uint32_t desiredSize = max(this->width, this->height);
  uint32_t actualSize = 64;
  while (actualSize < desiredSize)
    actualSize *= 2;
  if (actualSize <= this->imageSize)
    return true;

  freeImage(this);

  struct VkImageCreateInfo createInfo =
  {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = VK_FORMAT_B8G8R8A8_SRGB,
    .extent.width = actualSize,
    .extent.height = actualSize,
    .extent.depth = 1,
    .mipLevels = 1,
    .arrayLayers = 1,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = NULL,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
  };

  VkResult result = vkCreateImage(this->device, &createInfo, NULL,
      &this->image);
  if (result != VK_SUCCESS)
  {
    DEBUG_ERROR("Failed to create cursor image (VkResult: %d)", result);
    goto err;
  }

  struct VkMemoryRequirements memoryRequirements;
  vkGetImageMemoryRequirements(this->device, this->image,
      &memoryRequirements);

  this->imageMemory = vulkan_allocateMemory(this->memoryProperties,
      this->device, &memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (!this->imageMemory)
    goto err_image;

  result = vkBindImageMemory(this->device, this->image, this->imageMemory,
      0);
  if (result != VK_SUCCESS)
  {
    DEBUG_ERROR("Failed to bind cursor image memory (VkResult: %d)", result);
    goto err_memory;
  }

  this->imageView = vulkan_createImageView(this->device, this->image,
      createInfo.format);
  if (!this->imageView)
    goto err_memory;

  this->stagingBuffer = vulkan_createBuffer(this->memoryProperties,
      this->device, actualSize * actualSize * 4,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &this->stagingMemory,
      &this->stagingMap);
  if (!this->stagingBuffer)
    goto err_image_view;

  vulkan_updateDescriptorSet(this->device, this->descriptorSet,
      this->uniformBuffer, this->imageView,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  this->imageSize = actualSize;
  return true;

err_image_view:
  vkDestroyImageView(this->device, this->imageView, NULL);
  this->imageView = NULL;

err_memory:
  vkFreeMemory(this->device, this->imageMemory, NULL);
  this->imageMemory = NULL;

err_image:
  vkDestroyImage(this->device, this->image, NULL);
  this->image = NULL;

err:
  return false;
}

bool vulkan_cursorInit(Vulkan_Cursor ** cursor,
    struct VkPhysicalDeviceMemoryProperties * memoryProperties, VkDevice device,
    VkCommandBuffer commandBuffer, VkDescriptorSetLayout descriptorSetLayout,
    VkDescriptorPool descriptorPool, VkPipelineLayout pipelineLayout)
{
  *cursor = calloc(1, sizeof(**cursor));
  if (!*cursor)
  {
    DEBUG_ERROR("Failed to malloc Vulkan_Cursor");
    goto err;
  }

  LG_LOCK_INIT((*cursor)->lock);
  (*cursor)->memoryProperties = memoryProperties;
  (*cursor)->device = device;
  (*cursor)->commandBuffer = commandBuffer;
  (*cursor)->descriptorPool = descriptorPool;
  (*cursor)->pipelineLayout = pipelineLayout;

  (*cursor)->descriptorSet = vulkan_allocateDescriptorSet(device,
      descriptorSetLayout, descriptorPool);
  if (!(*cursor)->descriptorSet)
    goto err_cursor;

  (*cursor)->uniformBuffer = vulkan_createBuffer(memoryProperties, device,
      sizeof(struct VulkanUniformBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      &(*cursor)->uniformBufferMemory, &(*cursor)->uniformBufferMap);
  if (!(*cursor)->uniformBuffer)
    goto err_descriptor_set;

  if (!createImage(*cursor))
    goto err_uniform;

  struct CursorPos  pos  = { .x = 0, .y = 0 };
  struct CursorPos  hs   = { .x = 0, .y = 0 };
  struct CursorSize size = { .w = 0, .h = 0 };
  atomic_init(&(*cursor)->pos , pos );
  atomic_init(&(*cursor)->hs  , hs  );
  atomic_init(&(*cursor)->size, size);

  return true;

err_uniform:
  freeUniformBuffer(*cursor);

err_descriptor_set:
  vkFreeDescriptorSets(device, descriptorPool, 1, &(*cursor)->descriptorSet);

err_cursor:
  LG_LOCK_FREE((*cursor)->lock);
  free(cursor);

err:
  return false;
}

void vulkan_cursorFree(Vulkan_Cursor ** cursor)
{
  Vulkan_Cursor * this = *cursor;
  if (!this)
    return;

  LG_LOCK_FREE(this->lock);
  if (this->data)
    free(this->data);

  freeImage(this);

  freeUniformBuffer(this);

  vkFreeDescriptorSets(this->device, this->descriptorPool, 1,
      &this->descriptorSet);

  free(this);
  *cursor = NULL;
}

bool vulkan_cursorSetShape(Vulkan_Cursor * this, const LG_RendererCursor type,
    const int width, const int height, const int stride, const uint8_t * data)
{
  LG_LOCK(this->lock);

  this->type   = type;
  this->width  = width;
  this->height = (type == LG_CURSOR_MONOCHROME ? height / 2 : height);
  this->stride = stride;

  const size_t size = height * stride;
  if (size > this->dataSize)
  {
    if (this->data)
      free(this->data);

    this->data = malloc(size);
    if (!this->data)
    {
      DEBUG_ERROR("Failed to malloc buffer for cursor shape");
      return false;
    }

    this->dataSize = size;
  }

  memcpy(this->data, data, size);
  this->update = true;

  LG_UNLOCK(this->lock);
  return true;
}

void vulkan_cursorSetSize(Vulkan_Cursor * cursor, const float w, const float h)
{
  struct CursorSize size = { .w = w, .h = h };
  atomic_store(&cursor->size, size);
}

void vulkan_cursorSetState(Vulkan_Cursor * cursor, const bool visible,
    const float x, const float y, const float hx, const float hy)
{
  cursor->visible = visible;
  struct CursorPos pos = { .x = x , .y = y  };
  struct CursorPos hs  = { .x = hx, .y = hy };
  atomic_store(&cursor->pos, pos);
  atomic_store(&cursor->hs , hs);
}

static void updateImage(Vulkan_Cursor * this)
{
  if (this->type != LG_CURSOR_COLOR)
  {
    DEBUG_ERROR("Cursor type %d not implemented", this->type);
    return;
  }

  memcpy(this->stagingMap, this->data, this->width * this->height * 4);

  struct VkImageMemoryBarrier copyImageBarrier =
  {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .pNext = NULL,
    .srcAccessMask = VK_ACCESS_NONE,
    .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = this->image,
    .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .subresourceRange.baseMipLevel = 0,
    .subresourceRange.levelCount = 1,
    .subresourceRange.baseArrayLayer = 0,
    .subresourceRange.layerCount = 1
  };

  vkCmdPipelineBarrier(this->commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
      &copyImageBarrier);

  if (this->width != this->imageSize || this->height != this->imageSize)
  {
    union VkClearColorValue clearValue =
    {
      .float32 = {0.0f, 0.0f, 0.0f, 0.0f}
    };

    struct VkImageSubresourceRange range =
    {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1
    };

    vkCmdClearColorImage(this->commandBuffer, this->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &range);
  }

  struct VkBufferImageCopy region =
  {
    .bufferOffset = 0,
    .bufferRowLength = 0,
    .bufferImageHeight = 0,
    .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .imageSubresource.mipLevel = 0,
    .imageSubresource.baseArrayLayer = 0,
    .imageSubresource.layerCount = 1,
    .imageOffset.x = 0,
    .imageOffset.y = 0,
    .imageOffset.z = 0,
    .imageExtent.width = this->width,
    .imageExtent.height = this->height,
    .imageExtent.depth = 1
  };

  vkCmdCopyBufferToImage(this->commandBuffer, this->stagingBuffer, this->image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  struct VkImageMemoryBarrier renderImageBarrier =
  {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .pNext = NULL,
    .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = this->image,
    .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .subresourceRange.baseMipLevel = 0,
    .subresourceRange.levelCount = 1,
    .subresourceRange.baseArrayLayer = 0,
    .subresourceRange.layerCount = 1
  };

  vkCmdPipelineBarrier(this->commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1,
      &renderImageBarrier);
}

bool vulkan_cursorPreRender(Vulkan_Cursor * this)
{
  if (!this->visible || !this->update)
    return true;

  LG_LOCK(this->lock);
  this->update = false;

  if (!createImage(this))
  {
    LG_UNLOCK(this->lock);
    return false;
  }

  updateImage(this);

  LG_UNLOCK(this->lock);
  return true;
}

void vulkan_cursorRender(Vulkan_Cursor * this, LG_RendererRotate rotate,
    int width, int height)
{
  if (!this->visible)
    return;

  if (rotate != LG_ROTATE_0)
    DEBUG_ERROR("Cursor rotation %d not implemented", rotate);

  struct CursorPos  pos  = atomic_load(&this->pos );
  struct CursorPos  hs   = atomic_load(&this->hs  );
  struct CursorSize size = atomic_load(&this->size);

  pos.x -= hs.x;
  pos.y -= hs.y;

  float translateX = pos.x + (float) this->imageSize / (float) width;
  float translateY = pos.y + (float) this->imageSize / (float) height;
  float scaleX = (float) this->imageSize / (float) width;
  float scaleY = (float) this->imageSize / (float) height;

  vulkan_updateUniformBuffer(this->uniformBufferMap, translateX, translateY,
      scaleX, scaleY, LG_ROTATE_0);

  VkRect2D scissor =
  {
    .offset.x = max((pos.x * width + width) / 2, 0),
    .offset.y = max((pos.y * height + height) / 2, 0),
    .extent.width = size.w * width,
    .extent.height = size.h * height
  };

  vkCmdSetScissor(this->commandBuffer, 0, 1, &scissor);

  vkCmdBindDescriptorSets(this->commandBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, this->pipelineLayout, 0, 1,
      &this->descriptorSet, 0, NULL);

  vkCmdDraw(this->commandBuffer, 4, 1, 0, 0);
}
