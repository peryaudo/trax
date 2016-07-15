// Copyright (C) 2016 Tetsui Ohkubo.

#ifndef SEARCH_H_
#define SEARCH_H_

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>

#include "./timer.h"
#include "./trax.h"

//
// Searchers
//

// Searcher that randomly selects any legal moves.
class RandomSearcher : public Searcher {
 public:
  virtual Move SearchBestMove(const Position& position, Timer *timer);

  virtual std::string name() { return "RandomSearcher"; }
};

// Searcher that directly selects the best move determined by the evaluator.
template <typename Evaluator>
class SimpleSearcher : public Searcher {
 public:
  virtual Move SearchBestMove(const Position& position, Timer *timer);

  virtual std::string name() {
    return "SimpleSearcher<" + Evaluator::name() + ">";
  }
};

// Transposition Table boundary flags for combination with alpha-beta pruning.
enum Bound {
  BOUND_LOWER = 0,
  BOUND_UPPER,
  BOUND_EXACT
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
  int score : 32;
  int depth : 30;
  Bound bound : 2;
};

// Searcher that selects the best move by using the given evaluator and NegaMax
// search with Alpha-Beta pruning.
template <typename Evaluator>
class NegaMaxSearcher : public Searcher {
 public:
  explicit NegaMaxSearcher(int max_depth, bool iterative = false)
      : max_depth_(max_depth)
      , current_max_depth_(0)
      , iterative_(iterative) {
    std::vector<Game> games;
    ParseCommentedGames("vendor/commented/Comment.txt", &games);
    book_.Init(games);
  }

  virtual Move SearchBestMove(const Position& position, Timer *timer);

  virtual std::string name() {
    std::stringstream name;
    name << "NegaMaxSearcher<" << Evaluator::name()
      << ">(max_depth=" << max_depth_;
    if (iterative_) {
      name << ", iterative";
    }
    name << ")";
    return name.str();
  }

 private:
  int NegaMax(const Position& position, Timer *timer,
              int depth, int alpha = -kInf, int beta = kInf);

  int max_depth_;
  int current_max_depth_;
  bool iterative_;
  std::unordered_map<PositionHash,
                     TranspositionTableEntry> transposition_table_;
  Book book_;
};

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

    int64_t numerator = 0;
    int64_t denominator = 0;

    std::vector<Move> initial_moves = initial_position.GenerateMoves();

    Timer timer(50);
    while (!timer.CheckTimeout()) {
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
        bool legal = false;
        for (int i = 0; i < moves.size(); ++i) {
          Move move = moves[Random() % moves.size()];
          if (position.DoMove(move, &next_position)) {
            // The move is legal.
            legal = true;
            break;
          }
        }

        if (!legal) {
          break;
        }

        position.Swap(&next_position);
      }

      numerator += position.winner();
      ++denominator;
    }

    if (initial_position.red_to_move()) {
      // I'm red.
      return static_cast<int>(kInf * numerator / denominator);
    } else {
      // I'm white.
      return static_cast<int>(kInf * -numerator / denominator);
    }
  }

  static std::string name() { return "MonteCarloEvaluator"; }
};

static int CalcMateScore(const Position& position,
                         const std::vector<Line>& lines) {
  int red_mates = 0;
  int white_mates = 0;
  for (const Line& line : lines) {
    if (line.is_mate()) {
      if (line.is_red) {
        ++red_mates;
      } else {
        ++white_mates;
      }
    }
  }

  if (position.red_to_move()) {
    if (red_mates > 0) {
      return kInf / 10 * 9;
    }
    if (white_mates >= 2) {
      return -kInf / 10 * 9;
    }
  } else {
    if (white_mates > 0) {
      return kInf / 10 * 9;
    }
    if (red_mates >= 2) {
      return -kInf / 10 * 9;
    }
  }

  return 0;
}

class FactorEvaluator {
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

    std::vector<Line> lines;
    position.EnumerateLines(&lines);

    const int mate_score = CalcMateScore(position, lines);
    if (mate_score != 0) {
      return mate_score;
    }

    const int unit = kInf / 100;
    int endpoint_factor = 0;
    for (Line& line : lines) {
      int endpoint = unit / (5 + line.endpoint_distance);
      if (!line.is_red) {
        endpoint *= -1;
      }
      endpoint_factor += endpoint;
    }

    int score = endpoint_factor;
    if (!position.red_to_move()) {
      score *= -1;
    }

    return score;
  }

  static std::string name() { return "FactorEvaluator"; }
};

class AdvancedFactorEvaluator {
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

    std::vector<Line> lines;
    position.EnumerateLines(&lines);

    const int mate_score = CalcMateScore(position, lines);
    if (mate_score != 0) {
      return mate_score;
    }

    const int unit = kInf / 100;
    int endpoint_factor = 0;
    int inner_count_factor = 0;
    for (Line& line : lines) {
      int endpoint = unit / (5 + line.endpoint_distance);
      int inner = line.is_inner;
      if (!line.is_red) {
        endpoint *= -1;
        inner *= -1;
      }
      endpoint_factor += endpoint;
      inner_count_factor += inner;
    }

    int score = endpoint_factor + inner_count_factor;
    if (!position.red_to_move()) {
      score *= -1;
    }

    return score;
  }

  static std::string name() { return "AdvancedFactorEvaluator"; }
};

class LoopFactorEvaluator {
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

    std::vector<Line> lines;
    position.EnumerateLines(&lines);

    const int mate_score = CalcMateScore(position, lines);
    if (mate_score != 0) {
      return mate_score;
    }

    const int unit = kInf / 100;
    int loop_factor = 0;
    for (Line& line : lines) {
      int loop = 0;

      if (line.is_inner) {
        loop += unit / (3 + line.loop_distances[0]);
        loop += unit / (3 + line.loop_distances[1]);
      } else {
        loop += unit / line.endpoint_distance;
      }
      if (!line.is_red) {
        loop *= -1;
      }
      loop_factor += loop;
    }

    int score = loop_factor;
    if (!position.red_to_move()) {
      score *= -1;
    }

    return score;
  }

  static std::string name() { return "LoopFactorEvaluator"; }
};

void GenerateFactors(const Position& position,
                     std::vector<std::pair<std::string, double>> *factors);

#endif  // SEARCH_H_
