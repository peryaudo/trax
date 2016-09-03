// Copyright (C) 2016 Tetsui Ohkubo.

#ifndef TIMER_H_
#define TIMER_H_

#include <unistd.h>

#if defined __MACH__
#include <mach/mach_time.h>
#elif defined _POSIX_MONOTONIC_CLOCK
#include <time.h>
#else
#error no way to get accurate time.
#endif

#include <cstdint>
#include <mutex>   // NOLINT
#include <thread>  // NOLINT

static const uint64_t kNanosecondsToMilliseconds = 1000000;
static const uint64_t kNanosecondsToSeconds = 1000000000;

class Timer {
 public:
  // Check() will always return false (the timer will never expire)
  // by setting timeout_ms negative.
  // The timer will start right after the constructor is called.
  explicit Timer(int timeout_ms = -1)
      : timeout_ms_(timeout_ms)
      , node_count_(0)
      , completed_depth_(0)
      , mutex_() {
    GetAccurateCurrentTime(&begin_time_);
    GetAccurateCurrentTime(&current_time_);
  }

  // Return true if the timer is expired.
  bool CheckTimeout(bool allow_false_negative = false) {
    if (allow_false_negative) {
      if (node_count_ % 100 > 0) {
        return false;
      }
    }
    // std::lock_guard<std::mutex> lock(mutex_);
    GetAccurateCurrentTime(&current_time_);

    if (timeout_ms_ < 0) {
      return false;
    }

    if (DiffAccurateTime(current_time_, begin_time_) >
        static_cast<uint64_t>(timeout_ms_) * kNanosecondsToMilliseconds) {
      return true;
    }

    return false;
  }

  // Should be called from the bottom of the search.
  void IncrementNodeCounter() {
    // std::lock_guard<std::mutex> lock(mutex_);
    ++node_count_;
  }

  // Return node per second value.
  int nps() {
    uint64_t diff = DiffAccurateTime(current_time_, begin_time_);
    if (diff == 0) {
      return 0;
    }

    return node_count_ * kNanosecondsToSeconds / diff;
  }

  // Return elapsed milliseconds since the Timer constructor is called.
  int elapsed_ms() {
    return DiffAccurateTime(current_time_, begin_time_) /
      kNanosecondsToMilliseconds;
  }

  int timeout_ms() { return timeout_ms_; }

  int completed_depth() { return completed_depth_; }

  void set_completed_depth(int completed_depth) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (completed_depth_ < completed_depth) {
      completed_depth_ = completed_depth;
    }
  }

 private:
#if defined __MACH__

  using TimeType = uint64_t;

  static void GetAccurateCurrentTime(TimeType *current_time) {
    mach_timebase_info_data_t base;
    mach_timebase_info(&base);
    *current_time = mach_absolute_time() / base.denom;
  }

  static uint64_t DiffAccurateTime(const TimeType& after,
                                   const TimeType& before) {
    return after - before;
  }

#elif defined _POSIX_MONOTONIC_CLOCK

  using TimeType = struct timespec;

  static void GetAccurateCurrentTime(TimeType *current_time) {
    clock_gettime(CLOCK_MONOTONIC, current_time);
  }

  static uint64_t DiffAccurateTime(const TimeType& after,
                                   const TimeType& before) {
    if (after.tv_nsec - before.tv_nsec < 0) {
      return (after.tv_sec - before.tv_sec - 1) * kNanosecondsToSeconds +
        (kNanosecondsToSeconds + after.tv_nsec - before.tv_nsec);
    } else {
      return (after.tv_sec - before.tv_sec) * kNanosecondsToSeconds +
        (after.tv_nsec - before.tv_nsec);
    }
  }

#endif

  int timeout_ms_;
  TimeType begin_time_;
  TimeType current_time_;
  uint64_t node_count_;
  int completed_depth_;
  std::mutex mutex_;
};

#endif  // TIMER_H_
