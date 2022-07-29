#pragma once

#include "taichi/taichi_platform.h"

#include "taichi/taichi_core.h"

#include "taichi/compiler/program.h"

#if TI_WITH_VULKAN
#define VK_NO_PROTOTYPES 1
#include "taichi/taichi_vulkan.h"
#endif  // TI_WITH_VULKAN
