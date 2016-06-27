// Copyright (C) 2016 Tetsui Ohkubo.

#ifndef TIMER_H_
#define TIMER_H_

#include <cstdint>

#include <unistd.h>

#if defined __MACH__
#include <mach/mach_time.h>
#elif defined _POSIX_MONOTONIC_CLOCK
#include <time.h>
#else
#error no way to get accurate time.
#endif

static const uint64_t kNanosecondsToMilliseconds = 1000000;
static const uint64_t kNanosecondsToSeconds = 1000000000;

namespace {

#if defined __MACH__

using TimeType = uint64_t;

void GetAccurateCurrentTime(TimeType *current_time) {
  mach_timebase_info_data_t base;
  mach_timebase_info(&base);
  *current_time = mach_absolute_time() / base.denom;
}

uint64_t DiffAccurateTime(const TimeType& after, const TimeType& before) {
  return after - before;
}

#elif defined _POSIX_MONOTONIC_CLOCK

using TimeType = struct timespec;

void GetAccurateCurrentTime(TimeType *current_time) {
  clock_gettime(CLOCK_MONOTONIC, current_time);
}

uint64_t DiffAccurateTime(const TimeType& after, const TimeType& before) {
  if (after.tv_nsec - before.tv_nsec < 0) {
    return (after.tv_sec - before.tv_sec - 1) * kNanosecondsToSeconds +
      (kNanosecondsToSeconds + after.tv_nsec - before.tv_nsec);
  } else {
    return (after.tv_sec - before.tv_sec) * kNanosecondsToSeconds +
      (after.tv_nsec - before.tv_nsec);
  }
}

#endif
 
}  // namespace

class Timer {
 public:
  // Check() will always return false (the timer will never expire)
  // by setting timeout_ms negative.
  // The timer will start right after the constructor is called.
  explicit Timer(int timeout_ms = -1)
      : timeout_ms_(timeout_ms)
      , node_count_(0) {
    GetAccurateCurrentTime(&begin_time_);
    GetAccurateCurrentTime(&current_time_);
  }

  // Return true if the timer is expired.
  bool CheckTimeout() {
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

 private:
  int timeout_ms_;
  TimeType begin_time_;
  TimeType current_time_;
  uint64_t node_count_;
};

#endif  // TIMER_H_