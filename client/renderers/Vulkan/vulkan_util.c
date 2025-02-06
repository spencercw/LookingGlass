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

#include "vulkan_util.h"

#include "common/debug.h"

#include <string.h>

uint32_t vulkan_findMemoryType(
    struct VkPhysicalDeviceMemoryProperties * memoryProperties,
    uint32_t memoryTypeBits, VkMemoryPropertyFlags requiredProperties)
{
  for (uint32_t i = 0; i < memoryProperties->memoryTypeCount; ++i)
  {
    if ((memoryTypeBits & (1 << i)) == 0)
      continue;

    VkMemoryPropertyFlags properties =
        memoryProperties->memoryTypes[i].propertyFlags;
    if ((properties & requiredProperties) == requiredProperties)
      return i;
  }

  return UINT32_MAX;
}

VkDeviceMemory vulkan_allocateMemory(
    struct VkPhysicalDeviceMemoryProperties *memoryProperties,
    VkDevice device, struct VkMemoryRequirements *memoryRequirements,
    VkMemoryPropertyFlags requiredProperties)
{
  uint32_t memoryTypeIndex = vulkan_findMemoryType(memoryProperties,
      memoryRequirements->memoryTypeBits, requiredProperties);
  if (memoryTypeIndex == UINT32_MAX)
  {
    DEBUG_ERROR("Could not find suitable memory type with properties %"PRIu32,
        requiredProperties);
    return NULL;
  }

  struct VkMemoryAllocateInfo allocateInfo =
  {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .pNext = NULL,
    .allocationSize = memoryRequirements->size,
    .memoryTypeIndex = memoryTypeIndex
  };

  VkDeviceMemory memory;
  VkResult result = vkAllocateMemory(device, &allocateInfo, NULL, &memory);
  if (result != VK_SUCCESS)
  {
    DEBUG_ERROR("Failed to allocate memory (VkResult: %d)", result);
    return NULL;
  }

  return memory;
}

VkShaderModule vulkan_loadShader(VkDevice device, const char * spv, size_t len)
{
  if (len % 4 != 0)
  {
    DEBUG_ERROR("SPIR-V length is not a multiple of 4");
    goto err;
  }

  uint32_t *spvAligned = aligned_alloc(4, len);
  if (!spvAligned)
  {
    DEBUG_ERROR("out of memory");
    goto err;
  }
  memcpy(spvAligned, spv, len);

  struct VkShaderModuleCreateInfo createInfo =
  {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .codeSize = len,
    .pCode = spvAligned,
  };

  VkShaderModule shader;
  VkResult result = vkCreateShaderModule(device, &createInfo, NULL, &shader);
  if (result != VK_SUCCESS)
  {
    DEBUG_ERROR("Failed to create shader module (VkResult: %d)", result);
    goto err_spv;
  }

  free(spvAligned);
  return shader;

err_spv:
  free(spvAligned);

err:
  return NULL;
}

VkPipeline vulkan_createGraphicsPipeline(VkDevice device,
    VkShaderModule vertexShader, VkShaderModule fragmentShader,
    struct VkSpecializationInfo * fragmentSpecializationInfo,
    VkPipelineLayout pipelineLayout, VkRenderPass renderPass)
{
  struct VkPipelineShaderStageCreateInfo stages[] =
  {
    {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vertexShader,
      .pName = "main",
      .pSpecializationInfo = NULL
    },
    {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = fragmentShader,
      .pName = "main",
      .pSpecializationInfo = fragmentSpecializationInfo
    }
  };

  struct VkPipelineVertexInputStateCreateInfo vertexInputState =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .vertexBindingDescriptionCount = 0,
    .pVertexBindingDescriptions = NULL,
    .vertexAttributeDescriptionCount = 0,
    .pVertexAttributeDescriptions = NULL
  };

  struct VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    .primitiveRestartEnable = VK_FALSE
  };

  struct VkPipelineViewportStateCreateInfo viewportState =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .viewportCount = 1,
    .pViewports = NULL,
    .scissorCount = 1,
    .pScissors = NULL
  };

  struct VkPipelineRasterizationStateCreateInfo rasterizationState =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_CLOCKWISE,
    .depthBiasEnable = VK_FALSE,
    .depthBiasConstantFactor = 0.0f,
    .depthBiasClamp = 0.0f,
    .depthBiasSlopeFactor = 0.0f,
    .lineWidth = 1.0f
  };

  struct VkPipelineMultisampleStateCreateInfo multisampleState =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    .sampleShadingEnable = VK_FALSE,
    .minSampleShading = 0.0f,
    .pSampleMask = NULL,
    .alphaToCoverageEnable = VK_FALSE,
    .alphaToOneEnable = VK_FALSE
  };

  struct VkPipelineColorBlendAttachmentState colorBlendAttachment =
  {
    .blendEnable = VK_FALSE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
  };

  struct VkPipelineColorBlendStateCreateInfo colorBlendState =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .logicOpEnable = VK_FALSE,
    .logicOp = VK_LOGIC_OP_CLEAR,
    .attachmentCount = 1,
    .pAttachments = &colorBlendAttachment,
    .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
  };

  VkDynamicState dynamicStates[] =
  {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
  };

  struct VkPipelineDynamicStateCreateInfo dynamicState =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .dynamicStateCount = 2,
    .pDynamicStates = dynamicStates
  };

  struct VkGraphicsPipelineCreateInfo createInfo =
  {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .stageCount = 2,
    .pStages = stages,
    .pVertexInputState = &vertexInputState,
    .pInputAssemblyState = &inputAssemblyState,
    .pTessellationState = NULL,
    .pViewportState = &viewportState,
    .pRasterizationState = &rasterizationState,
    .pMultisampleState = &multisampleState,
    .pDepthStencilState = NULL,
    .pColorBlendState = &colorBlendState,
    .pDynamicState = &dynamicState,
    .layout = pipelineLayout,
    .renderPass = renderPass,
    .subpass = 0,
    .basePipelineHandle = NULL,
    .basePipelineIndex = 0
  };

  VkPipeline pipeline;
  VkResult result = vkCreateGraphicsPipelines(device, NULL, 1,
      &createInfo, NULL, &pipeline);
  if (result != VK_SUCCESS)
  {
    DEBUG_ERROR("Failed to create graphics pipeline (VkResult: %d)", result);
    return NULL;
  }

  return pipeline;
}

VkDescriptorSet vulkan_allocateDescriptorSet(VkDevice device,
    VkDescriptorSetLayout layout, VkDescriptorPool descriptorPool)
{
  struct VkDescriptorSetAllocateInfo allocateInfo =
  {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .pNext = NULL,
    .descriptorPool = descriptorPool,
    .descriptorSetCount = 1,
    .pSetLayouts = &layout
  };

  VkDescriptorSet descriptorSet;
  VkResult result = vkAllocateDescriptorSets(device, &allocateInfo,
      &descriptorSet);
  if (result != VK_SUCCESS)
  {
    DEBUG_ERROR("Failed to allocate descriptor set (VkResult: %d)", result);
    return NULL;
  }

  return descriptorSet;
}

VkBuffer vulkan_createBuffer(
    struct VkPhysicalDeviceMemoryProperties * memoryProperties, VkDevice device,
    VkDeviceSize size, VkBufferUsageFlags usage, VkDeviceMemory * memory,
    void * map)
{
  struct VkBufferCreateInfo createInfo =
  {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .size = size,
    .usage = usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = NULL
  };

  VkBuffer buffer;
  VkResult result = vkCreateBuffer(device, &createInfo, NULL, &buffer);
  if (result != VK_SUCCESS)
  {
    DEBUG_ERROR("Failed to create buffer (VkResult: %d)", result);
    goto err;
  }

  struct VkMemoryRequirements memoryRequirements;
  vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);
  uint32_t memoryTypeIndex = vulkan_findMemoryType(memoryProperties,
      memoryRequirements.memoryTypeBits,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if (memoryTypeIndex == UINT32_MAX)
  {
    DEBUG_ERROR("Could not find suitable memory type for buffer");
    goto err_buffer;
  }

  struct VkMemoryAllocateInfo allocateInfo =
  {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .pNext = NULL,
    .allocationSize = memoryRequirements.size,
    .memoryTypeIndex = memoryTypeIndex
  };

  result = vkAllocateMemory(device, &allocateInfo, NULL, memory);
  if (result != VK_SUCCESS)
  {
    DEBUG_ERROR("Failed to allocate buffer memory (VkResult: %d)", result);
    goto err_buffer;
  }

  result = vkBindBufferMemory(device, buffer, *memory, 0);
  if (result != VK_SUCCESS)
  {
    DEBUG_ERROR("Failed to bind buffer memory (VkResult: %d)",
        result);
    goto err_memory;
  }

  result = vkMapMemory(device, *memory, 0, VK_WHOLE_SIZE, 0, map);
  if (result != VK_SUCCESS)
  {
    DEBUG_ERROR("Failed to map buffer memory (VkResult: %d)", result);
    goto err_memory;
  }

  return buffer;

err_memory:
  vkFreeMemory(device, *memory, NULL);
  *memory = NULL;

err_buffer:
  vkDestroyBuffer(device, buffer, NULL);

err:
  return false;
}

VkImageView vulkan_createImageView(VkDevice device, VkImage image,
    VkFormat format)
{
  struct VkImageViewCreateInfo createInfo =
  {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .pNext = NULL,
    .image = image,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = format,
    .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
    .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
    .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
    .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
    .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .subresourceRange.baseMipLevel = 0,
    .subresourceRange.levelCount = 1,
    .subresourceRange.baseArrayLayer = 0,
    .subresourceRange.layerCount = 1
  };

  VkImageView imageView;
  VkResult result = vkCreateImageView(device, &createInfo, NULL, &imageView);
  if (result != VK_SUCCESS)
  {
    DEBUG_ERROR("Failed to create image view (VkResult: %d)", result);
    return NULL;
  }

  return imageView;
}

bool vulkan_waitFence(VkDevice device, VkFence fence)
{
  VkResult result = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
  if (result != VK_SUCCESS)
  {
    DEBUG_ERROR("Failed to wait for fence (VkResult: %d)", result);
    return false;
  }

  result = vkResetFences(device, 1, &fence);
  if (result != VK_SUCCESS)
  {
    DEBUG_ERROR("Failed to reset fence (VkResult: %d)", result);
    return false;
  }

  return true;
}

void vulkan_updateDescriptorSet0(VkDevice device, VkDescriptorSet descriptorSet,
    VkImageView imageView)
{
  struct VkDescriptorImageInfo imageInfo =
  {
    .sampler = NULL,
    .imageView = imageView,
    .imageLayout = VK_IMAGE_LAYOUT_GENERAL
  };

  struct VkWriteDescriptorSet descriptorWrites[] =
  {
    {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .pNext = NULL,
      .dstSet = descriptorSet,
      .dstBinding = 0,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
      .pImageInfo = &imageInfo,
      .pBufferInfo = NULL,
      .pTexelBufferView = NULL
    }
  };

  vkUpdateDescriptorSets(device, 1, descriptorWrites, 0, NULL);
}

void vulkan_updateDescriptorSet1(VkDevice device, VkDescriptorSet descriptorSet,
    VkBuffer uniformBuffer, VkImageView imageView, VkImageLayout imageLayout)
{
  struct VkDescriptorBufferInfo bufferInfo =
  {
    .buffer = uniformBuffer,
    .offset = 0,
    .range = VK_WHOLE_SIZE
  };

  struct VkDescriptorImageInfo imageInfo =
  {
    .sampler = NULL,
    .imageView = imageView,
    .imageLayout = imageLayout
  };

  struct VkWriteDescriptorSet descriptorWrites[] =
  {
    {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .pNext = NULL,
      .dstSet = descriptorSet,
      .dstBinding = 0,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .pImageInfo = NULL,
      .pBufferInfo = &bufferInfo,
      .pTexelBufferView = NULL
    },
    {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .pNext = NULL,
      .dstSet = descriptorSet,
      .dstBinding = 1,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImageInfo = &imageInfo,
      .pBufferInfo = NULL,
      .pTexelBufferView = NULL
    }
  };

  vkUpdateDescriptorSets(device, 2, descriptorWrites, 0, NULL);
}

void vulkan_updateUniformBuffer(void * bufferMap, float translateX,
    float translateY, float scaleX, float scaleY, LG_RendererRotate rotate)
{
  struct VulkanUniformBuffer uniformBuffer = {};

  switch (rotate)
  {
    case LG_ROTATE_0:
      uniformBuffer.transform[0 * 4 + 0] = scaleX;
      uniformBuffer.transform[0 * 4 + 1] = 0.0f;
      uniformBuffer.transform[0 * 4 + 2] = 0.0f;
      uniformBuffer.transform[0 * 4 + 3] = 0.0f;

      uniformBuffer.transform[1 * 4 + 0] = 0.0f;
      uniformBuffer.transform[1 * 4 + 1] = scaleY;
      uniformBuffer.transform[1 * 4 + 2] = 0.0f;
      uniformBuffer.transform[1 * 4 + 3] = 0.0f;

      uniformBuffer.transform[2 * 4 + 0] = 0.0f;
      uniformBuffer.transform[2 * 4 + 1] = 0.0f;
      uniformBuffer.transform[2 * 4 + 2] = 1.0f;
      uniformBuffer.transform[2 * 4 + 3] = 0.0f;

      uniformBuffer.transform[3 * 4 + 0] = translateX;
      uniformBuffer.transform[3 * 4 + 1] = translateY;
      uniformBuffer.transform[3 * 4 + 2] = 0.0f;
      uniformBuffer.transform[3 * 4 + 3] = 1.0f;
      break;

    case LG_ROTATE_90:
      uniformBuffer.transform[0 * 4 + 0] = 0.0f;
      uniformBuffer.transform[0 * 4 + 1] = scaleY;
      uniformBuffer.transform[0 * 4 + 2] = 0.0f;
      uniformBuffer.transform[0 * 4 + 3] = 0.0f;

      uniformBuffer.transform[1 * 4 + 0] = -scaleX;
      uniformBuffer.transform[1 * 4 + 1] = 0.0f;
      uniformBuffer.transform[1 * 4 + 2] = 0.0f;
      uniformBuffer.transform[1 * 4 + 3] = 0.0f;

      uniformBuffer.transform[2 * 4 + 0] = 0.0f;
      uniformBuffer.transform[2 * 4 + 1] = 0.0f;
      uniformBuffer.transform[2 * 4 + 2] = 1.0f;
      uniformBuffer.transform[2 * 4 + 3] = 0.0f;

      uniformBuffer.transform[3 * 4 + 0] = translateX;
      uniformBuffer.transform[3 * 4 + 1] = translateY;
      uniformBuffer.transform[3 * 4 + 2] = 0.0f;
      uniformBuffer.transform[3 * 4 + 3] = 1.0f;
      break;

    case LG_ROTATE_180:
      uniformBuffer.transform[0 * 4 + 0] = -scaleX;
      uniformBuffer.transform[0 * 4 + 1] = 0.0f;
      uniformBuffer.transform[0 * 4 + 2] = 0.0f;
      uniformBuffer.transform[0 * 4 + 3] = 0.0f;

      uniformBuffer.transform[1 * 4 + 0] = 0.0f;
      uniformBuffer.transform[1 * 4 + 1] = -scaleY;
      uniformBuffer.transform[1 * 4 + 2] = 0.0f;
      uniformBuffer.transform[1 * 4 + 3] = 0.0f;

      uniformBuffer.transform[2 * 4 + 0] = 0.0f;
      uniformBuffer.transform[2 * 4 + 1] = 0.0f;
      uniformBuffer.transform[2 * 4 + 2] = 1.0f;
      uniformBuffer.transform[2 * 4 + 3] = 0.0f;

      uniformBuffer.transform[3 * 4 + 0] = translateX;
      uniformBuffer.transform[3 * 4 + 1] = translateY;
      uniformBuffer.transform[3 * 4 + 2] = 0.0f;
      uniformBuffer.transform[3 * 4 + 3] = 1.0f;
      break;

    case LG_ROTATE_270:
      uniformBuffer.transform[0 * 4 + 0] = 0.0f;
      uniformBuffer.transform[0 * 4 + 1] = -scaleY;
      uniformBuffer.transform[0 * 4 + 2] = 0.0f;
      uniformBuffer.transform[0 * 4 + 3] = 0.0f;

      uniformBuffer.transform[1 * 4 + 0] = scaleX;
      uniformBuffer.transform[1 * 4 + 1] = 0.0f;
      uniformBuffer.transform[1 * 4 + 2] = 0.0f;
      uniformBuffer.transform[1 * 4 + 3] = 0.0f;

      uniformBuffer.transform[2 * 4 + 0] = 0.0f;
      uniformBuffer.transform[2 * 4 + 1] = 0.0f;
      uniformBuffer.transform[2 * 4 + 2] = 1.0f;
      uniformBuffer.transform[2 * 4 + 3] = 0.0f;

      uniformBuffer.transform[3 * 4 + 0] = translateX;
      uniformBuffer.transform[3 * 4 + 1] = translateY;
      uniformBuffer.transform[3 * 4 + 2] = 0.0f;
      uniformBuffer.transform[3 * 4 + 3] = 1.0f;
      break;
  }

  memcpy(bufferMap, &uniformBuffer, sizeof(uniformBuffer));
}
