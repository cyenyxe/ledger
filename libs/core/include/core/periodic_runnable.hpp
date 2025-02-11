#pragma once
//------------------------------------------------------------------------------
//
//   Copyright 2018-2019 Fetch.AI Limited
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
//------------------------------------------------------------------------------

#include "core/runnable.hpp"

#include <chrono>
#include <functional>

namespace fetch {
namespace core {

class PeriodicRunnable : public Runnable
{
public:
  using Clock     = std::chrono::steady_clock;
  using Timepoint = Clock::time_point;
  using Duration  = Clock::duration;

  // Construction / Destruction
  template <typename R, typename P>
  explicit PeriodicRunnable(std::chrono::duration<R, P> const &period);
  explicit PeriodicRunnable(Duration const &period);
  PeriodicRunnable(PeriodicRunnable const &) = delete;
  PeriodicRunnable(PeriodicRunnable &&)      = delete;
  ~PeriodicRunnable() override               = default;

  /// @name Runnable Interface
  /// @{
  bool IsReadyToExecute() const final;
  void Execute() final;
  /// @}

  /// @name Periodic Runnable Interface
  /// @{
  virtual void Periodically() = 0;
  /// @}

  // Operators
  PeriodicRunnable &operator=(PeriodicRunnable const &) = delete;
  PeriodicRunnable &operator=(PeriodicRunnable &&) = delete;

private:
  Timepoint last_executed_;
  Duration  interval_{};
};

template <typename R, typename P>
PeriodicRunnable::PeriodicRunnable(std::chrono::duration<R, P> const &period)
  : PeriodicRunnable(std::chrono::duration_cast<Duration>(period))
{}

}  // namespace core
}  // namespace fetch
