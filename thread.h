// Copyright (C) 2016 Tetsui Ohkubo.

#ifndef THREAD_H_
#define THREAD_H_

#include <mutex>   // NOLINT
#include <thread>  // NOLINT
#include <condition_variable>  // NOLINT
#include <vector>

#include "./trax.h"

class ThreadedSearcher;

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
      , timer_(nullptr)
      , best_move_()
      , best_score_(-kInf)
      , completed_depth_(0)
      , searcher_(nullptr)
      , thread_(&SearchThread::Loop, this) {
  }

  ~SearchThread() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      is_quit_ = true;
      is_searching_ = false;
    }
    condition_.notify_all();
    thread_.join();
  }

  SearchThread(SearchThread&) = delete;
  void operator=(SearchThread) = delete;

  void StartSearch(const Position& position, Timer *timer);

  void Wait();

  Move best_move() const {
    return best_move_;
  }

  int best_score() const {
    return best_score_;
  }

  int completed_depth() const {
    return completed_depth_;
  }

  void set_thread_index(int thread_index) {
    thread_index_ = thread_index;
  }

  void set_num_threads(int num_threads) {
    num_threads_ = num_threads;
  }

  void set_searcher(ThreadedSearcher *searcher) {
    searcher_ = searcher;
  }

 private:
  void Loop();

  bool is_quit_;
  bool is_searching_;
  std::mutex mutex_;
  std::condition_variable condition_;

  int thread_index_;
  int num_threads_;

  const Position *root_position_;
  Timer *timer_;
  Move best_move_;
  int best_score_;
  int completed_depth_;
  ThreadedSearcher *searcher_;

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
