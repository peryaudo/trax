#include "search.h"

#include <iostream>
#include <cstdlib>

#include "gflags/gflags.h"
#include "trax.h"


DEFINE_bool(enable_transposition_table, false, "Enable Transposition Table.");
DEFINE_bool(debug_transposition_table, false, "Debug Transposition Table.");


Move RandomSearcher::SearchBestMove(const Position& position) {
  std::vector<Move> legal_moves;
  for (auto&& move : position.GenerateMoves()) {
    Position next_position;
    if (position.DoMove(move, &next_position)) {
      // The move is proved to be legal.
      legal_moves.push_back(move);
    }
  }
  assert(legal_moves.size() > 0);
  return legal_moves[Random() % legal_moves.size()];
}

// Return the best move from the perspective of position.red_to_move().
template<typename Evaluator>
Move SimpleSearcher<Evaluator>::SearchBestMove(const Position& position) {
  assert(!position.finished());

  int best_score = -kInf;
  std::vector<ScoredMove> moves;

  for (auto&& move : position.GenerateMoves()) {
    Position next_position;
    if (!position.DoMove(move, &next_position)) {
      // This is illegal move.
      continue;
    }

    // next_position.red_to_move() == !position.red_to_move() holds.
    // Evaluate() evaluates from the perspective of next_position.
    // Therefore, position that is good for next_position.red_to_move() is
    // bad for position.red_to_move().
    const int score = -Evaluator::Evaluate(next_position);
#if 0
    std::cerr << score << " " << move.notation() << std::endl;
    std::cerr << std::endl;
#endif
    best_score = std::max(best_score, score);
    moves.emplace_back(score, move);
  }

  std::vector<Move> best_moves;
  for (auto&& move : moves) {
    if (move.score == best_score) {
      best_moves.push_back(move);
    }
  }

  assert(best_moves.size() > 0);
  return best_moves[Random() % best_moves.size()];
}

// Return the best move from the perspective of position.red_to_move().
template<typename Evaluator>
Move NegaMaxSearcher<Evaluator>::SearchBestMove(const Position& position) {
  assert(!position.finished());

  int best_score = -kInf;
  std::vector<ScoredMove> moves;

  for (auto&& move : position.GenerateMoves()) {
    Position next_position;
    if (!position.DoMove(move, &next_position)) {
      // This is illegal move.
      continue;
    }

    // next_position.red_to_move() == !position.red_to_move() holds.
    // NegaMax() evaluates from the perspective of next_position.
    // Therefore, position that is good for next_position.red_to_move() is
    // bad for position.red_to_move().
    const int score = -NegaMax(next_position, 0);

    best_score = std::max(best_score, score);
    moves.emplace_back(score, move);
  }

  std::vector<Move> best_moves;
  for (auto&& move : moves) {
    if (move.score == best_score) {
      best_moves.push_back(move);
    }
  }

  assert(best_moves.size() > 0);
  return best_moves[Random() % best_moves.size()];
}

// Score the move from the perspective of position.red_to_move().
// Larger is better.
template<typename Evaluator>
int NegaMaxSearcher<Evaluator>::NegaMax(
    const Position& position, int depth, int alpha, int beta) {
  if (position.Hash() == 12455711137682810522ULL) {
    std::cerr << " === BEGIN === " << std::endl;
  }
  if (FLAGS_enable_transposition_table && !FLAGS_debug_transposition_table) {
    // Conflict could happen but we don't care.
    if (transposition_table_.count(position.Hash()) &&
        transposition_table_depth_[position.Hash()] >= depth) {
      return transposition_table_[position.Hash()];
    }
  }

  assert(depth <= max_depth_);

  int best_score = -kInf;

  if (position.finished() || depth >= max_depth_) {
    // Evaluate the position, from the perspective of position.red_to_move(),
    // and this is same as NegaMax().
    // Thus, there is no need for sign flip.
    best_score = Evaluator::Evaluate(position);

    if (position.Hash() == 12455711137682810522ULL) {
      std::cerr << "Evaluate(position) = " << best_score << std::endl;
    }
  } else {
    for (auto&& move : position.GenerateMoves()) {
      Position next_position;
      if (!position.DoMove(move, &next_position)) {
        // This is illegal move.
        continue;
      }

      // next_position.red_to_move() == !position.red_to_move() holds.
      // NegaMax() evaluates from the perspective of next_position.
      // Therefore, position that is good for next_position.red_to_move() is
      // bad for position.red_to_move().
      const int score = -NegaMax(next_position, depth + 1, -beta, -alpha);
      best_score = std::max(best_score, score);
      alpha = std::max(alpha, score);
      // if (alpha >= beta) {
      //  break;
      // }
    }
  }

  if (FLAGS_enable_transposition_table) {
    if (FLAGS_debug_transposition_table &&
        transposition_table_.count(position.Hash()) &&
        transposition_table_depth_[position.Hash()] == depth &&
        transposition_table_[position.Hash()] != best_score) {
      std::cerr << "!!!! Transposition table broken !!!!" << std::endl;
      std::cerr << "Hash: " << position.Hash() << std::endl;
      std::cerr << "Depth: " << depth << std::endl;
      std::cerr << "Score in TT: " << transposition_table_[position.Hash()]
        << std::endl;
      std::cerr << "Score: " << best_score << std::endl;
      // std::cerr << "Before: " << std::endl;
      // before_position_[position.Hash()].Dump();
      // std::cerr << "After: " << std::endl;
      position.Dump();
      exit(EXIT_FAILURE);
    }

    transposition_table_[position.Hash()] = best_score;
    transposition_table_depth_[position.Hash()] = depth;
  }

  if (position.Hash() == 12455711137682810522ULL) {
    std::cerr << " === END === " << std::endl;
  }
  return best_score;
}

// Instantiation.
template Move SimpleSearcher<LeafAverageEvaluator>::SearchBestMove(
    const Position& position);

template Move NegaMaxSearcher<LeafAverageEvaluator>::SearchBestMove(
    const Position& position);

template int NegaMaxSearcher<LeafAverageEvaluator>::NegaMax(
    const Position& position, int depth, int alpha, int beta);
