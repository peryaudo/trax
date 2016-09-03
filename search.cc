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

  transposition_table_.NewSearch();

  if (iterative_) {
    std::vector<Move> possible_moves = position.GenerateMoves();

    Move best_move;
    for (int current_depth = 0; current_depth <= max_depth_; ++current_depth) {
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
        const int score = -NegaMax(next_position, timer, current_depth);

        best_score = std::max(best_score, score);
        moves.emplace_back(score, move);

        if (current_depth > 0 && timer->CheckTimeout()) {
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

      timer->set_completed_depth(current_depth);
    }

    return best_move;
  } else {
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
      const int score = -NegaMax(next_position, timer, max_depth_);

      best_score = std::max(best_score, score);
      moves.emplace_back(score, move);
    }

    std::vector<Move> best_moves;
    for (ScoredMove& move : moves) {
      if (move.score == best_score) {
        best_moves.push_back(move);
      }
    }

    timer->set_completed_depth(max_depth_);

    assert(best_moves.size() > 0);
    return best_moves[Random() % best_moves.size()];
  }
}

// Decrement the value in a way its absolute value will desrease.
int AbsoluteDecrement(int x) {
  if (x > 0) {
    return x - 1;
  } else if (x < 0) {
    return x + 1;
  } else {
    return 0;
  }
}

// Score the move from the perspective of position.red_to_move().
// Larger is better.
template<typename Evaluator>
int NegaMaxSearcher<Evaluator>::NegaMax(
    const Position& position, Timer* timer, int depth, int alpha, int beta) {
  const int original_alpha = alpha;

  TranspositionTable::Entry entry;

  // At that point of time, we don't care about conflicts.
  const PositionHash key = position.Hash();

  const bool found = transposition_table_.Probe(key, &entry);

  if (found && entry.depth >= depth) {
    if (entry.bound == BOUND_EXACT) {
      return entry.score;
    } else if (entry.bound == BOUND_LOWER) {
      alpha = std::max(alpha, entry.score);
    } else if (entry.bound == BOUND_UPPER) {
      beta = std::min(beta, entry.score);
    }

    if (alpha >= beta) {
      return entry.score;
    }
  }

  entry.score = -kInf;
  entry.best_move = Move();

  if (position.finished() || depth <= 0) {
    // Evaluate the position, from the perspective of position.red_to_move(),
    // and this is same as NegaMax().
    // Thus, there is no need for sign flip.
    entry.score = Evaluator::Evaluate(position);

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
      const int score = AbsoluteDecrement(
          -NegaMax(next_position, timer, depth - 1, -beta, -alpha));

      // The reason why we used AbsoluteDecrement here is to finish the game
      // as early as possible.
      // This not only reduces unexpected behavior to human players, but also
      // works as very good pruning.

      if (entry.score < score) {
        entry.score = score;
        entry.best_move = move;
      }

      alpha = std::max(alpha, score);
      if (alpha >= beta) {
       break;
      }

      if (timer->CheckTimeout()) {
        return 0;
      }
    }
  }

  entry.depth = depth;

  if (entry.score <= original_alpha) {
    entry.bound = BOUND_UPPER;
  } else if (entry.score >= beta) {
    entry.bound = BOUND_LOWER;
  } else {
    entry.bound = BOUND_EXACT;
  }

  transposition_table_.Store(
      key, entry.best_move, entry.score, entry.depth, entry.bound);

  return entry.score;
}

template<typename Evaluator>
Move ThreadedIterativeSearcher<Evaluator>::SearchBestMove(
    const Position& position, Timer* timer) {
  assert(!position.finished());

  Move book_move;
  if (book_.Select(position, &book_move)) {
    return book_move;
  }

  transposition_table_.NewSearch();

  return ThreadedSearcher::SearchBestMove(position, timer);
}

static const std::vector<int> kDepthDensityMatrix[] = {
  {1},
  {0, 1},
  {1, 0},
  {0, 0, 1, 1},
  {0, 1, 1, 0},
  {1, 1, 0, 0},
  {1, 0, 0, 1},
  {0, 0, 0, 1, 1, 1},
  {0, 0, 1, 1, 1, 0},
  {0, 1, 1, 1, 0, 0},
  {1, 1, 1, 0, 0, 0},
  {1, 1, 0, 0, 0, 1},
  {1, 0, 0, 0, 1, 1}
};

template<typename Evaluator>
void ThreadedIterativeSearcher<Evaluator>::DoSearchBestMove(
    const Position& position, int thread_index, int num_threads,
    Timer* timer, Move* best_move, int* best_score, int* completed_depth) {
  std::vector<Move> possible_moves = position.GenerateMoves();

  for (int current_depth = 0; ; ++current_depth) {
    // Skip different depths for each thread using density matrix.
    // auto& row = kDepthDensityMatrix[thread_index];
    // if (!row[current_depth % row.size()]) {
    //   continue;
    // }

    *best_score = -kInf;
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
      const int score = -NegaMax(next_position, timer, current_depth);

      *best_score = std::max(*best_score, score);
      moves.emplace_back(score, move);

      if (current_depth > 0 && timer->CheckTimeout()) {
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
      if (move.score == *best_score) {
        best_moves.push_back(move);
      }
    }

    assert(best_moves.size() > 0);
    *best_move = best_moves[Random() % best_moves.size()];

    timer->set_completed_depth(current_depth);
    *completed_depth = current_depth;
  }
}

// Score the move from the perspective of position.red_to_move().
// Larger is better.
template<typename Evaluator>
int ThreadedIterativeSearcher<Evaluator>::NegaMax(
    const Position& position, Timer* timer, int depth, int alpha, int beta) {
  const int original_alpha = alpha;

  TranspositionTable::Entry entry;

  // At that point of time, we don't care about conflicts.
  const PositionHash key = position.Hash();

  const bool found = transposition_table_.Probe(key, &entry);

  if (found && entry.depth >= depth) {
    if (entry.bound == BOUND_EXACT) {
      return entry.score;
    } else if (entry.bound == BOUND_LOWER) {
      alpha = std::max(alpha, entry.score);
    } else if (entry.bound == BOUND_UPPER) {
      beta = std::min(beta, entry.score);
    }

    if (alpha >= beta) {
      return entry.score;
    }
  }

  entry.score = -kInf;
  entry.best_move = Move();

  if (position.finished() || depth <= 0) {
    // Evaluate the position, from the perspective of position.red_to_move(),
    // and this is same as NegaMax().
    // Thus, there is no need for sign flip.
    entry.score = Evaluator::Evaluate(position);

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
      const int score = AbsoluteDecrement(
          -NegaMax(next_position, timer, depth - 1, -beta, -alpha));

      // The reason why we used AbsoluteDecrement here is to finish the game
      // as early as possible.
      // This not only reduces unexpected behavior to human players, but also
      // works as very good pruning.

      if (entry.score < score) {
        entry.score = score;
        entry.best_move = move;
      }

      alpha = std::max(alpha, score);
      if (alpha >= beta) {
       break;
      }

      if (timer->CheckTimeout()) {
        return 0;
      }
    }
  }

  entry.depth = depth;

  if (entry.score <= original_alpha) {
    entry.bound = BOUND_UPPER;
  } else if (entry.score >= beta) {
    entry.bound = BOUND_LOWER;
  } else {
    entry.bound = BOUND_EXACT;
  }

  transposition_table_.Store(
      key, entry.best_move, entry.score, entry.depth, entry.bound);

  return entry.score;
}


// Instantiation.

#define INSTANTIATE_TEMPLATES_FOR(CLASS) \
  template Move SimpleSearcher<CLASS>::SearchBestMove( \
    const Position& position, Timer* timer); \
  template Move NegaMaxSearcher<CLASS>::SearchBestMove( \
      const Position& position, Timer* timer); \
  template int NegaMaxSearcher<CLASS>::NegaMax( \
      const Position& position, Timer* timer, \
      int depth, int alpha, int beta); \
  template Move ThreadedIterativeSearcher<CLASS>::SearchBestMove( \
      const Position& position, Timer* timer); \
  template void ThreadedIterativeSearcher<CLASS>::DoSearchBestMove( \
      const Position& position, int thread_index, int num_threads, \
      Timer* timer, Move* best_move, int* best_score, int* completed_depth); \
  template int ThreadedIterativeSearcher<CLASS>::NegaMax( \
      const Position& position, Timer* timer, int depth, int alpha, int beta)

INSTANTIATE_TEMPLATES_FOR(LeafAverageEvaluator);
INSTANTIATE_TEMPLATES_FOR(MonteCarloEvaluator);
INSTANTIATE_TEMPLATES_FOR(FactorEvaluator);
INSTANTIATE_TEMPLATES_FOR(NoneEvaluator);
INSTANTIATE_TEMPLATES_FOR(AdvancedFactorEvaluator);
INSTANTIATE_TEMPLATES_FOR(LoopFactorEvaluator);

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
  double factor_evaluator = FactorEvaluator::Evaluate(position);
  if (!position.red_to_move()) {
    leaf_average *= -1.0;
    factor_evaluator *= -1.0;
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

  double total_count = 0.0;
  double inner_count = 0.0;

  for (Line& line : lines) {
    if (line.is_inner) {
      if (line.is_red) {
        inner_count += 1.0;
      } else {
        inner_count -= 1.0;
      }
    }

    if (line.is_red) {
      total_count += 1.0;
    } else {
      total_count -= 1.0;
    }

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
  factors->emplace_back("factor_evaluator", factor_evaluator);
  factors->emplace_back("inner_count", inner_count);
  factors->emplace_back("total_count", total_count);
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
