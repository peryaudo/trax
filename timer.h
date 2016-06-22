// Copyright (C) 2016 Tetsui Ohkubo.

#ifndef TIMER_H_
#define TIMER_H_

#ifdef __MACH__
#include <mach/mach_time.h>
#endif

const static uint64_t kNanosecondsToMilliseconds = 1000000;
const static uint64_t kNanosecondsToSeconds = 1000000000;

class Timer {
 public:
  // Check() will always return false (the timer will never expire)
  // by setting timeout_ms negative.
  // The timer will start right after the constructor is called.
  Timer(int timeout_ms = -1)
      : timeout_ms_(timeout_ms)
      , begin_time_(0)
      , check_count_(0)
      , check_count_target_(0)
      , node_count_(0) {
#ifdef __MACH__
    mach_timebase_info_data_t base;
    mach_timebase_info(&base);
    begin_time_ = mach_absolute_time() / base.denom;
#endif

    current_time_ = begin_time_;
  }

  // TODO(peryaudo): ForceCheckTimeout? or (bool force = false) ?

  // Return true if the timer is expired.
  // For first several calls it always checks the realtime clock and after that
  // it tries to reduce call to once in 100ms to eliminate context switching
  // overhead.
  bool CheckTimeout() {
    if (current_time_ - begin_time_ >
        static_cast<uint64_t>(timeout_ms_) * kNanosecondsToMilliseconds) {
      return true;
    }

    ++check_count_;

    if (check_count_ > 10 && check_count_ < check_count_target_) {
      return false;
    }

#ifdef __MACH__
    mach_timebase_info_data_t base;
    mach_timebase_info(&base);
    current_time_ = mach_absolute_time() / base.denom;
#endif

    check_count_target_ = (check_count_ +
                           check_count_ * kNanosecondsToMilliseconds * 50 /
                           (current_time_ - begin_time_));

    if (timeout_ms_ < 0) {
      return false;
    }

    if (current_time_ - begin_time_ >
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
    if (current_time_ == begin_time_) {
      return 0;
    }

    return node_count_ * kNanosecondsToSeconds / (current_time_ - begin_time_);
  }

  // Return elapsed milliseconds since the Timer constructor is called.
  int elapsed_ms() {
    return (current_time_ - begin_time_) / kNanosecondsToMilliseconds;
  }

 private:
  int timeout_ms_;
  uint64_t begin_time_;
  uint64_t current_time_;
  uint64_t check_count_;
  uint64_t check_count_target_;
  uint64_t node_count_;
};

#endif  // TIMER_H_
