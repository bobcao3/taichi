#pragma once

#include "taichi/common/core.h"
#include "taichi/lang_util.h"
#include "taichi/codegen/codegen.h"

#include <map>

TLANG_NAMESPACE_BEGIN

// A device classifies as a device as long as memory can be allocated from it,
// and its allocations can be accessed from other devices
// FIXME: `DeviceCapabilities` conflicts with a type in WIN32 API
enum class DeviceCaps {
  COMPUTE = 1,   // Programmable compute capability (can run taichi code)
  GRAPHICS = 2,  // Rasterization graphics capability
  DISPLAY = 4,   // Display capability
  ACCEL_RAYTRACING = 8,  // Accelerated raytracing
  ACCEL_TENSOR = 16,     // Accelerated tensor & matrix operations
  ACCEL_CODEC = 32,      // Accelerated video / audio / compression codec
};

// A universal pointer that points to memory that could be anywhere
struct DevicePtr {
  uint32_t dev_id{0};      // The device this memory is allocated from
  uint32_t addr_space{0};  // The address space / pool this memory is from
  uint64_t ptr{0};         // The on-device pointer (bytes)

  DevicePtr(uint32_t dev_id, uint32_t addr_space, uint64_t ptr)
      : dev_id(dev_id), addr_space(addr_space), ptr(ptr) {
  }

  DevicePtr(void *host_ptr) : ptr(uint64_t(host_ptr)) {
  }
};

struct DeviceRange : public DevicePtr {
  uint64_t length{0};

  DeviceRange(uint32_t dev_id,
              uint32_t addr_space,
              uint64_t ptr,
              uint64_t length)
      : DevicePtr(dev_id, addr_space, ptr), length(length) {
  }

  DeviceRange(void *host_ptr, uint64_t length)
      : DevicePtr(host_ptr), length(length) {
  }

  DeviceRange(const DevicePtr &ptr, uint64_t length)
      : DevicePtr(ptr), length(length) {
  }
};

constexpr size_t taichi_max_num_devices = 256;

class Device {
 public:
  enum class MemoryType {
    DEVICE,          // Memory exclusive to device, assumes to be fastest
    HOST_VISIBLE,    // Memory on device, visible from host
    HOST_COHERENT,   // Coherent memory
    HOST_TO_DEVICE,  // Host writes, device reads
    DEVICE_TO_HOST   // Host reads, device writes
  };

  enum class DeviceType {
    HOST,     // The device taichi host is running on
    GPU_UMA,  // GPU with unified memory with host (e.g. consoles, M1)
    GPU,      // GPU with discrete memory
    REMOTE    // Device not local to the machine
  };

  enum class AllocationHint {
    STATIC,    // Usually resident until device is removed
    DYNAMIC,   // Resident for a while
    TRANSIENT  // For only one frame. Automatically GCed after `sync_all` call
  };

  uint32_t device_id = 0;

 public:
  virtual ~Device() = 0;

  // All device functions can be called asynchronously
  // All memory is assumed to be continous & word size aligned within its
  // address_space

  // Query type of the device
  virtual DeviceType query_type() = 0;

  // Query the device's capabilities
  virtual DeviceCaps query_capabilities() = 0;

  // Querys the avaiable amount of memory that can be allocated, based on type
  virtual size_t query_available_memory(MemoryType type) = 0;

  // Allocate contiguous chunk of memory of `size`
  virtual DevicePtr allocate_memory(MemoryType type, size_t size) = 0;

  virtual void deallocate_memory(DevicePtr ptr) = 0;

  // Alias / sharing of memory (if possible)
  // Returns true if sharing is possible
  // Deallocation of an alias simply removes the alias
  // Alias is achievable usually in:
  // - Unified memory GPU, where GPU shares host memory
  // - Same physical device but different API (e.g. Cuda <-> Vulkan)
  virtual bool get_local_alias(DevicePtr& local, DeviceRange other) = 0;

  // Check whether a device pointer is an alias
  virtual bool is_alias(DevicePtr ptr) = 0;

  // Device local barrier
  virtual void barrier(DeviceRange range) = 0;

  // Cross device sync
  // Stalls current device so that `other` device finishes modifications to the
  // range specified
  // Usually useful for transfers & host mapping
  virtual void sync_read(DeviceRange range, Device *other) = 0;

  // Cross device sync
  // Flushes the current device so that memory write are visible to `other`
  // Usually useful for host mapping
  virtual void sync_write(DeviceRange range, Device *other) = 0;

  // Map to host
  virtual void *map(DeviceRange ptr, bool host_read, bool host_write) = 0;
  virtual void unmap(DeviceRange ptr) = 0;

  // Transfer between devices (one device can be the host)
  // The target device handles this transfer (device for dst_ptr)
  // ASSERT(dst_ptr.device_id == this->device_id)
  virtual void transfer(DevicePtr dst_ptr, DeviceRange src_range) = 0;

  // Whole device sync
  // For some async implementation where data in-flight tracking is required,
  // this function essentially acts as "NewFrame"
  virtual void sync_all() = 0;

 public:
  // Get a device instance by device ID
  // Device 0 is always the host
  // Device 0 addr_space 0 is the address space of Taichi's process
  static Device *get_device_by_id(uint32_t id);

  // Register a new device, returns its assigned device id
  static uint32_t register_device(std::unique_ptr<Device> &device);
};

class ComputeDevice : public Device {
 public:
  // Most of the functionality here should be handled independently by backend

  using SNodeMapping = std::unordered_map<int, DeviceRange>;

  virtual void launch_kernel_internal(uint32_t stream_id,
                                      uint32_t kernel_id,
                                      SNodeMapping mapping) = 0;

  // TODO: Figure out how this part will work & how does it effect the code
  // currently in program.cpp, function.cpp, and codegen.cpp
  void launch_kernel(uint32_t stream_id,
                     uint32_t kernel_id,
                     SNodeMapping mapping);

  // ... APIs to generate kernels

  // Get the number of asynchronous command streams on this device
  virtual uint32_t get_num_streams() = 0;

  // Wait until the commands on the strem are complete
  virtual void wait_for_stream_complete(uint32_t stream_id) = 0;
};

class GraphicsDevice : public Device {
 public:
  // Most of the functionality here should be handled independently by backend
};

class DisplayDevice : public Device {
 public:
  enum class DisplayFormat { R8, R32F, RGBA8, RGBA16F, RGBA16F_HDR, RGBA32F };

  class View {
    // Functions to resize & configure this view (usually a window)
    // ...
  };

  // Acqurie the next image for a specific view
  virtual void acquire_next_image(View &view) = 0;

  // Present a buffer to the image acquired for the view
  virtual void present_image(View &view, DevicePtr buffer) = 0;

  // Create a new view
  virtual void create_view(int width, int height, DisplayFormat format) = 0;
};

// Accelerators are nice to have, leave it blank for now
class AccelRaytracingDevice : public Device {};
class AccelTensorDevice : public Device {};
class AccelCodecDevice : public Device {};

TLANG_NAMESPACE_END
