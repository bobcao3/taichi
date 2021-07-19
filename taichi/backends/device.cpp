#include "device.h"

TLANG_NAMESPACE_BEGIN

static struct {
  std::array<std::unique_ptr<Device>, taichi_max_num_devices> devices;
  uint32_t num_registered_devices;
  std::mutex mutex;
} taichi_devices;

Device *Device::get_device_by_id(uint32_t id) {
  return taichi_devices.devices[id].get();
}

uint32_t Device::register_device(std::unique_ptr<Device> &device) {
  std::lock_guard<std::mutex> lg(taichi_devices.mutex);

  if (taichi_devices.num_registered_devices >= taichi_max_num_devices) {
    TI_ERROR("Failed to add device, maximum limit reached");
    return -1;
  }

  device->device_id = taichi_devices.num_registered_devices;
  taichi_devices.devices[taichi_devices.num_registered_devices] =
      std::move(device);

  return taichi_devices.num_registered_devices++;
}

void ComputeDevice::launch_kernel(uint32_t stream_id,
                                  uint32_t kernel_id,
                                  SNodeMapping mapping) {
  SNodeMapping device_mapping;

  // TODO: Here would be a good place to query which mapping is input / output /
  // inout, and reduce the transfer calls & sync calls as possible.

  // Preambles
  for (auto &[snode_id, range] : mapping) {
    if (range.dev_id == this->device_id) {
      // Same device
      device_mapping.emplace(snode_id, range);
    } else {
      // Heterogeneous
      DevicePtr buf = nullptr;
      if (!get_local_alias(buf, range)) {
        // Alias failed, replication required
        buf = this->allocate_memory(MemoryType::DEVICE, range.length);
        this->transfer(buf, range);
      }
      device_mapping.emplace(snode_id, DeviceRange(buf, range.length));
    }
  }

  // Sync is blocking (unlike barriers), delay them to hide latency
  for (auto &[snode_id, range] : mapping) {
    if (range.dev_id != this->device_id) {
      // Heterogeneous, sync required
      this->sync_read(device_mapping[snode_id], get_device_by_id(range.dev_id));
    }
  }

  // Implementation launch
  launch_kernel_internal(stream_id, kernel_id, device_mapping);

  // Postambles
  for (auto &[snode_id, range] : mapping) {
    if (range.dev_id != this->device_id) {
      // Copy back (if required)
      DeviceRange replicated_range = device_mapping[snode_id];
      if (!is_alias(replicated_range)) {
        get_device_by_id(range.dev_id)->transfer(range, replicated_range);
      }
      this->deallocate_memory(replicated_range);
    }
  }

  for (auto &[snode_id, range] : mapping) {
    if (range.dev_id != this->device_id) {
      // Heterogeneous, sync required
      get_device_by_id(range.dev_id)->sync_read(range, this);
    }
  }
}

TLANG_NAMESPACE_END
