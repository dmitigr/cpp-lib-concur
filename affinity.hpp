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

#ifdef _WIN32
#error dmitigr/concur/affinity.hpp is not usable on Microsoft Windows at the moment!
#endif

#ifndef DMITIGR_CONCUR_AFFINITY_HPP
#define DMITIGR_CONCUR_AFFINITY_HPP

#include <system_error>
#include <thread>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <pthread.h>

namespace dmitigr::concur {

/// Sets the CPU affinity of the thread `handle` to the `cpu`.
inline std::error_code set_affinity(const pthread_t handle,
  const unsigned int cpu) noexcept
{
  if (!(handle && cpu < std::thread::hardware_concurrency()))
    return std::make_error_code(std::errc::invalid_argument);

  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  const int err{pthread_setaffinity_np(handle, sizeof(cpu_set_t), &set)};
  return std::error_code{err, std::generic_category()};
}

/// @overload
inline std::error_code set_affinity(std::thread& thread,
  const unsigned int cpu) noexcept
{
  return set_affinity(thread.native_handle(), cpu);
}

} // namespace dmitigr::concur

#endif  // DMITIGR_CONCUR_AFFINITY_HPP
