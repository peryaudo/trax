#include "search.h"

#include "gflags/gflags.h"

// For NegaMax(depth=2), it has impact of 40secs -> 20secs
// for 200 times NegaMax-Random self play.
DEFINE_bool(enable_transposition_table, true,
            "Enable Transposition Table.");


Move NegaMaxSearcher::SearchBestMove(const Position& position) {
  int best_score = -kInf;
  std::vector<ScoredMove> moves;

  for (auto&& move : position.GenerateMoves()) {
    Position next_position;
    if (!position.DoMove(move, &next_position)) {
      // This is illegal move.
      continue;
    }

    const int score = NegaMax(next_position, max_depth_);
    best_score = std::max(best_score, score);
    moves.emplace_back(score, move);
  }

  // std::sort(moves.begin(), moves.end());
#if 0
  for (auto&& move : moves) {
    std::cerr << move.first << " " << move.second.notation() << std::endl;
  }
#endif

  std::vector<Move> best_moves;
  for (auto&& move : moves) {
    if (move.score == best_score) {
      best_moves.push_back(move);
    }
  }

  assert(best_moves.size() > 0);
  return best_moves[Random() % best_moves.size()];
}

int NegaMaxSearcher::NegaMax(const Position& position,
                             int depth, int alpha, int beta) {
  PositionHash hash;
  if (FLAGS_enable_transposition_table) {
    hash = position.Hash();

    // Conflict could happen but we don't care.
    if (transposition_table_.count(hash)) {
      return transposition_table_[hash];
    }
  }

  int max_score = -kInf;

  if (position.finished()) {
    // Think the case where position.red_to_move() == true in SearchBestMove.
    // red_to_move() == false in NegaMax.
    // NegaMax should return positive score for winner() == 1.
    if (position.red_to_move()) {
      max_score = kInf * -position.winner();
    } else {
      max_score = kInf * position.winner();
    }
  } else {
    assert(depth >= 0);
    if (depth == 0) {
      return 0;
    }

    int64_t average_score = 0;
    int64_t num_moves = 0;

    for (auto&& move : position.GenerateMoves()) {
      Position next_position;
      if (!position.DoMove(move, &next_position)) {
        // This is illegal move.
        continue;
      }

      const int score = -NegaMax(next_position, depth - 1, -beta, -alpha);
      if (depth == 1) {
        average_score += score;
        ++num_moves;
      } else {
        max_score = std::max(max_score, score);
        alpha = std::max(alpha, score);
        if (alpha >= beta) {
          break;
        }
      }
    }

    if (depth == 1 && num_moves > 0) {
      average_score /= num_moves;
      max_score = average_score;
    }
  }

  if (FLAGS_enable_transposition_table) {
    return transposition_table_[hash] = max_score;
  } else {
    return max_score;
  }
}

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
