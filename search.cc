#include "search.h"

#include <iostream>
#include <cstdlib>

#include "gflags/gflags.h"


// For NegaMax(depth=2), it has impact of 40secs -> 20secs
// for 200 times NegaMax-Random self play.
DEFINE_bool(enable_transposition_table, false, "Enable Transposition Table.");


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

    const int score = Evaluator::Evaluate(next_position);
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

    const int score = NegaMax(next_position, max_depth_);
    // This assert should always hold, but actually transposition table
    // conflict can lead this condition to fail.
    assert(score == LeafAverageEvaluator::Evaluate(next_position));

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

template<typename Evaluator>
int NegaMaxSearcher<Evaluator>::NegaMax(
    const Position& position, int depth, int alpha, int beta) {
  PositionHash hash;
  if (FLAGS_enable_transposition_table) {
    hash = position.Hash();

    // Conflict could happen but we don't care.
    if (transposition_table_.count(hash)) {
      return transposition_table_[hash];
    }
  }

  assert(depth >= 0);

  int max_score = -kInf;

  if (position.finished() || depth == 0) {
    max_score = Evaluator::Evaluate(position);
  } else {
    for (auto&& move : position.GenerateMoves()) {
      Position next_position;
      if (!position.DoMove(move, &next_position)) {
        // This is illegal move.
        continue;
      }

      const int score = -NegaMax(next_position, depth - 1, -beta, -alpha);
      max_score = std::max(max_score, score);
      alpha = std::max(alpha, score);
      if (alpha >= beta) {
        break;
      }
    }
  }

  if (FLAGS_enable_transposition_table) {
    return transposition_table_[hash] = max_score;
  } else {
    return max_score;
  }
}

// Instantiation.
template Move SimpleSearcher<LeafAverageEvaluator>::SearchBestMove(
    const Position& position);

template Move NegaMaxSearcher<LeafAverageEvaluator>::SearchBestMove(
    const Position& position);

template int NegaMaxSearcher<LeafAverageEvaluator>::NegaMax(
    const Position& position, int depth, int alpha, int beta);
