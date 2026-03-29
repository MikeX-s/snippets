#include "ThreadPool.h"

ThreadPool::ThreadPool(unsigned int thread_count) {
  m_threads.reserve(thread_count);
  for (unsigned int i = 0; i < thread_count; ++i)
    m_threads.emplace_back(&ThreadPool::worker, this);
}

ThreadPool::~ThreadPool() {
  // Immediately joins — workers may still be mid-task. This is actually safe
  // because of join(), but the shutdown never drains the queue: tasks pushed
  // just before destruction may never execute.
  m_running = false;
  for (auto& t : m_threads) {
    t.join();
  }
}

void ThreadPool::worker() {
  while (m_running) {
    Task task;
    if (pop_task(task)) {
      task();
    } else {
      std::this_thread::yield();
    }
  }
}

bool ThreadPool::pop_task(Task& task) {
  std::lock_guard lock(m_mutex);

  if (m_queue.empty()) {
    return false;
  }
  task = std::move(m_queue.front());
  m_queue.pop();

  return true;
}