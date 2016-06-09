#ifndef TRAX_SEARCH_H_
#define TRAX_SEARCH_H_

#include <iostream>
#include <sstream>

#include "trax.h"

class NoneEvaluator {
 public:
  // Evaluate the position, from the perspective of position.red_to_move().
  // or more simply, you are red inside the method if red_to_move() == true.
  // Larger value is better.
  static int Evaluate(const Position& position) {
    return 0;
  }
};

class LeafAverageEvaluator  {
 public:
  // Evaluate the position, from the perspective of position.red_to_move().
  // or more simply, you are red inside the method if red_to_move() == true.
  // Larger value is better.
  static int Evaluate(const Position& position) {
    if (position.Hash() == 6357593829197971889ULL) {
      position.Dump();
    }
    if (position.finished()) {
      if (position.red_to_move()) {
        // I'm red.
        // winner() > 0 if red wins.
        return kInf * position.winner();
      } else {
        // I'm white.
        // winner() > 0 if red wins.
        // Flip the sign.
        return kInf * -position.winner();
      }
    }

    int64_t numerator = 0;
    int64_t denominator = 0;

    for (auto&& move : position.GenerateMoves()) {
      Position next_position;
      if (!position.DoMove(move, &next_position)) {
        // This is illegal move.
        continue;
      }
      int64_t score = 0;
      if (position.red_to_move()) {
        // I'm red.
        // winner() > 0 if red wins.
        score = kInf * next_position.winner();
      } else {
        // I'm white.
        // Flip the sign.
        score = kInf * -next_position.winner();
      }
#if 1
      if (position.Hash() == 6357593829197971889ULL) {
        std::cerr << "> " << score << " " << move.notation() << std::endl;
      }
#endif
      numerator += score;
      ++denominator;
    }

    return numerator / denominator;
  }
};

// Searcher that randomly selects any legal moves.
class RandomSearcher : public Searcher {
 public:
  virtual Move SearchBestMove(const Position& position);

  virtual std::string name() { return "RandomSearcher"; }
};

// Searcher that directly selects the best move determined by the evaluator.
template <typename Evaluator>
class SimpleSearcher : public Searcher {
 public:
  virtual Move SearchBestMove(const Position& position);

  virtual std::string name() { return "SimpleSearcher"; }
};

// Searcher that selects the best move by using the given evaluator and NegaMax
// search with Alpha-Beta pruning.
template <typename Evaluator>
class NegaMaxSearcher : public Searcher {
 public:
  NegaMaxSearcher(int max_depth) : max_depth_(max_depth) {
  }

  virtual Move SearchBestMove(const Position& position);

  virtual std::string name() {
    std::stringstream name;
    name << "NegaMaxSearcher(max_depth=" << max_depth_ << ")";
    return name.str();
  }

 private:
  int NegaMax(const Position& position,
              int depth, int alpha=-kInf, int beta=kInf);

  int max_depth_;
  std::unordered_map<PositionHash, int> transposition_table_;
  std::unordered_map<PositionHash, int> transposition_table_depth_;
};

#endif // TRAX_SEARCH_H_
