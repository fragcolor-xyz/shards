# SPDX-License-Identifier: BSD-3-Clause
# Copyright © 2020 Fragcolor Pte. Ltd.

import asyncio
import sys

# this is necessary seems...
# see: https://bugs.python.org/issue34679
policy = asyncio.get_event_loop_policy()
policy._loop_factory = asyncio.SelectorEventLoop
loop = asyncio.new_event_loop()
asyncio.set_event_loop(loop)

def run_once():
    print("run_once")
    sys.stdout.flush()
    loop.call_soon(loop.stop)
    loop.run_forever()

async def test():
    # our async task
    print("Pre-Await")
    sys.stdout.flush()
    await asyncio.sleep(1)
    print("Post-Await")
    sys.stdout.flush()

task = loop.create_task(test())

def inputTypes():
    return ["None"]

def outputTypes():
    return ["StringSeq"]

def activate(inputs):
    # we tick the async task here
    run_once()
    print(task.done())
    sys.stdout.flush()
    return ["Hello", "World"]
