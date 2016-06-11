// Copyright (C) 2016 Tetsui Ohkubo.

#ifndef SEARCH_H_
#define SEARCH_H_

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>

#include "./trax.h"

//
// Evaluators
//

// Evaluator that only returns zero except for finished positions.
class NoneEvaluator {
 public:
  // Evaluate the position, from the perspective of position.red_to_move().
  // or more simply, you are red inside the method if red_to_move() == true.
  // Larger value is better.
  static int Evaluate(const Position& position) {
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

  static std::string name() { return "NoneEvaluator"; }
};

// Evaluator that averages the leaves score of NoneEvaluator.
class LeafAverageEvaluator  {
 public:
  // Evaluate the position, from the perspective of position.red_to_move().
  // or more simply, you are red inside the method if red_to_move() == true.
  // Larger value is better.
  static int Evaluate(const Position& position) {
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

    for (Move move : position.GenerateMoves()) {
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
#if 0
      if (position.Hash() == /* set board hash you want to debug here */) {
        std::cerr << "> " << score << " " << move.notation() << std::endl;
      }
#endif
      numerator += score;
      ++denominator;
    }

    return numerator / denominator;
  }

  static std::string name() { return "LeafAverageEvaluator"; }
};

extern int g_num_monte_carlo_trial;

// Evaluator that uses primitive Monte Carlo method to evaluate the position.
// You can change the number of time to sample by --num_monte_carlo_trial.
class MonteCarloEvaluator {
 public:
  static int Evaluate(const Position& initial_position) {
    if (initial_position.finished()) {
      if (initial_position.red_to_move()) {
        // I'm red.
        // winner() > 0 if red wins.
        return kInf * initial_position.winner();
      } else {
        // I'm white.
        // winner() > 0 if red wins.
        // Flip the sign.
        return kInf * -initial_position.winner();
      }
    }

    int winner_sum = 0;

    std::vector<Move> initial_moves = initial_position.GenerateMoves();

    for (int i = 0; i < g_num_monte_carlo_trial; ++i) {
      Position position;

      // First step.
      Move initial_move = initial_moves[Random() % initial_moves.size()];
      if (!initial_position.DoMove(initial_move, &position)) {
        // The move is illegal.
        continue;
      }

      while (!position.finished()) {
        Position next_position;

        std::vector<Move> moves = position.GenerateMoves();
        Move move = moves[Random() % moves.size()];
        position.DoMove(move, &next_position);
        position.Swap(&next_position);
      }

      winner_sum += position.winner();
    }

    if (initial_position.red_to_move()) {
      // I'm red.
      return kInf * winner_sum / g_num_monte_carlo_trial;
    } else {
      // I'm white.
      return kInf * -winner_sum / g_num_monte_carlo_trial;
    }
  }

  static std::string name() { return "MonteCarloEvaluator"; }
};

// Evaluate position by number of external facing color of the player.
// NegaMax<EdgeColorEvaluator>(depth=2)
class EdgeColorEvaluator {
 public:
  static int Evaluate(const Position& position) {
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

    int red = 0;
    int white = 0;

    for (int i_x = 0; i_x < position.max_x(); ++i_x) {
      for (int j_y = 0; j_y < position.max_y(); ++j_y) {
        if (position.at(i_x, j_y) == PIECE_EMPTY) {
          continue;
        }

        for (int k = 0; k < 4; ++k) {
          const int nx = i_x + kDx[k];
          const int ny = j_y + kDy[k];
          if (position.at(nx, ny) != PIECE_EMPTY) {
            continue;
          }

          if (kPieceColors[position.at(i_x, j_y)][k] == 'R') {
            ++red;
          } else {
            ++white;
          }
        }
      }
    }

    if (red + white == 0) {
      return 0;
    }

    // --self --white=negamax1-ec --red=negamax1-la --silent --num_games=20

    // white(NegaMaxSearcher<EdgeColorEvaluator>(max_depth=1)): 2
    // red(NegaMaxSearcher<LeafAverageEvaluator>(max_depth=1)): 17
    const int score = red - white;

    // white(NegaMaxSearcher<EdgeColorEvaluator>(max_depth=1)): 1
    // red(NegaMaxSearcher<LeafAverageEvaluator>(max_depth=1)): 14
    // const int score = kInf * (red - white) / (red + white);

    if (position.red_to_move()) {
      return score;
    } else {
      return -score;
    }
  }

  static std::string name() { return "EdgeColorEvaluator"; }
};

// Evaluate position by its longest line of the color.
class LongestLineEvaluator {
 public:
  static int Evaluate(const Position& position) {
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

    const int score = position.red_longest() - position.white_longest();
    if (position.red_to_move()) {
      return score;
    } else {
      return -score;
    }
  }

  static std::string name() { return "LongestLineEvaluator"; }
};

class CombinedEvaluator {
 public:
  static int Evaluate(const Position& position) {
    int score = LeafAverageEvaluator::Evaluate(position);
    if (score == kInf || score == -kInf)
      return score;

    int denom = 1;

    if (position.max_x() <= 2 || position.max_y() <= 2) {
      score += EdgeColorEvaluator::Evaluate(position);
      ++denom;
    }

    if ((5 <= position.max_x() && position.max_x() <= 7) &&
        (5 <= position.max_y() && position.max_y() <= 7)) {
      score += LongestLineEvaluator::Evaluate(position);
      ++denom;
    }

    return score / denom;
  }

  static std::string name() { return "CombinedEvaluator"; }
};

//
// Searchers
//

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

  virtual std::string name() {
    return "SimpleSearcher<" + Evaluator::name() + ">";
  }
};

// Transposition Table boundary flags for combination with alpha-beta pruning.
enum TranspositionTableBound {
  TRANSPOSITION_TABLE_LOWER_BOUND,
  TRANSPOSITION_TABLE_UPPER_BOUND,
  TRANSPOSITION_TABLE_EXACT
};

// Entry of Transposition Table.
//
// See also:
//
// https://en.wikipedia.org/wiki/Negamax
// #Negamax_with_alpha_beta_pruning_and_transposition_tables
//
// https://groups.google.com/forum/#!msg/
// rec.games.chess.computer/p8GbiiLjp0o/81vZ3czsthIJ
struct TranspositionTableEntry {
  int score;
  int depth;
  TranspositionTableBound bound;
  PositionHash hash_b;
};

// Searcher that selects the best move by using the given evaluator and NegaMax
// search with Alpha-Beta pruning.
template <typename Evaluator>
class NegaMaxSearcher : public Searcher {
 public:
  explicit NegaMaxSearcher(int max_depth) : max_depth_(max_depth) {
  }

  virtual Move SearchBestMove(const Position& position);

  virtual std::string name() {
    std::stringstream name;
    name << "NegaMaxSearcher<" << Evaluator::name()
      << ">(max_depth=" << max_depth_ << ")";
    return name.str();
  }

 private:
  int NegaMax(const Position& position,
              int depth, int alpha = -kInf, int beta = kInf);

  int max_depth_;
  std::unordered_map<PositionHash,
                     TranspositionTableEntry> transposition_table_;
};

#endif  // SEARCH_H_
