// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2020-2024 Fragcolor Pte. Ltd.
//
// Minimal FreeRTOSConfig.h stub for compilation
// This should be replaced with a proper configuration for your target board
//
// For actual FreeRTOS usage, you need to provide a proper FreeRTOSConfig.h
// that matches your hardware and application requirements.

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

// Minimal configuration for header-only compile check
// These values are placeholders and should be adjusted for actual hardware

// Application specific definitions
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      100000000
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                128
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   3
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               10
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIME_SLICING                  1
#define configUSE_NEWLIB_REENTRANT              0
#define configENABLE_BACKWARD_COMPATIBILITY     0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 5
#define configSTACK_DEPTH_TYPE                  uint16_t
#define configMESSAGE_BUFFER_LENGTH_TYPE        size_t

// Memory allocation related definitions
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   (64 * 1024)
#define configAPPLICATION_ALLOCATED_HEAP        0

// Hook function related definitions
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCHECK_FOR_STACK_OVERFLOW          0
#define configUSE_MALLOC_FAILED_HOOK            0
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0

// Run time and task stats gathering
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

// Co-routine definitions
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         1

// Software timer definitions
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               3
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            configMINIMAL_STACK_SIZE

// Interrupt nesting
#define configKERNEL_INTERRUPT_PRIORITY         255
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    191
#define configMAX_API_CALL_INTERRUPT_PRIORITY   configMAX_SYSCALL_INTERRUPT_PRIORITY

// Define to trap errors during development
#define configASSERT(x) if ((x) == 0) { for (;;); }

// FreeRTOS MPU specific definitions (not used on RISC-V)
#define configENABLE_FPU                        1
#define configENABLE_MPU                        0

// Optional functions
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xResumeFromISR                  1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     0
#define INCLUDE_xTaskGetIdleTaskHandle          0
#define INCLUDE_eTaskGetState                   0
#define INCLUDE_xEventGroupSetBitFromISR        1
#define INCLUDE_xTimerPendFunctionCall          0
#define INCLUDE_xTaskAbortDelay                 0
#define INCLUDE_xTaskGetHandle                  0
#define INCLUDE_xTaskResumeFromISR              1

// pdMS_TO_TICKS helper macro
#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(xTimeInMs) ((TickType_t)(((TickType_t)(xTimeInMs) * (TickType_t)configTICK_RATE_HZ) / (TickType_t)1000U))
#endif

// RISC-V specific: Machine Timer registers
// Set to 0 if no MTIME (machine time) clock, or define actual addresses for your board
#define configMTIME_BASE_ADDRESS        0
#define configMTIMECMP_BASE_ADDRESS     0

#endif /* FREERTOS_CONFIG_H */
