/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

#ifndef SHARDS_SWIFT_H
#define SHARDS_SWIFT_H

#include "shards.h"

struct SwiftShard {
  struct Shard header;
  void *swiftClass;
};

#endif