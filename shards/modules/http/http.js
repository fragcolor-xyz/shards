/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

// emscripten related utilities

var shards_http = {}

autoAddDeps(shards_http, '$Fetch');
mergeInto(LibraryManager.library, shards_http);
