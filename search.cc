// Copyright (C) 2016 Tetsui Ohkubo.

#include "./search.h"

#include <gflags/gflags.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>

#include "./trax.h"


DEFINE_bool(enable_transposition_table, true, "Enable Transposition Table.");


int g_num_monte_carlo_trial = 100;


Move RandomSearcher::SearchBestMove(const Position& position) {
  std::vector<Move> legal_moves;
  for (Move move : position.GenerateMoves()) {
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

  for (Move move : position.GenerateMoves()) {
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
#endif
    best_score = std::max(best_score, score);
    moves.emplace_back(score, move);
  }

  std::vector<Move> best_moves;
  for (ScoredMove& move : moves) {
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

  for (Move move : position.GenerateMoves()) {
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
#if 0
    std::cerr << score << " " << move.notation() << std::endl;
#endif
  }

  std::vector<Move> best_moves;
  for (ScoredMove& move : moves) {
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

  PositionHash hash_b;

  if (FLAGS_enable_transposition_table) {
    // At that point of time, we don't care about conflicts.
    const PositionHash hash_a = position.Hash(kPositionHashPrimeA);

    // But at least it uses two hashes to decrese likelihood of hash conflict.
    hash_b = position.Hash(kPositionHashPrimeB);

    auto it = transposition_table_.find(hash_a);
    if (it != transposition_table_.end()) {
      entry = &it->second;
    }

    if (entry != nullptr && entry->hash_b == hash_b && entry->depth >= depth) {
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
      entry = &transposition_table_[hash_a];
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
    for (Move move : position.GenerateMoves()) {
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

    entry->hash_b = hash_b;
  }

  return best_score;
}

// Instantiation.
template Move SimpleSearcher<LeafAverageEvaluator>::SearchBestMove(
    const Position& position);

template Move SimpleSearcher<MonteCarloEvaluator>::SearchBestMove(
    const Position& position);

template Move SimpleSearcher<EdgeColorEvaluator>::SearchBestMove(
    const Position& position);

template Move SimpleSearcher<LongestLineEvaluator>::SearchBestMove(
    const Position& position);

template Move SimpleSearcher<CombinedEvaluator>::SearchBestMove(
    const Position& position);

template Move NegaMaxSearcher<LeafAverageEvaluator>::SearchBestMove(
    const Position& position);
template int NegaMaxSearcher<LeafAverageEvaluator>::NegaMax(
    const Position& position, int depth, int alpha, int beta);

template Move NegaMaxSearcher<MonteCarloEvaluator>::SearchBestMove(
    const Position& position);
template int NegaMaxSearcher<MonteCarloEvaluator>::NegaMax(
    const Position& position, int depth, int alpha, int beta);

template Move NegaMaxSearcher<EdgeColorEvaluator>::SearchBestMove(
    const Position& position);
template int NegaMaxSearcher<EdgeColorEvaluator>::NegaMax(
    const Position& position, int depth, int alpha, int beta);

template Move NegaMaxSearcher<LongestLineEvaluator>::SearchBestMove(
    const Position& position);
template int NegaMaxSearcher<LongestLineEvaluator>::NegaMax(
    const Position& position, int depth, int alpha, int beta);

template Move NegaMaxSearcher<CombinedEvaluator>::SearchBestMove(
    const Position& position);
template int NegaMaxSearcher<CombinedEvaluator>::NegaMax(
    const Position& position, int depth, int alpha, int beta);
