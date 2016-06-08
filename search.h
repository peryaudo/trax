#ifndef TRAX_SEARCH_H_
#define TRAX_SEARCH_H_

#include "trax.h"

class Evaluator {
 public:
  virtual int Evaluate(const Position& position) = 0;
};

class RandomSearcher : public Searcher {
 public:
  virtual Move SearchBestMove(const Position& position);
};

class NegaMaxSearcher : public Searcher {
 public:
  // depth=2: white: 146 red: 54 73%
  // depth=3: white: 153 red: 47 76.5%
  NegaMaxSearcher(int max_depth) : max_depth_(max_depth) {
  }

  virtual Move SearchBestMove(const Position& position);

 private:
  int NegaMax(const Position& position,
              int depth, int alpha=-kInf, int beta=kInf);

  int max_depth_;
  std::unordered_map<PositionHash, int> transposition_table_;
};

#endif // TRAX_SEARCH_H_
