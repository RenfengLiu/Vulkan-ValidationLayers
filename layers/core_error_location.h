/* Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Valve Corporation
 * Copyright (c) 2021 LunarG, Inc.
 * Copyright (C) 2021 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Jeremy Gebben <jeremyg@lunarg.com>
 */
#pragma once

#include <cstdint>
#include <string>
#include <sstream>
#include <limits>

#include "vk_layer_data.h"

// structure to track where a validation error occurs, and capture enough information
// to generate the start of a log message and find the correct VUID for many commonvalidity errors.
//
// usage example:
// CoreErrorLocation outer(ErrFunc::vkCmdPipelineBarrier", RefPage::VkImageMemoryBarrier);
//     auto struct_level = outer.dot(Field::pImageMemoryBarriers, i);
//        auto field_level = struct_level.dot(Field::srcAccessMask);
//        std::cout << field_level.Message() << std::endl;
// will print:
//        vkCmdPipelineBarrier(): pImageMemoryBarriers[42].srcAccessMask
// VUIDs can be found for an error in generic code using a combination of the
// func_name, refpage, and field_name members.

/// TODO: these enums can eventually be autogenerated from vk.xml
enum class ErrFunc {
    Empty = 0,
    vkQueueSubmit,
    vkQueueSubmit2KHR,
    vkCmdSetEvent,
    vkCmdSetEvent2KHR,
    vkCmdResetEvent,
    vkCmdResetEvent2KHR,
    vkCmdPipelineBarrier,
    vkCmdPipelineBarrier2KHR,
    vkCmdWaitEvents,
    vkCmdWaitEvents2KHR,
    vkCmdWriteTimestamp2,
    vkCmdWriteTimestamp2KHR,
    vkCreateRenderPass,
    vkCreateRenderPass2,
    vkQueueBindSparse,
    vkSignalSemaphore,
};

enum class RefPage {
    Empty = 0,
    VkMemoryBarrier,
    VkMemoryBarrier2KHR,
    VkBufferMemoryBarrier,
    VkImageMemoryBarrier,
    VkBufferMemoryBarrier2KHR,
    VkImageMemoryBarrier2KHR,
    VkSubmitInfo,
    VkSubmitInfo2KHR,
    VkCommandBufferSubmitInfoKHR,
    vkCmdSetEvent,
    vkCmdSetEvent2KHR,
    vkCmdResetEvent,
    vkCmdResetEvent2KHR,
    vkCmdPipelineBarrier,
    vkCmdPipelineBarrier2KHR,
    vkCmdWaitEvents,
    vkCmdWaitEvents2KHR,
    vkCmdWriteTimestamp2,
    vkCmdWriteTimestamp2KHR,
    VkSubpassDependency,
    VkSubpassDependency2,
    VkBindSparseInfo,
    VkSemaphoreSignalInfo,
};

enum class Field {
    Empty = 0,
    oldLayout,
    newLayout,
    image,
    buffer,
    pMemoryBarriers,
    pBufferMemoryBarriers,
    pImageMemoryBarriers,
    offset,
    size,
    subresourceRange,
    srcAccessMask,
    dstAccessMask,
    srcStageMask,
    dstStageMask,
    pNext,
    pWaitDstStageMask,
    pWaitSemaphores,
    pSignalSemaphores,
    pWaitSemaphoreInfos,
    pWaitSemaphoreValues,
    pSignalSemaphoreInfos,
    pSignalSemaphoreValues,
    stage,
    stageMask,
    value,
    pCommandBuffers,
    pSubmits,
    pCommandBufferInfos,
    semaphore,
    commandBuffer,
    dependencyFlags,
    pDependencyInfo,
    pDependencyInfos,
    srcQueueFamilyIndex,
    dstQueueFamilyIndex,
    queryPool,
    pDependencies,
};

struct CoreErrorLocation {
    static const uint32_t kNoIndex = std::numeric_limits<uint32_t>::max();

    // name of the vulkan function we're checking
    ErrFunc func_name;

    // VUID-{refpage}-{field_name}-#####
    RefPage refpage;
    Field field_name;
    // optional index if checking an array.
    uint32_t index;

    // tracking for how we walk down into arrays of structures
    struct Path {
        Path() : field(Field::Empty), index(kNoIndex) {}
        Path(Field f, uint32_t i) : field(f), index(i) {}
        Field field;
        uint32_t index;
    };
    // should be sized to cover the common struct nesting depths without allocating.
    small_vector<Path, 3> field_path;

    CoreErrorLocation(ErrFunc func, RefPage ref, Field f = Field::Empty, uint32_t i = kNoIndex)
        : func_name(func), refpage(ref), field_name(f), index(i) {}

    std::string Message() const {
        std::stringstream out;
        out << StringFuncName() << "(): ";
        for (const auto& f : field_path) {
            out << String(f.field);
            if (f.index != kNoIndex) {
                out << "[" << f.index << "]";
            }
            out << ".";
        }
        out << String(field_name);
        if (index != kNoIndex) {
            out << "[" << index << "]";
        }
        return out.str();
    }

    // the dot() method is for walking down into a structure that is being validated
    // eg:  loc.dot(Field::pMemoryBarriers, 5).dot(Field::srcStagemask)
    CoreErrorLocation dot(Field sub_field, uint32_t sub_index = kNoIndex) const {
        CoreErrorLocation result(this->func_name, this->refpage);
        result.field_path.reserve(this->field_path.size() + 1);
        result.field_path = this->field_path;
        if (this->field_name != Field::Empty) {
            result.field_path.emplace_back(Path{this->field_name, this->index});
        }
        result.field_name = sub_field;
        result.index = sub_index;
        return result;
    }

    static const std::string& String(ErrFunc func);
    static const std::string& String(RefPage refpage);
    static const std::string& String(Field field);

    const std::string& StringFuncName() const { return String(func_name); }
    const std::string& StringRefPage() const { return String(refpage); }
    const std::string& StringField() const { return String(field_name); }
};

template <typename VuidFunctor>
struct CoreErrorLocationVuidAdapter {
    const CoreErrorLocation loc;
    VuidFunctor vuid_functor;
    const char* FuncName() const {
        // the returned reference from loc must be valid for lifespan of loc, at least.
        const std::string& func_name = loc.StringFuncName();
        return func_name.c_str();
    }
    const char* Vuid() const {
        // the returned reference from functor must be valid for lifespan of vuid_functor, at least.
        const std::string& vuid = vuid_functor(loc);
        return vuid.c_str();
    }
    template <typename... Args>
    CoreErrorLocationVuidAdapter(const CoreErrorLocation& loc_, const Args&... args) : loc(loc_), vuid_functor(args...) {}
};
