// Copyright (C) 2016 Tetsui Ohkubo.

#include "./search.h"

#include <gflags/gflags.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <numeric>

#include "./timer.h"
#include "./trax.h"


Move RandomSearcher::SearchBestMove(const Position& position, Timer* timer) {
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
Move SimpleSearcher<Evaluator>::SearchBestMove(const Position& position,
                                               Timer *timer) {
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
Move NegaMaxSearcher<Evaluator>::SearchBestMove(const Position& position,
                                                Timer* timer) {
  assert(!position.finished());

  Move book_move;
  if (book_.Select(position, &book_move)) {
    return book_move;
  }

  if (iterative_) {
    std::vector<Move> possible_moves = position.GenerateMoves();

    Move best_move;
    for (current_max_depth_ = 0;
         current_max_depth_ <= max_depth_; ++current_max_depth_) {
      transposition_table_.clear();
      int best_score = -kInf;
      std::vector<ScoredMove> moves;

      bool aborted = false;

      for (Move move : possible_moves) {
        Position next_position;
        if (!position.DoMove(move, &next_position)) {
          // This is illegal move.
          continue;
        }

        // next_position.red_to_move() == !position.red_to_move() holds.
        // NegaMax() evaluates from the perspective of next_position.
        // Therefore, position that is good for next_position.red_to_move() is
        // bad for position.red_to_move().
        const int score = -NegaMax(next_position, timer, 0);

        best_score = std::max(best_score, score);
        moves.emplace_back(score, move);

        if (current_max_depth_ > 0 && timer->CheckTimeout()) {
          aborted = true;
          break;
        }
      }

      if (aborted) {
        // Drop the result of that iteration.
        break;
      }

      std::vector<Move> best_moves;
      for (ScoredMove& move : moves) {
        if (move.score == best_score) {
          best_moves.push_back(move);
        }
      }

      assert(best_moves.size() > 0);
      best_move = best_moves[Random() % best_moves.size()];

      timer->set_completed_depth(current_max_depth_);
    }

    return best_move;
  } else {
    current_max_depth_ = max_depth_;

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
      const int score = -NegaMax(next_position, timer, 0);

      best_score = std::max(best_score, score);
      moves.emplace_back(score, move);
    }

    std::vector<Move> best_moves;
    for (ScoredMove& move : moves) {
      if (move.score == best_score) {
        best_moves.push_back(move);
      }
    }

    timer->set_completed_depth(current_max_depth_);

    assert(best_moves.size() > 0);
    return best_moves[Random() % best_moves.size()];
  }
}

// Score the move from the perspective of position.red_to_move().
// Larger is better.
template<typename Evaluator>
int NegaMaxSearcher<Evaluator>::NegaMax(
    const Position& position, Timer* timer, int depth, int alpha, int beta) {
  const int original_alpha = alpha;

  TranspositionTableEntry* entry = nullptr;

  // At that point of time, we don't care about conflicts.
  const PositionHash hash = position.Hash();

  auto it = transposition_table_.find(hash);
  if (it != transposition_table_.end()) {
    entry = &it->second;
  }

  if (entry != nullptr && entry->depth >= depth) {
    if (entry->bound == BOUND_EXACT) {
      return entry->score;
    } else if (entry->bound == BOUND_LOWER) {
      alpha = std::max(alpha, entry->score);
    } else if (entry->bound == BOUND_UPPER) {
      beta = std::min(beta, entry->score);
    }

    if (alpha >= beta) {
      return entry->score;
    }
  }

  if (entry == nullptr) {
    entry = &transposition_table_[hash];
  }

  assert(depth <= max_depth_);

  int best_score = -kInf;

  if (position.finished() || depth >= current_max_depth_) {
    // Evaluate the position, from the perspective of position.red_to_move(),
    // and this is same as NegaMax().
    // Thus, there is no need for sign flip.
    best_score = Evaluator::Evaluate(position);

    timer->IncrementNodeCounter();

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
      const int score = -NegaMax(
          next_position, timer, depth + 1, -beta, -alpha);
      best_score = std::max(best_score, score);
      alpha = std::max(alpha, score);
      if (alpha >= beta) {
       break;
      }

      if (timer->CheckTimeout()) {
        return 0;
      }
    }
  }

  assert(entry != nullptr);

  entry->score = best_score;
  entry->depth = depth;
  if (best_score <= original_alpha) {
    entry->bound = BOUND_UPPER;
  } else if (best_score >= beta) {
    entry->bound = BOUND_LOWER;
  } else {
    entry->bound = BOUND_EXACT;
  }

  return best_score;
}

// Instantiation.
template Move SimpleSearcher<LeafAverageEvaluator>::SearchBestMove(
    const Position& position, Timer* timer);

template Move SimpleSearcher<MonteCarloEvaluator>::SearchBestMove(
    const Position& position, Timer* timer);

template Move SimpleSearcher<FactorEvaluator>::SearchBestMove(
    const Position& position, Timer* timer);

template Move NegaMaxSearcher<NoneEvaluator>::SearchBestMove(
    const Position& position, Timer* timer);
template int NegaMaxSearcher<NoneEvaluator>::NegaMax(
    const Position& position, Timer* timer, int depth, int alpha, int beta);

template Move NegaMaxSearcher<LeafAverageEvaluator>::SearchBestMove(
    const Position& position, Timer* timer);
template int NegaMaxSearcher<LeafAverageEvaluator>::NegaMax(
    const Position& position, Timer* timer, int depth, int alpha, int beta);

template Move NegaMaxSearcher<MonteCarloEvaluator>::SearchBestMove(
    const Position& position, Timer* timer);
template int NegaMaxSearcher<MonteCarloEvaluator>::NegaMax(
    const Position& position, Timer* timer, int depth, int alpha, int beta);

template Move NegaMaxSearcher<FactorEvaluator>::SearchBestMove(
    const Position& position, Timer* timer);
template int NegaMaxSearcher<FactorEvaluator>::NegaMax(
    const Position& position, Timer* timer, int depth, int alpha, int beta);

namespace {

int CountEdgeColors(const Position& position) {
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

  return red - white;
}

double AverageColor(const std::vector<double>& v) {
  double red_numerator = 0.0;
  double white_numerator = 0.0;
  int red_denominator = 0;
  int white_denominator = 0;
  for (double x : v) {
    if (x > 0) {
      red_numerator += x;
      ++red_denominator;
    } else {
      white_numerator += x;
      ++white_denominator;
    }
  }
  if (red_denominator > 0) {
    red_numerator /= red_denominator;
  }
  if (white_denominator > 0) {
    white_numerator /= white_denominator;
  }
  return red_numerator + white_numerator;
}

}  // namespace

void GenerateFactors(const Position& position,
                     std::vector<std::pair<std::string, double>> *factors) {
  double leaf_average = LeafAverageEvaluator::Evaluate(position);
  if (!position.red_to_move()) {
    leaf_average *= -1.0;
  }

  // double longest_line = position.red_longest() - position.white_longest();
  double edge_color = CountEdgeColors(position);

  std::vector<Line> lines;
  position.EnumerateLines(&lines);
  if (lines.size() == 0) {
    std::cerr << "gah!";
    exit(EXIT_FAILURE);
  }

  std::vector<double> endpoints;
  std::vector<double> sum_edges;
  std::vector<double> max_edges;

  for (Line& line : lines) {
    double endpoint = 1.0 / (1.0 + line.endpoint_distance);
    double sum_edge = (1.0 / (1.0 + line.edge_distances[0]) +
        1.0 / (1.0 + line.edge_distances[1]));
    double max_edge = std::max(
        1.0 / (1.0 + line.edge_distances[0]),
        1.0 / (1.0 + line.edge_distances[1]));

    if (!line.is_red) {
      endpoint *= -1.0;
      sum_edge *= -1.0;
      max_edge *= -1.0;
    }
    endpoints.push_back(endpoint);
    sum_edges.push_back(sum_edge);
    max_edges.push_back(max_edge);
  }

  factors->emplace_back("leaf_average", leaf_average);
  // factors->emplace_back("longest_line", longest_line);
  factors->emplace_back("edge_color", edge_color);

  factors->emplace_back("endpoint_factor",
      std::accumulate(endpoints.begin(), endpoints.end(), 0.0));
  factors->emplace_back("sum_edge_factor",
      std::accumulate(sum_edges.begin(), sum_edges.end(), 0.0));
  factors->emplace_back("max_edge_factor",
      std::accumulate(max_edges.begin(), max_edges.end(), 0.0));

  factors->emplace_back("endpoint_factor_max_min",
      *std::max_element(endpoints.begin(), endpoints.end()) +
      *std::min_element(endpoints.begin(), endpoints.end()));
  factors->emplace_back("sum_edge_factor_max_min",
      *std::max_element(sum_edges.begin(), sum_edges.end()) +
      *std::min_element(sum_edges.begin(), sum_edges.end()));
  factors->emplace_back("max_edge_factor_max_min",
      *std::max_element(max_edges.begin(), max_edges.end()) +
      *std::min_element(max_edges.begin(), max_edges.end()));

  factors->emplace_back("endpoint_factor_average", AverageColor(endpoints));
  factors->emplace_back("sum_edge_factor_average", AverageColor(sum_edges));
  factors->emplace_back("max_edge_factor_average", AverageColor(max_edges));

  double shortcut = 0.0;

  int red_mates = 0;
  int white_mates = 0;
  for (Line& line : lines) {
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
      shortcut = 1.0;
    }
    if (white_mates >= 2) {
      shortcut = -1.0;
    }
  } else {
    if (white_mates > 0) {
      shortcut = -1.0;
    }
    if (red_mates >= 2) {
      shortcut = 1.0;
    }
  }

  factors->emplace_back("shortcut", shortcut);

  factors->emplace_back("min_edge_size",
      std::min<double>(position.max_x(), position.max_y()));
  factors->emplace_back("max_edge_size",
      std::max<double>(position.max_x(), position.max_y()));
}
