// Copyright (C) 2016 Tetsui Ohkubo.

#include "./thread.h"

#include <gflags/gflags.h>

DEFINE_int32(num_threads,
             4,
             "Number of threads to be used for Lazy SMP.");

ThreadedSearcher::ThreadedSearcher() : threads_(FLAGS_num_threads) {
  for (int i = 0; i < threads_.size(); ++i) {
    threads_[i].set_thread_index(i);
    threads_[i].set_num_threads(threads_.size());
  }
}
