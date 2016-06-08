#ifndef TRAX_SEARCH_H_
#define TRAX_SEARCH_H_

#include <sstream>

#include "trax.h"

// If the position is finished, it returns positive or negative infinite score
// depending on the side to move. Otherwise, it returns zero.
// TODO(tetsui): Write test for ScoreFinishedPosition()
// TODO(testui): Is the sign correct?
inline static int ScoreFinishedPosition(const Position& position) {
  if (position.finished()) {
    // Think the case where position.red_to_move() == true in SearchBestMove.
    // red_to_move() == false in NegaMax.
    // NegaMax should return positive score for winner() == 1.
    if (position.red_to_move()) {
      return kInf * -position.winner();
    } else {
      return kInf * position.winner();
    }
  } else {
    return 0;
  }
}

class NoneEvaluator {
 public:
  static int Evaluate(const Position& position) {
    return 0;
  }
};

class LeafAverageEvaluator  {
 public:
  static int Evaluate(const Position& position) {
    if (position.finished()) {
      return ScoreFinishedPosition(position);
    }

    int64_t numerator = 0;
    int64_t denominator = 0;

    for (auto&& move : position.GenerateMoves()) {
      Position next_position;
      if (!position.DoMove(move, &next_position)) {
        // This is illegal move.
        continue;
      }
      numerator -= ScoreFinishedPosition(next_position);
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
class NegaMaxSearcher : public Searcher {
 public:
  // depth=2: white: 146 red: 54 73%
  // depth=3: white: 153 red: 47 76.5%
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
};

#endif // TRAX_SEARCH_H_
