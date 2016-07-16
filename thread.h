// Copyright (C) 2016 Tetsui Ohkubo.

#ifndef THREAD_H_
#define THREAD_H_

#include "./trax.h"

class ThreadedSearcher : public Searcher {
 public:
  ThreadedSearcher() {
    // TODO(tetsui): create thread pools
  }

  virtual ~ThreadedSearcher() {
    // TODO(tetsui): destroy thread pools
  }

  virtual Move SearchBestMove(const Position& position, Timer *timer);

  virtual void DoSearchBestMove(const Position& position,
                                int thread_index,
                                int num_threads,
                                Timer* timer,
                                Move* best_move, int* best_score,
                                int* completed_depth) = 0;
};


#endif  // THREAD_H_
