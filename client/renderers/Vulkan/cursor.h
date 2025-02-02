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

#pragma once

#include <stdbool.h>

#include "interface/renderer.h"

typedef struct Vulkan_Cursor Vulkan_Cursor;

bool vulkan_cursorInit(Vulkan_Cursor ** this,
    struct VkPhysicalDeviceMemoryProperties * memoryProperties, VkDevice device,
    VkCommandBuffer commandBuffer);
void vulkan_cursorFree(Vulkan_Cursor ** cursor);

bool vulkan_cursorSetShape(
    Vulkan_Cursor * this,
    const LG_RendererCursor type,
    const int width,
    const int height,
    const int stride,
    const uint8_t * data);

void vulkan_cursorSetSize(Vulkan_Cursor * cursor, const float x, const float y);

void vulkan_cursorSetState(Vulkan_Cursor * cursor, const bool visible,
    const float x, const float y, const float hx, const float hy);

bool vulkan_cursorPreRender(Vulkan_Cursor * this);
