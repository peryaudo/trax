#include "search.h"

#include <iostream>
#include <cstdlib>

#include "gflags/gflags.h"
#include "trax.h"


DEFINE_bool(enable_transposition_table, true, "Enable Transposition Table.");


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
  const int original_alpha = alpha;

  TranspositionTableEntry* entry = nullptr;

  if (FLAGS_enable_transposition_table) {
    // At that point of time, we don't care about conflicts.
    const PositionHash hash = position.Hash();

    auto it = transposition_table_.find(hash);
    if (it != transposition_table_.end()) {
      entry = &it->second;
    }

    if (entry != nullptr && entry->depth >= depth) {
      if (entry->bound == TRANSPOSITION_TABLE_EXACT) {
        return entry->score;
      } else if (entry->bound == TRANSPOSITION_TABLE_LOWER_BOUND) {
        alpha = std::max(alpha, entry->score);
      } else if (entry->bound == TRANSPOSITION_TABLE_UPPER_BOUND) {
        beta = std::min(beta, entry->score);
      }

      if (alpha >= beta) {
        return entry->score;
      }
    }

    if (entry == nullptr) {
      entry = &transposition_table_[hash];
    }
  }

  assert(depth <= max_depth_);

  int best_score = -kInf;

  if (position.finished() || depth >= max_depth_) {
    // Evaluate the position, from the perspective of position.red_to_move(),
    // and this is same as NegaMax().
    // Thus, there is no need for sign flip.
    best_score = Evaluator::Evaluate(position);

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
      if (alpha >= beta) {
       break;
      }
    }
  }

  if (FLAGS_enable_transposition_table) {
    assert(entry != nullptr);

    entry->score = best_score;
    entry->depth = depth;
    if (best_score <= original_alpha) {
      entry->bound = TRANSPOSITION_TABLE_UPPER_BOUND;
    } else if (best_score >= beta) {
      entry->bound = TRANSPOSITION_TABLE_LOWER_BOUND;
    } else {
      entry->bound = TRANSPOSITION_TABLE_EXACT;
    }
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
