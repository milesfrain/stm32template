/*
 * Wrappers for creating static freeRTOS objects and some
 * other convenience functions.
 * Note that these don't have destructors because the intended
 * use case is to keep these alive for the lifetime of the program.
 */

#pragma once

#include "FreeRTOS.h"
#include "catch_errors.h"
#include "cmsis_os.h"
#include "event_groups.h"
#include "interfaces.h"
#include "message_buffer.h"
#include "queue.h"
#include "semphr.h"
#include "stream_buffer.h"
#include "task.h"

// ------- Chapter 2: Task -----------

/*
 * Constructor has near-identical API as xTaskCreate()
 * Differences are:
 *   A non-void argument type for "func" may be specified (via TArg) to improve type safety
 *   Stack depth is passed via template parameter
 *   - Note that this represents the number of WORDS used for stack storage.
 *   Task function parameters and priority are optional (uses defaults if omitted)
 *   Handle is stored within the object, so no need to create this ahead of time to pass in.
 *   Handle is retrieved via .handle
 */

template<typename TArg = void, uint32_t TDepth = configMINIMAL_STACK_SIZE * 2>
class StaticTask
{
public:
  StaticTask(const char* const name,                 // task name
             void (*func)(TArg*),                    // function to run
             TArg* const params = nullptr,           // function arguments
             UBaseType_t priority = osPriorityNormal // task priority
             )
    : handle{ xTaskCreateStatic((TaskFunction_t)func, name, TDepth, params, priority, stack, &taskData) }
  {}                         // No body in constructor
  const TaskHandle_t handle; // the task handle
private:
  StackType_t stack[TDepth]; // Static storage for task stack
  StaticTask_t taskData;     // Static storage for task bookkeeping data
};

// Convenience wrappers for xTaskNotifyFromISR

// Setting bits
inline void isrTaskNotifyBits(TaskHandle_t task, uint32_t bits)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xTaskNotifyFromISR(task, bits, eSetBits, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Increment
inline void isrTaskNotifyIncrement(TaskHandle_t task)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  // second argument is unused
  xTaskNotifyFromISR(task, 42, eIncrement, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// ------- Chapter 3: Queue -----------

// Provide the element type and the number of elements to store in the queue
template<typename T, size_t TLen = 16>
class StaticQueue
{
public:
  StaticQueue()
    : handle{ xQueueCreateStatic(TLen, sizeof(T), queueBuf, &staticQ) }
  {
    // Todo - Assign human-readable name as a debugging aid
    vQueueAddToRegistry(handle, "todoQueueName");
  }
  const QueueHandle_t handle; // the message buffer handle
private:
  uint8_t queueBuf[TLen * sizeof(T)]; // storage for queue
  StaticQueue_t staticQ;              // static storage for bookkeeping data
};

// ------- Chapter 4: Semaphore -----------

class StaticMutex
{
public:
  StaticMutex()
    : handle{ xSemaphoreCreateMutexStatic(&staticSemaphore) }
  {
    // Todo - Assign human-readable name as a debugging aid
    vQueueAddToRegistry(handle, "todoMutexName");
  }
  const SemaphoreHandle_t handle; // the mutex / semaphore handle
private:
  StaticSemaphore_t staticSemaphore; // static storage for mutex / semaphore
};

// Enables scoped locking (i.e. never forgetting to unlock a mutex).
// May provide non-maximum timeout and then call gotLock() to check if
// lock was successfully acquired.
// Note - create recursive and from-isr versions as needed
class ScopedLock
{
public:
  // Blocks forever while waiting for mutex
  ScopedLock(StaticMutex& mutex, TickType_t ticks = portMAX_DELAY)
    : mutex{ mutex }
  {
    locked = xSemaphoreTake(mutex.handle, ticks);
  }

  ~ScopedLock()
  {
    if (locked) {
      xSemaphoreGive(mutex.handle);
    }
  }

  bool gotLock() { return locked; }

private:
  StaticMutex& mutex;
  bool locked;
};

// ------- Chapter 5: Timer -----------

// ------- Chapter 6: Event Groups -----------

class StaticEventGroup
{
public:
  StaticEventGroup()
    : handle{ xEventGroupCreateStatic(&staticEventGroup) }
  {}

  const EventGroupHandle_t handle;

private:
  StaticEventGroup_t staticEventGroup;
};

// ------- Chapter 7: Kernel config - skip -----------

// ------- Chapter 8: Stream Buffer -----------

template<size_t TSize = 1024>
class StaticStreamBuffer
  : public Readable
  , public Writable
{
public:
  StaticStreamBuffer()
    : handle{ xStreamBufferCreateStatic(TSize,
                                        1, // trigger level. Unblock as soon as any bytes are available
                                        mbStorage,
                                        &staticMb) }
  {} // No body in constructor

  size_t write(const void* buf, size_t len, TickType_t ticks) //
  {
    return xStreamBufferSend(handle, buf, len, ticks);
  }

  size_t read(void* buf, size_t len, TickType_t ticks) //
  {
    return xStreamBufferReceive(handle, buf, len, ticks);
  }

  const StreamBufferHandle_t handle; // the message buffer handle

private:
  uint8_t mbStorage[TSize + 1];  // storage for message buffer, must contain an additional byte
  StaticStreamBuffer_t staticMb; // static storage for bookkeeping data
};

// ------- Chapter 9: Message Buffer -----------

template<size_t TSize = 1024>
class StaticMessageBuffer
  : public Readable
  , public Writable
{
public:
  StaticMessageBuffer()
    : handle{ xMessageBufferCreateStatic(TSize, mbStorage, &staticMb) }
  {} // No body in constructor

  size_t write(const void* buf, size_t len, TickType_t ticks) //
  {
    return xMessageBufferSend(handle, buf, len, ticks);
  }

  size_t read(void* buf, size_t len, TickType_t ticks)
  {

    // Attempt to get next message
    size_t retLen = xMessageBufferReceive(handle, buf, len, ticks);

    // Check if zero returned because next message is too big to fit in buffer
    size_t nextLen = nextLengthBytes();
    if (retLen == 0 && len < nextLen) {
      critical();
    }

    return retLen;
  }

  size_t nextLengthBytes() //
  {
    return xMessageBufferNextLengthBytes(handle);
  }

  const MessageBufferHandle_t handle; // the message buffer handle

private:
  uint8_t mbStorage[TSize + 1];   // storage for message buffer, must contain an additional byte
  StaticMessageBuffer_t staticMb; // static storage for bookkeeping data
};
