#ifndef __INCLUDE_TAICHI_TAICHI_H__
#define __INCLUDE_TAICHI_TAICHI_H__

#include "taichi_platform.h"

#include "taichi_core.h"

#include "program.h"

#if TI_WITH_VULKAN
#define VK_NO_PROTOTYPES 1
#include "taichi_vulkan.h"
#endif  // TI_WITH_VULKAN

#endif // __INCLUDE_TAICHI_TAICHI_H__