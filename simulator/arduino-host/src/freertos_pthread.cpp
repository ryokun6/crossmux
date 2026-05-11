// FreeRTOS API implemented on top of std::thread / std::mutex / std::condition_variable.
// This is the default model; matches real FreeRTOS semantics closely enough that application
// code written for the chip (xTaskCreate + blocking ulTaskNotifyTake etc.) runs unchanged.
//
// Naming note: "pthread" here is shorthand for "uses real OS threads", not POSIX pthread API.
// The implementation uses C++ standard concurrency.

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// ---------- Task ----------

struct TaskState_ {
  std::thread thread;
  std::atomic<bool> stopRequested{false};

  std::mutex notifyMutex;
  std::condition_variable notifyCv;
  uint32_t notifyValue = 0;
  bool notifyPending = false;
};

namespace {
thread_local TaskState_* tls_current_task = nullptr;

// Held while we're inside a critical section. Recursive to match FreeRTOS taskENTER_CRITICAL nesting.
std::recursive_mutex& critical_mutex() {
  static std::recursive_mutex m;
  return m;
}
}  // namespace

BaseType_t xTaskCreate(TaskFunction_t code, const char* /*name*/, uint32_t /*stack*/, void* params,
                       UBaseType_t /*priority*/, TaskHandle_t* out) {
  auto* state = new TaskState_();
  state->thread = std::thread([state, code, params]() {
    tls_current_task = state;
    code(params);
    tls_current_task = nullptr;
  });
  if (out) *out = state;
  return pdPASS;
}

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t code, const char* name, uint32_t stack, void* params,
                                   UBaseType_t priority, TaskHandle_t* out, BaseType_t /*coreId*/) {
  return xTaskCreate(code, name, stack, params, priority, out);
}

void vTaskDelete(TaskHandle_t task) {
  if (!task) task = tls_current_task;
  if (!task) return;
  task->stopRequested = true;
  if (task->thread.joinable()) {
    if (task->thread.get_id() == std::this_thread::get_id()) {
      // Task is deleting itself; detach and let the thread complete naturally.
      task->thread.detach();
      // The TaskState_ leaks here — that's intentional, can't free it from inside.
      return;
    }
    task->thread.join();
  }
  delete task;
}

void vTaskDelay(const TickType_t ticks) {
  if (ticks == 0) {
    std::this_thread::yield();
    return;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(ticks));
}

void vTaskSuspend(TaskHandle_t /*task*/) {}
void vTaskResume(TaskHandle_t /*task*/) {}

TaskHandle_t xTaskGetCurrentTaskHandle() { return tls_current_task; }

UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t /*task*/) {
  // Stack accounting is meaningless on host; return a sane non-zero value.
  return 4096;
}

BaseType_t xTaskNotify(TaskHandle_t task, uint32_t value, eNotifyAction action) {
  if (!task) return pdFAIL;
  std::lock_guard<std::mutex> lock(task->notifyMutex);
  switch (action) {
    case eNoAction:
      break;
    case eSetBits:
      task->notifyValue |= value;
      break;
    case eIncrement:
      task->notifyValue += 1;
      break;
    case eSetValueWithOverwrite:
      task->notifyValue = value;
      break;
    case eSetValueWithoutOverwrite:
      if (!task->notifyPending) task->notifyValue = value;
      break;
  }
  task->notifyPending = true;
  task->notifyCv.notify_one();
  return pdPASS;
}

BaseType_t xTaskNotifyGive(TaskHandle_t task) { return xTaskNotify(task, 0, eIncrement); }

uint32_t ulTaskNotifyTake(BaseType_t clearOnExit, TickType_t ticksToWait) {
  TaskState_* self = tls_current_task;
  if (!self) return 0;  // Not in a registered task context.
  std::unique_lock<std::mutex> lock(self->notifyMutex);
  if (ticksToWait == portMAX_DELAY) {
    self->notifyCv.wait(lock, [self]() { return self->notifyPending; });
  } else {
    self->notifyCv.wait_for(lock, std::chrono::milliseconds(ticksToWait), [self]() { return self->notifyPending; });
  }
  uint32_t value = self->notifyValue;
  if (clearOnExit == pdTRUE) {
    self->notifyValue = 0;
    self->notifyPending = false;
  } else {
    if (self->notifyValue > 0) self->notifyValue -= 1;
    if (self->notifyValue == 0) self->notifyPending = false;
  }
  return value;
}

BaseType_t xTaskNotifyWait(uint32_t /*clearEntry*/, uint32_t clearExit, uint32_t* outValue, TickType_t ticksToWait) {
  TaskState_* self = tls_current_task;
  if (!self) return pdFAIL;
  std::unique_lock<std::mutex> lock(self->notifyMutex);
  bool got = false;
  if (ticksToWait == portMAX_DELAY) {
    self->notifyCv.wait(lock, [self]() { return self->notifyPending; });
    got = true;
  } else {
    got = self->notifyCv.wait_for(lock, std::chrono::milliseconds(ticksToWait), [self]() { return self->notifyPending; });
  }
  if (outValue) *outValue = self->notifyValue;
  if (got) {
    self->notifyValue &= ~clearExit;
    self->notifyPending = self->notifyValue != 0;
  }
  return got ? pdPASS : pdFAIL;
}

// ---------- Semaphore ----------

struct Semaphore_ {
  enum Kind { Mutex, Recursive, Binary, Counting };
  Kind kind;
  std::mutex mutex;
  std::condition_variable cv;
  std::recursive_mutex recursive;
  uint32_t count = 0;
  uint32_t maxCount = 1;
  // Tracked for Mutex/Binary/Counting; recursive mutex holder is not tracked.
  TaskHandle_t holder = nullptr;
};

SemaphoreHandle_t xSemaphoreCreateMutex() {
  auto* s = new Semaphore_{};
  s->kind = Semaphore_::Mutex;
  s->count = 1;
  s->maxCount = 1;
  return s;
}

SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() {
  auto* s = new Semaphore_{};
  s->kind = Semaphore_::Recursive;
  return s;
}

SemaphoreHandle_t xSemaphoreCreateBinary() {
  auto* s = new Semaphore_{};
  s->kind = Semaphore_::Binary;
  s->count = 0;
  s->maxCount = 1;
  return s;
}

SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t maxCount, UBaseType_t initialCount) {
  auto* s = new Semaphore_{};
  s->kind = Semaphore_::Counting;
  s->count = initialCount;
  s->maxCount = maxCount;
  return s;
}

void vSemaphoreDelete(SemaphoreHandle_t sem) { delete sem; }

BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t ticksToWait) {
  if (!sem) return pdFAIL;
  if (sem->kind == Semaphore_::Recursive) {
    sem->recursive.lock();  // Recursive mutex has no try_lock_for; assume success.
    return pdPASS;
  }
  std::unique_lock<std::mutex> lock(sem->mutex);
  auto avail = [sem]() { return sem->count > 0; };
  if (ticksToWait == portMAX_DELAY) {
    sem->cv.wait(lock, avail);
  } else {
    if (!sem->cv.wait_for(lock, std::chrono::milliseconds(ticksToWait), avail)) return pdFAIL;
  }
  sem->count -= 1;
  sem->holder = tls_current_task;
  return pdPASS;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t sem) {
  if (!sem) return pdFAIL;
  if (sem->kind == Semaphore_::Recursive) {
    sem->recursive.unlock();
    return pdPASS;
  }
  std::lock_guard<std::mutex> lock(sem->mutex);
  sem->holder = nullptr;
  if (sem->count < sem->maxCount) sem->count += 1;
  sem->cv.notify_one();
  return pdPASS;
}

BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t sem, TickType_t ticksToWait) { return xSemaphoreTake(sem, ticksToWait); }
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t sem) { return xSemaphoreGive(sem); }

BaseType_t xSemaphoreTakeFromISR(SemaphoreHandle_t sem, BaseType_t* /*woken*/) { return xSemaphoreTake(sem, 0); }
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t sem, BaseType_t* /*woken*/) { return xSemaphoreGive(sem); }

// ---------- Queue ----------

struct Queue_ {
  size_t itemSize;
  std::mutex mutex;
  std::condition_variable cv;
  std::deque<std::vector<uint8_t>> items;
  size_t maxLen;
};

QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t itemSize) {
  auto* q = new Queue_{};
  q->itemSize = itemSize;
  q->maxLen = length;
  return q;
}

void vQueueDelete(QueueHandle_t q) { delete q; }

BaseType_t xQueueSend(QueueHandle_t q, const void* item, TickType_t ticksToWait) {
  if (!q) return pdFAIL;
  std::unique_lock<std::mutex> lock(q->mutex);
  auto canSend = [q]() { return q->items.size() < q->maxLen; };
  if (ticksToWait == portMAX_DELAY) {
    q->cv.wait(lock, canSend);
  } else if (!canSend()) {
    if (!q->cv.wait_for(lock, std::chrono::milliseconds(ticksToWait), canSend)) return pdFAIL;
  }
  std::vector<uint8_t> buf(q->itemSize);
  std::memcpy(buf.data(), item, q->itemSize);
  q->items.push_back(std::move(buf));
  q->cv.notify_one();
  return pdPASS;
}

BaseType_t xQueueSendFromISR(QueueHandle_t q, const void* item, BaseType_t* /*woken*/) {
  return xQueueSend(q, item, 0);
}

BaseType_t xQueueReceive(QueueHandle_t q, void* out, TickType_t ticksToWait) {
  if (!q) return pdFAIL;
  std::unique_lock<std::mutex> lock(q->mutex);
  auto canRecv = [q]() { return !q->items.empty(); };
  if (ticksToWait == portMAX_DELAY) {
    q->cv.wait(lock, canRecv);
  } else if (!canRecv()) {
    if (!q->cv.wait_for(lock, std::chrono::milliseconds(ticksToWait), canRecv)) return pdFAIL;
  }
  std::memcpy(out, q->items.front().data(), q->itemSize);
  q->items.pop_front();
  q->cv.notify_one();
  return pdPASS;
}

UBaseType_t uxQueueMessagesWaiting(QueueHandle_t q) {
  if (!q) return 0;
  std::lock_guard<std::mutex> lock(q->mutex);
  return static_cast<UBaseType_t>(q->items.size());
}

BaseType_t xQueuePeek(QueueHandle_t q, void* out, TickType_t ticksToWait) {
  if (!q) return pdFAIL;
  std::unique_lock<std::mutex> lock(q->mutex);
  auto canRecv = [q]() { return !q->items.empty(); };
  if (ticksToWait == portMAX_DELAY) {
    q->cv.wait(lock, canRecv);
  } else if (!canRecv()) {
    if (!q->cv.wait_for(lock, std::chrono::milliseconds(ticksToWait), canRecv)) return pdFAIL;
  }
  std::memcpy(out, q->items.front().data(), q->itemSize);
  return pdPASS;
}

// xSemaphoreGetMutexHolder: returns the TaskHandle_t recorded on the most recent
// successful take, or nullptr if the mutex is free. Only Mutex/Binary/Counting
// kinds track the holder; recursive mutex always reports nullptr.
TaskState_* xSemaphoreGetMutexHolder(SemaphoreHandle_t sem) {
  if (!sem) return nullptr;
  std::lock_guard<std::mutex> lock(sem->mutex);
  return sem->holder;
}

// xQueuePeek overload for SemaphoreHandle_t: returns pdTRUE if the mutex is currently
// available (not held), pdFALSE if held. Best-effort: try_lock and immediately release.
BaseType_t xQueuePeek(Semaphore_* sem, void* /*buf*/, TickType_t /*ticks*/) {
  if (!sem) return pdFAIL;
  if (sem->kind == Semaphore_::Recursive) {
    if (sem->recursive.try_lock()) {
      sem->recursive.unlock();
      return pdTRUE;
    }
    return pdFALSE;
  }
  std::lock_guard<std::mutex> lock(sem->mutex);
  return sem->count > 0 ? pdTRUE : pdFALSE;
}

// ---------- Critical sections ----------

void taskENTER_CRITICAL(portMUX_TYPE* /*mux*/) { critical_mutex().lock(); }
void taskEXIT_CRITICAL(portMUX_TYPE* /*mux*/) { critical_mutex().unlock(); }
