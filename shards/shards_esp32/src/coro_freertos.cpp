// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2020-2024 Fragcolor Pte. Ltd.

#include "platform.hpp"

#if SH_FREERTOS

#include "coro.hpp"
#include <esp_log.h>

static const char* TAG = "shards_fiber";

namespace shards {

void FreeRTOSFiber::taskEntry(void* param) {
  auto* fiber = static_cast<FreeRTOSFiber*>(param);

  // Wait for first resume signal before executing
  xEventGroupWaitBits(fiber->eventGroup, BIT_RESUME, pdTRUE, pdFALSE, portMAX_DELAY);

  // Execute the fiber function
  if (fiber->func) {
    fiber->func();
  }

  // Mark as finished
  fiber->finished = true;
  xEventGroupSetBits(fiber->eventGroup, BIT_FINISHED | BIT_SUSPENDED);

  // Task will be deleted by destructor, suspend indefinitely
  vTaskSuspend(nullptr);
}

FreeRTOSFiber::~FreeRTOSFiber() {
  if (taskHandle) {
    // Signal task to exit if still running
    if (!finished) {
      finished = true;
      xEventGroupSetBits(eventGroup, BIT_RESUME);
      // Give it a moment to notice
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    vTaskDelete(taskHandle);
    taskHandle = nullptr;
  }
  if (eventGroup) {
    vEventGroupDelete(eventGroup);
    eventGroup = nullptr;
  }
}

void FreeRTOSFiber::init(std::function<void()> fn) {
  func = std::move(fn);
  finished = false;
  started = false;

  // Create event group for synchronization
  eventGroup = xEventGroupCreate();
  if (!eventGroup) {
    ESP_LOGE(TAG, "Failed to create event group");
    return;
  }

  // Convert stack size from bytes to words (FreeRTOS uses words)
  const size_t stackWords = stackSize / sizeof(StackType_t);

  // Create the fiber task
  BaseType_t result = xTaskCreate(
      taskEntry,
      "shards_fiber",
      stackWords,
      this,
      tskIDLE_PRIORITY + 1,
      &taskHandle
  );

  if (result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create fiber task, stack size: %d words", (int)stackWords);
    vEventGroupDelete(eventGroup);
    eventGroup = nullptr;
    taskHandle = nullptr;
    return;
  }

  // Run until first suspend point
  resume();
}

void FreeRTOSFiber::resume() {
  if (finished || !taskHandle || !eventGroup) {
    return;
  }

  // Clear suspended bit, set resume bit
  xEventGroupClearBits(eventGroup, BIT_SUSPENDED);
  xEventGroupSetBits(eventGroup, BIT_RESUME);

  // Wait for fiber to suspend or finish
  EventBits_t bits = xEventGroupWaitBits(
      eventGroup,
      BIT_SUSPENDED | BIT_FINISHED,
      pdFALSE,  // Don't clear on exit
      pdFALSE,  // Wait for any bit
      portMAX_DELAY
  );

  if (bits & BIT_FINISHED) {
    finished = true;
  }
}

void FreeRTOSFiber::suspend() {
  if (finished || !taskHandle || !eventGroup) {
    return;
  }

  // Signal that we're suspended
  xEventGroupSetBits(eventGroup, BIT_SUSPENDED);

  // Wait for resume signal
  xEventGroupWaitBits(
      eventGroup,
      BIT_RESUME,
      pdTRUE,   // Clear on exit
      pdFALSE,  // Wait for any bit
      portMAX_DELAY
  );

  // Clear suspended bit now that we're resuming
  xEventGroupClearBits(eventGroup, BIT_SUSPENDED);
}

} // namespace shards

#endif // SH_FREERTOS
