// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/bigint/vector-arithmetic.h"

#include "src/bigint/bigint-internal.h"
#include "src/bigint/digit-arithmetic.h"

namespace v8 {
namespace bigint {

void AddAt(RWDigits Z, Digits X) {
  X.Normalize();
  if (X.len() == 0) return;
  digit_t carry = 0;
  int i = 0;
  for (; i < X.len(); i++) {
    Z[i] = digit_add3(Z[i], X[i], carry, &carry);
  }
  for (; carry != 0; i++) {
    Z[i] = digit_add2(Z[i], carry, &carry);
  }
}

void SubAt(RWDigits Z, Digits X) {
  X.Normalize();
  digit_t borrow = 0;
  int i = 0;
  for (; i < X.len(); i++) {
    Z[i] = digit_sub2(Z[i], X[i], borrow, &borrow);
  }
  for (; borrow != 0; i++) {
    Z[i] = digit_sub(Z[i], borrow, &borrow);
  }
}

void Add(RWDigits Z, Digits X, Digits Y) {
  if (X.len() < Y.len()) {
    return Add(Z, Y, X);
  }
  int i = 0;
  digit_t carry = 0;
  for (; i < Y.len(); i++) {
    Z[i] = digit_add3(X[i], Y[i], carry, &carry);
  }
  for (; i < X.len(); i++) {
    Z[i] = digit_add2(X[i], carry, &carry);
  }
  for (; i < Z.len(); i++) {
    Z[i] = carry;
    carry = 0;
  }
}

void Subtract(RWDigits Z, Digits X, Digits Y) {
  X.Normalize();
  Y.Normalize();
  DCHECK(X.len() >= Y.len());
  int i = 0;
  digit_t borrow = 0;
  for (; i < Y.len(); i++) {
    Z[i] = digit_sub2(X[i], Y[i], borrow, &borrow);
  }
  for (; i < X.len(); i++) {
    Z[i] = digit_sub(X[i], borrow, &borrow);
  }
  DCHECK(borrow == 0);  // NOLINT(readability/check)
  for (; i < Z.len(); i++) Z[i] = 0;
}

digit_t AddAndReturnCarry(RWDigits Z, Digits X, Digits Y) {
  DCHECK(Z.len() >= Y.len() && X.len() >= Y.len());
  digit_t carry = 0;
  for (int i = 0; i < Y.len(); i++) {
    Z[i] = digit_add3(X[i], Y[i], carry, &carry);
  }
  return carry;
}

digit_t SubtractAndReturnBorrow(RWDigits Z, Digits X, Digits Y) {
  DCHECK(Z.len() >= Y.len() && X.len() >= Y.len());
  digit_t borrow = 0;
  for (int i = 0; i < Y.len(); i++) {
    Z[i] = digit_sub2(X[i], Y[i], borrow, &borrow);
  }
  return borrow;
}

}  // namespace bigint
}  // namespace v8
