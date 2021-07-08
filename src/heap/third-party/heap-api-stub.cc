// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/isolate-utils-inl.h"
#include "src/heap/heap-inl.h"
#include "src/heap/third-party/heap-api.h"

namespace v8 {
namespace internal {

Isolate* Heap::GetIsolateFromWritableObject(HeapObject object) {
  return GetHeapFromWritableObject(object)->isolate();
}

}  // namespace internal
}  // namespace v8

namespace v8 {
namespace internal {
namespace third_party_heap {

class Impl {};

// static
std::unique_ptr<Heap> Heap::New(v8::internal::Isolate*) { return nullptr; }

// static
v8::internal::Isolate* Heap::GetIsolate(Address) { return nullptr; }

AllocationResult Heap::Allocate(size_t, AllocationType, AllocationAlignment) {
  return AllocationResult();
}

Address Heap::GetObjectFromInnerPointer(Address) { return 0; }

const base::AddressRegion& Heap::GetCodeRange() {
  static const base::AddressRegion no_region(0, 0);
  return no_region;
}

bool Heap::IsPendingAllocation(HeapObject) { return false; }

// static
bool Heap::InSpace(Address, AllocationSpace) { return false; }

// static
bool Heap::InOldSpace(Address) { return false; }

// static
bool Heap::InCodeSpace(Address) { return false; }

// static
bool Heap::InReadOnlySpace(Address) { return false; }

// static
bool Heap::IsValidHeapObject(HeapObject) { return false; }

bool Heap::CollectGarbage() { return false; }

template <typename TSlot>
void Heap::WriteBarrierForRange(HeapObject object, TSlot start_slot, TSlot end_slot) {}

template void Heap::WriteBarrierForRange<ObjectSlot>(HeapObject object, ObjectSlot start_slot, ObjectSlot end_slot);
template void Heap::WriteBarrierForRange<MaybeObjectSlot>(HeapObject object, MaybeObjectSlot start_slot, MaybeObjectSlot end_slot);

void Heap::ClearRecordedSlot(HeapObject object, ObjectSlot slot) {}

void Heap::ClearRecordedSlotRange(Address start, Address end) {}

void Heap::WriteBarrier(Address object, Address slot, Address value) {}

}  // namespace third_party_heap
}  // namespace internal
}  // namespace v8
