// Copyright (C) 2016 Tetsui Ohkubo.

#ifndef THREAD_H_
#define THREAD_H_

#include <mutex>   // NOLINT
#include <thread>  // NOLINT
#include <vector>

#include "./trax.h"

class SearchThread {
 public:
  SearchThread()
      : is_quit_(false)
      , is_searching_(false)
      , mutex_()
      , condition_()
      , thread_index_(0)
      , num_threads_(0)
      , root_position_(nullptr)
      , best_move_()
      , best_score_(-kInf)
      , completed_depth_(0)
      , thread_(&SearchThread::Loop, this) {
  }

  ~SearchThread() {
    thread_.join();
  }

  SearchThread(SearchThread&) = delete;
  void operator=(SearchThread) = delete;

  void set_thread_index(int thread_index) {
    thread_index_ = thread_index;
  }

  void set_num_threads(int num_threads) {
    num_threads_ = num_threads;
  }

 private:
  void Loop() {
  }

  bool is_quit_;
  bool is_searching_;
  std::mutex mutex_;
  std::condition_variable condition_;

  int thread_index_;
  int num_threads_;

  Position *root_position_;
  Move best_move_;
  int best_score_;
  int completed_depth_;

  std::thread thread_;
};

class ThreadedSearcher : public Searcher {
 public:
  ThreadedSearcher();

  virtual ~ThreadedSearcher() {
  }

  virtual Move SearchBestMove(const Position& position, Timer *timer);

  virtual void DoSearchBestMove(const Position& position,
                                int thread_index,
                                int num_threads,
                                Timer* timer,
                                Move* best_move, int* best_score,
                                int* completed_depth) = 0;

 private:
  std::vector<SearchThread> threads_;
};


#endif  // THREAD_H_
