// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARK_COMPACT_INL_H_
#define V8_HEAP_MARK_COMPACT_INL_H_

#include "src/base/bits.h"
#include "src/codegen/assembler-inl.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact.h"
#include "src/heap/marking-worklist-inl.h"
#include "src/heap/marking-worklist.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/remembered-set-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/slots-inl.h"
#include "src/objects/transitions.h"

namespace v8 {
namespace internal {

void MarkCompactCollector::MarkObject(HeapObject host, HeapObject obj) {
  DCHECK(!Internals::IsMapWord(obj.ptr()));
  if (marking_state()->WhiteToGrey(obj)) {
    local_marking_worklists()->Push(obj);
    if (V8_UNLIKELY(FLAG_track_retaining_path)) {
      heap_->AddRetainer(host, obj);
    }
  }
}

void MarkCompactCollector::MarkRootObject(Root root, HeapObject obj) {
  DCHECK(!Internals::IsMapWord(obj.ptr()));
  if (marking_state()->WhiteToGrey(obj)) {
    local_marking_worklists()->Push(obj);
    if (V8_UNLIKELY(FLAG_track_retaining_path)) {
      heap_->AddRetainingRoot(root, obj);
    }
  }
}

#ifdef ENABLE_MINOR_MC

void MinorMarkCompactCollector::MarkRootObject(HeapObject obj) {
  DCHECK(!Internals::IsMapWord(obj.ptr()));
  if (Heap::InYoungGeneration(obj) &&
      non_atomic_marking_state_.WhiteToGrey(obj)) {
    worklist_->Push(kMainThreadTask, obj);
  }
}

#endif

void MarkCompactCollector::MarkExternallyReferencedObject(HeapObject obj) {
  DCHECK(!Internals::IsMapWord(obj.ptr()));
  if (marking_state()->WhiteToGrey(obj)) {
    local_marking_worklists()->Push(obj);
    if (V8_UNLIKELY(FLAG_track_retaining_path)) {
      heap_->AddRetainingRoot(Root::kWrapperTracing, obj);
    }
  }
}

void MarkCompactCollector::RecordSlot(HeapObject object, ObjectSlot slot,
                                      HeapObject target) {
  DCHECK(!Internals::IsMapWord(object.ptr()));
  RecordSlot(object, HeapObjectSlot(slot), target);
}

void MarkCompactCollector::RecordSlot(HeapObject object, HeapObjectSlot slot,
                                      HeapObject target) {
  DCHECK(!Internals::IsMapWord(object.ptr()));
  BasicMemoryChunk* target_page = BasicMemoryChunk::FromHeapObject(target);
  MemoryChunk* source_page = MemoryChunk::FromHeapObject(object);
  if (target_page->IsEvacuationCandidate<AccessMode::ATOMIC>() &&
      !source_page->ShouldSkipEvacuationSlotRecording<AccessMode::ATOMIC>()) {
    RememberedSet<OLD_TO_OLD>::Insert<AccessMode::ATOMIC>(source_page,
                                                          slot.address());
  }
}

void MarkCompactCollector::RecordSlot(MemoryChunk* source_page,
                                      HeapObjectSlot slot, HeapObject target) {
  BasicMemoryChunk* target_page = BasicMemoryChunk::FromHeapObject(target);
  if (target_page->IsEvacuationCandidate<AccessMode::ATOMIC>()) {
    RememberedSet<OLD_TO_OLD>::Insert<AccessMode::ATOMIC>(source_page,
                                                          slot.address());
  }
}

void MarkCompactCollector::AddTransitionArray(TransitionArray array) {
  weak_objects_.transition_arrays.Push(kMainThreadTask, array);
}

template <typename MarkingState>
template <typename T, typename TBodyDescriptor>
int MainMarkingVisitor<MarkingState>::VisitJSObjectSubclass(Map map, T object) {
  if (!this->ShouldVisit(object)) return 0;
  this->VisitMapPointer(object);
  int size = TBodyDescriptor::SizeOf(map, object);
  TBodyDescriptor::IterateBody(map, object, size, this);
  return size;
}

template <typename MarkingState>
template <typename T>
int MainMarkingVisitor<MarkingState>::VisitLeftTrimmableArray(Map map,
                                                              T object) {
  if (!this->ShouldVisit(object)) return 0;
  int size = T::SizeFor(object.length());
  this->VisitMapPointer(object);
  T::BodyDescriptor::IterateBody(map, object, size, this);
  return size;
}

template <typename MarkingState>
template <typename TSlot>
void MainMarkingVisitor<MarkingState>::RecordSlot(HeapObject object, TSlot slot,
                                                  HeapObject target) {
  MarkCompactCollector::RecordSlot(object, slot, target);
}

template <typename MarkingState>
void MainMarkingVisitor<MarkingState>::RecordRelocSlot(Code host,
                                                       RelocInfo* rinfo,
                                                       HeapObject target) {
  MarkCompactCollector::RecordRelocSlot(host, rinfo, target);
}

template <typename MarkingState>
void MainMarkingVisitor<MarkingState>::MarkDescriptorArrayFromWriteBarrier(
    DescriptorArray descriptors, int number_of_own_descriptors) {
  // This is necessary because the Scavenger records slots only for the
  // promoted black objects and the marking visitor of DescriptorArray skips
  // the descriptors marked by the visitor.VisitDescriptors() below.
  this->MarkDescriptorArrayBlack(descriptors);
  this->VisitDescriptors(descriptors, number_of_own_descriptors);
}

template <LiveObjectIterationMode mode>
LiveObjectRange<mode>::iterator::iterator(const MemoryChunk* chunk,
                                          Bitmap* bitmap, Address start)
    : chunk_(chunk),
      one_word_filler_map_(
          ReadOnlyRoots(chunk->heap()).one_pointer_filler_map()),
      two_word_filler_map_(
          ReadOnlyRoots(chunk->heap()).two_pointer_filler_map()),
      free_space_map_(ReadOnlyRoots(chunk->heap()).free_space_map()),
      it_(chunk, bitmap) {
    cursor_ = start;
  // it_.Advance(Bitmap::IndexToCell(
  //     Bitmap::CellAlignIndex(chunk_->AddressToMarkbitIndex(start))));
  // if (!it_.Done()) {
  //   cell_base_ = it_.CurrentCellBase();
  //   current_cell_ = *it_.CurrentCell();
    AdvanceToNextValidObject();
  // }
}

template <LiveObjectIterationMode mode>
typename LiveObjectRange<mode>::iterator& LiveObjectRange<mode>::iterator::
operator++() {
  AdvanceToNextValidObject();
  return *this;
}

template <LiveObjectIterationMode mode>
typename LiveObjectRange<mode>::iterator LiveObjectRange<mode>::iterator::
operator++(int) {
  iterator retval = *this;
  ++(*this);
  return retval;
}

template <LiveObjectIterationMode mode>
void LiveObjectRange<mode>::iterator::AdvanceToNextValidObject() {
  auto chunk_end = chunk_->area_end();
  while (cursor_ < chunk_end) {
    // Get the object starting from `cursor_`
    auto obj = HeapObject::FromAddress(cursor_);
    auto map = obj.map();
    auto size = obj.SizeFromMap(map);
    cursor_ += size;
    // Return the object if it is live
    if ((mode == kBlackObjects && MarkingStateBase<void, AccessMode::ATOMIC>().IsBlack(obj))
      || (mode == kGreyObjects && MarkingStateBase<void, AccessMode::ATOMIC>().IsGrey(obj))
      || (mode == kAllLiveObjects && MarkingStateBase<void, AccessMode::ATOMIC>().IsBlackOrGrey(obj))
    ) {
      current_object_ = obj;
      current_size_ = size;
      if (cursor_ >> 19 == 0x18bdb4e80000 >> 19) {
        printf("obj: %p\n", (void*) obj.ptr());
      }
      return;
    }
  }
  cursor_ = chunk_end;
  current_object_ = HeapObject();
  current_size_ = 0;
}

template <LiveObjectIterationMode mode>
typename LiveObjectRange<mode>::iterator LiveObjectRange<mode>::begin() {
  return iterator(chunk_, bitmap_, start_);
}

template <LiveObjectIterationMode mode>
typename LiveObjectRange<mode>::iterator LiveObjectRange<mode>::end() {
  return iterator(chunk_, bitmap_, end_);
}

Isolate* MarkCompactCollectorBase::isolate() { return heap()->isolate(); }

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARK_COMPACT_INL_H_
