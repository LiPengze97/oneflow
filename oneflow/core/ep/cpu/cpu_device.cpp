/*
Copyright 2020 The OneFlow Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include "oneflow/core/ep/cpu/cpu_device.h"
#include "oneflow/core/ep/cpu/cpu_event.h"
#include "oneflow/core/ep/cpu/cpu_stream.h"
#include "oneflow/core/ep/include/device_manager_registry.h"

namespace oneflow {

namespace ep {

void CpuDevice::SetAsActiveDevice() {}

Stream* CpuDevice::CreateStream() { return new CpuStream(this); }

void CpuDevice::DestroyStream(Stream* stream) { delete stream; }

void CpuDevice::CreateEvents(Event** events, size_t count) {
  for (size_t i = 0; i < count; ++i) { events[i] = new CpuEvent(); }
}

void CpuDevice::DestroyEvents(Event** events, size_t count) {
  for (size_t i = 0; i < count; ++i) { delete events[i]; }
}

Maybe<void> CpuDevice::Alloc(const AllocationOptions& options, void** ptr, size_t size) {
  if (options.HasPinnedDevice()) {
    auto device = Global<ep::DeviceManagerRegistry>::Get()->GetDevice(
        options.GetPinnedDeviceType(), options.GetPinnedDeviceIndex());
    CHECK_OR_RETURN(device);
    return device->AllocPinned(options, ptr, size);
  } else {
    *ptr = aligned_alloc(kMaxAlignmentRequirement, size);
    if (*ptr == nullptr) {
      return Error::RuntimeError() << "allocate failed";
    } else {
      return Maybe<void>::Ok();
    }
  }
}

void CpuDevice::Free(const AllocationOptions& options, void* ptr) {
  if (options.HasPinnedDevice()) {
    auto device = Global<ep::DeviceManagerRegistry>::Get()->GetDevice(
        options.GetPinnedDeviceType(), options.GetPinnedDeviceIndex());
    CHECK(device);
    return device->FreePinned(options, ptr);
  } else {
    free(ptr);  // NOLINT
  }
}

Maybe<void> CpuDevice::AllocPinned(const AllocationOptions& options, void** ptr, size_t size) {
  UNIMPLEMENTED_THEN_RETURN();
}

void CpuDevice::FreePinned(const AllocationOptions& options, void* ptr) { UNIMPLEMENTED(); }

}  // namespace ep

}  // namespace oneflow
