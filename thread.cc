// Copyright (C) 2016 Tetsui Ohkubo.

#include "./thread.h"

#include <gflags/gflags.h>

#include <cstdlib>
#include <iostream>

DEFINE_int32(num_threads, 4, "Number of threads to be used for Lazy SMP.");

void SearchThread::StartSearch(const Position& position, Timer *timer) {
  {
    // Check search is not running and thread is running.

    std::lock_guard<std::mutex> lock(mutex_);
    if (is_searching_ || is_quit_) {
      std::cerr << "Search is already running or already quit" << std::endl;
      exit(EXIT_FAILURE);
    }
  }

  root_position_ = &position;
  timer_ = timer;

  // Set searching and notify condition variable

  {
    std::lock_guard<std::mutex> lock(mutex_);
    is_searching_ = true;
  }

  condition_.notify_all();
}

void SearchThread::Wait() {
  std::unique_lock<std::mutex> lock(mutex_);
  condition_.wait(lock, [=]{ return !is_searching_; });
}

void SearchThread::Loop() {
  while (!is_quit_) {
    {
      std::unique_lock<std::mutex> lock(mutex_);

      // This may help you understand that lambda:
      //   https://cpplover.blogspot.jp/2010/06/lambda_15.html
      condition_.wait(lock, [=]{ return is_searching_ || is_quit_; });
    }

    if (is_searching_ && !is_quit_) {
      // Perform actual search.
      searcher_->DoSearchBestMove(
          *root_position_, thread_index_, num_threads_, timer_,
          &best_move_, &best_score_, &completed_depth_);
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      is_searching_ = false;
    }
    condition_.notify_all();
  }
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

  // Select the thread with the best search depth and the best score.
  int best_index = 0;
  for (int i = 0; i < threads_.size(); ++i) {
    if (threads_[i].completed_depth() > threads_[best_index].completed_depth()
        && threads_[i].best_score() > threads_[best_index].best_score()) {
      best_index = i;
    }
  }

  // Return the best move of that thread.
  return threads_[best_index].best_move();
}
