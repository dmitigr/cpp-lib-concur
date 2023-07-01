// -*- C++ -*-
//
// Copyright 2023 Dmitry Igrishin
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef DMITIGR_CONCUR_SIMPLE_THREAD_POOL_HPP
#define DMITIGR_CONCUR_SIMPLE_THREAD_POOL_HPP

#include "../base/assert.hpp"
#include "exceptions.hpp"

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace dmitigr::concur {

/// Simple thread pool.
class Simple_thread_pool final {
public:
  /// A logger.
  using Logger = std::function<void(std::string_view)>;

  /// The destructor.
  ~Simple_thread_pool()
  {
    const std::lock_guard lg{work_mutex_};
    is_started_ = false;
    state_changed_.notify_all();
    for (auto& worker : workers_) {
      DMITIGR_ASSERT(worker.joinable());
      worker.join();
    }
  }

  /// @name Constructors
  /// @{

  /// Non copy-consructible.
  Simple_thread_pool(const Simple_thread_pool&) = delete;

  /// Non copy-assignable.
  Simple_thread_pool& operator=(const Simple_thread_pool&) = delete;

  /// Non move-constructible.
  Simple_thread_pool(Simple_thread_pool&&) = delete;

  /// Non move-assignable.
  Simple_thread_pool& operator=(Simple_thread_pool&&) = delete;

  /**
   * @brief Constructs the thread pool of size `std::thread::hardware_concurrency()`.
   *
   * @see Simple_thread_pool(std::size_t, Logger)
   */
  Simple_thread_pool(Logger logger = {})
    : Simple_thread_pool{std::thread::hardware_concurrency(), std::move(logger)}
  {}

  /**
   * @brief Constructs the thread pool of the given `size`.
   *
   * @param size The size of the thread pool.
   * @param logger A logger to use to report a error message of an exception
   * thrown in a thread.
   *
   * @par Requires
   * `size > 0`.
   */
  explicit Simple_thread_pool(const std::size_t size, Logger logger = {})
    : logger_{std::move(logger)}
  {
    if (!size)
      throw Exception{"cannot create thread pool: empty pool is not allowed"};

    is_started_ = true;
    workers_.resize(size);
    for (auto& worker : workers_)
      worker = std::thread{&Simple_thread_pool::wait_and_run, this};
  }

  /// @}

  /**
   * @brief Submit the function to run on the thread pool.
   *
   * @par Requires
   * `function`.
   */
  void submit(std::function<void()> function)
  {
    if (!function)
      throw Exception{"thread pool worker is invalid"};

    const std::lock_guard lg{queue_mutex_};
    queue_.push(std::move(function));
    state_changed_.notify_one();
  }

  /// Clears the queue of unstarted works.
  void clear() noexcept
  {
    const std::lock_guard lg{queue_mutex_};
    queue_ = {};
  }

  /// @returns The size of work queue.
  std::size_t queue_size() const noexcept
  {
    const std::lock_guard lg{queue_mutex_};
    return queue_.size();
  }

  /// @returns The thread pool size.
  std::size_t size() const noexcept
  {
    const std::lock_guard lg{work_mutex_};
    return workers_.size();
  }

private:
  std::condition_variable state_changed_;
  mutable std::mutex queue_mutex_;
  std::queue<std::function<void()>> queue_;
  mutable std::mutex work_mutex_;
  std::vector<std::thread> workers_;
  bool is_started_{};
  Logger logger_;

  void wait_and_run() noexcept
  {
    while (true) {
      try {
        std::function<void()> func;
        {
          std::unique_lock lk{queue_mutex_};
          state_changed_.wait(lk, [this]
          {
            return !queue_.empty() || !is_started_;
          });
          if (is_started_) {
            DMITIGR_ASSERT(!queue_.empty());
            func = std::move(queue_.front());
            DMITIGR_ASSERT(func);
            queue_.pop();
          } else
            return;
        }
        func();
      } catch (const std::exception& e) {
        log_error(e.what());
      } catch (...) {
        log_error("unknown error");
      }
    }
  }

  void log_error(const std::string_view what) const noexcept
  {
    try {
      if (logger_)
        logger_(what);
    } catch (...){}
  }
};

} // namespace dmitigr::concur

#endif  // DMITIGR_CONCUR_SIMPLE_THREAD_POOL_HPP
