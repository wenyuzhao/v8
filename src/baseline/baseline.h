// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASELINE_BASELINE_H_
#define V8_BASELINE_BASELINE_H_

#include "src/handles/handles.h"

namespace v8 {
namespace internal {

class Code;
class SharedFunctionInfo;
class BytecodeArray;

// TODO(v8:11429): Restrict header visibility to just this file.
Handle<Code> CompileWithBaseline(Isolate* isolate,
                                 Handle<SharedFunctionInfo> shared);

}  // namespace internal
}  // namespace v8

#endif  // V8_BASELINE_BASELINE_H_
