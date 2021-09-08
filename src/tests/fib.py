# SPDX-License-Identifier: BSD-3-Clause
# Copyright © 2020 Fragcolor Pte. Ltd.

def fib(n):
  if n < 2: return n
  return fib(n - 1) + fib(n - 2)

print(fib(34))
