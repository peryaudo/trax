// Copyright (C) 2016 Tetsui Ohkubo.

#include "./thread.h"

#include <gflags/gflags.h>

DEFINE_int32(num_threads,
             4,
             "Number of threads to be used for Lazy SMP.");

void SearchThread::StartSearch(const Position& position, Timer *timer) {
  // TODO(tetsui): check search is not running
  // TODO(tetsui): check thread is running
  root_position_ = &position;
  timer_ = timer;
  // TODO(tetsui): set searching and notify condition variable
}

ThreadedSearcher::ThreadedSearcher() : threads_(FLAGS_num_threads) {
  for (int i = 0; i < threads_.size(); ++i) {
    threads_[i].set_searcher(this);
    threads_[i].set_thread_index(i);
    threads_[i].set_num_threads(threads_.size());
  }
}

Move ThreadedSearcher::SearchBestMove(const Position& position, Timer *timer) {
  for (SearchThread& thread : threads_) {
    thread.StartSearch(position, timer);
  }

  for (SearchThread& thread : threads_) {
    // Threads are expected to stop within the time limit using the timer.
    thread.Wait();
  }

  int best_index = 0;

  for (int i = 0; i < threads_.size(); ++i) {
    if (threads_[i].completed_depth() > threads_[best_index].completed_depth()
        && threads_[i].best_score() > threads_[best_index].best_score()) {
      best_index = i;
    }
  }

  return threads_[best_index].best_move();
}
