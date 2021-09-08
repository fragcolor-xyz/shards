# SPDX-License-Identifier: BSD-3-Clause
# Copyright © 2019 Fragcolor Pte. Ltd.

import cProfile

def process_one():
  x = 0
  for i in range(18000000):
    x = x + 1

cProfile.run('process_one()')
cProfile.run('process_one()')
cProfile.run('process_one()')
cProfile.run('process_one()')
