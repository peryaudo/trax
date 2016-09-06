// Copyright (C) 2016 Tetsui Ohkubo.

#include "./trax.h"

#include <gflags/gflags.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>   // NOLINT
#include <set>
#include <sstream>
#include <string>
#include <thread>  // NOLINT
#include <utility>

#include "./timer.h"


DEFINE_bool(enable_pretty_dump, true,
            "Position::Dump() will return pretty colored boards.");

DEFINE_bool(enable_strict_timer, true,
            "Exit immediately if the searcher violates time constraint.");

DEFINE_bool(trax8x8, false, "Run as 8x8 Trax.");

DEFINE_string(player_id, "PE",
            "Contest issued player ID.");

DEFINE_int32(thinking_time_ms, 1000,
            "Thinking time in milliseconds.");


uint32_t Random() {
  static uint32_t x = 123456789;
  static uint32_t y = 362436069;
  static uint32_t z = 521288629;
  static uint32_t w = 88675123;
  static std::mutex mutex;
  uint32_t t;
  std::lock_guard<std::mutex> lock(mutex);

  t = x ^ (x << 11);
  x = y; y = z; z = w;
  return w = (w ^ (w >> 19)) ^ (t ^ (t >> 8));
}

using NeighborKey = uint32_t;

// Encode neighboring pieces into key.
NeighborKey EncodeNeighborKey(int right, int top, int left, int bottom) {
  return static_cast<NeighborKey>(
      right + (top << 3) + (left << 6) + (bottom << 9));
}

// Table of possible piece kinds for the certain neighboring piece combination.
// Using this gives significant performance improvement on
// Position::GetPossiblePieces().
PieceSet g_possible_pieces_table[1 << 12];

// Should be called before Position::GetPossiblePieces().
void GeneratePossiblePiecesTable() {
  std::fill(g_possible_pieces_table,
            g_possible_pieces_table +
            sizeof(g_possible_pieces_table) /
            sizeof(g_possible_pieces_table[0]),
            PieceSet());

  for (int i_right = 0; i_right < NUM_PIECES; ++i_right) {
    for (int j_top = 0; j_top < NUM_PIECES; ++j_top) {
      for (int k_left = 0; k_left < NUM_PIECES; ++k_left) {
        for (int l_bottom = 0; l_bottom < NUM_PIECES; ++l_bottom) {
          PieceSet pieces;

          if (i_right == PIECE_EMPTY &&
              j_top == PIECE_EMPTY &&
              k_left == PIECE_EMPTY &&
              l_bottom == PIECE_EMPTY) {
            // If all the neighboring pieces are empty then empty piece is
            // the only legal piece.
            pieces.set(PIECE_EMPTY);
          } else {
            const char *neighbors[4] = {
              kPieceColors[i_right],
              kPieceColors[j_top],
              kPieceColors[k_left],
              kPieceColors[l_bottom]
            };

            // Exclude empty piece for this loop.
            for (int m_candidate = 1; m_candidate< NUM_PIECES; ++m_candidate) {
              const char *candidate = kPieceColors[m_candidate];

              bool valid = true;
              for (int n = 0; n < 4; ++n) {
                // If the neighboring cell isn't empty,
                // and the color of the shared edges are different,
                // then the candidate is invalid piece placement.
                if (neighbors[n][0] != '_' &&
                    candidate[n] != neighbors[n][(n + 2) & 3]) {
                  valid = false;
                  break;
                }
              }

              if (valid) {
                pieces.set(m_candidate);
              }
            }

            // If it is possible to place at least one kind of non-empty pieces
            // then it would also legal to remain empty.
            // Otherwise, the placement is illegal at all.
            if (pieces.count() > 0) {
              pieces.set(PIECE_EMPTY);
            }
          }

          if (pieces.count() > 0) {
            g_possible_pieces_table[
              EncodeNeighborKey(i_right, j_top, k_left, l_bottom)] = pieces;
          }
        }
      }
    }
  }
}

// Table of track directions that come into pieces.
// Although using this makes code simpler, the performance improvement on
// Position::TraceVictoryLineOrLoop() is slighter.
int g_track_direction_table[NUM_PIECES][4];

// Should be called before Position::TraceVictoryLineOrLoop().
void GenerateTrackDirectionTable() {
  // Skip empty piece.
  for (int i = 1; i < NUM_PIECES; ++i) {
    // Loop for direction that the track come from.
    for (int j = 0; j < 4; ++j) {
      // Find the direction that the track goes to.
      for (int k = 0; k < 4; ++k) {
        // Two edges have the same color.
        if (j != k && kPieceColors[i][j] == kPieceColors[i][k]) {
          g_track_direction_table[i][j] = k;
        }
      }
    }
  }
}

bool g_forced_play_table[1 << 12];

// Should be called before Position::FillForcedPieces().
void GenerateForcedPlayTable() {
  std::fill(g_forced_play_table,
            g_forced_play_table +
            sizeof(g_forced_play_table) /
            sizeof(g_forced_play_table[0]),
            false);

  for (int i_right = 0; i_right < NUM_PIECES; ++i_right) {
    for (int j_top = 0; j_top < NUM_PIECES; ++j_top) {
      for (int k_left = 0; k_left < NUM_PIECES; ++k_left) {
        for (int l_bottom = 0; l_bottom < NUM_PIECES; ++l_bottom) {
          NeighborKey key = EncodeNeighborKey(
              i_right, j_top, k_left, l_bottom);
          PieceSet pieces = g_possible_pieces_table[key];
          // No possible piece including empty one for the location.
          // The position is invalid.
          if (pieces.count() == 0) {
            continue;
          }

          // Exclude empty piece.
          pieces.reset(PIECE_EMPTY);

          if (pieces.count() != 1) {
            // If more than one piece kind is possible, forced play
            // does not happen.
            continue;
          }

          int red_count = 0;
          int white_count = 0;

          int neighbors[4] = {i_right, j_top, k_left, l_bottom};
          for (int m = 0; m < 4; ++m) {
            const int neighbor = neighbors[m];
            if (neighbor == PIECE_EMPTY) {
              continue;
            }

            if (kPieceColors[neighbor][(m + 2) & 3] == 'R') {
              ++red_count;
            } else if (kPieceColors[neighbor][(m + 2) & 3] == 'W') {
              ++white_count;
            }
          }
          if (red_count >= 2 || white_count >= 2) {
            g_forced_play_table[key] = true;
          }
        }
      }
    }
  }
}


Move::Move(const std::string& trax_notation,
           const Position& previous_position)
    : x(0), y(0), piece(PIECE_EMPTY) {
  if (!Parse(trax_notation, previous_position)) {
    std::cerr
      << "cannot parse trax notation \"" << trax_notation << "\"" << std::endl;
    exit(EXIT_FAILURE);
  }
}

bool Move::Parse(const std::string& trax_notation,
                 const Position& previous_position) {
  auto it = trax_notation.begin();

  if (it == trax_notation.end()) {
    return false;
  }

  if (*it == '@') {
    x = -1;
    ++it;
  } else {
    // This way of encoding is called Bijective base-26.
    // http://stackoverflow.com/a/1004651/6583019
    // Specified in: http://fpt.massey.ac.nz/design_competition_rules.asp
    x = 0;
    while (it != trax_notation.end() && 'A' <= *it && *it <= 'Z') {
      x *= static_cast<int>('Z' - 'A') + 1;
      x += static_cast<int>(*it - 'A') + 1;
      ++it;
    }
    --x;
  }

  if (it == trax_notation.end()) {
    return false;
  }

  y = 0;
  while (it != trax_notation.end() && '0' <= *it && *it <= '9') {
    y *= 10;
    y += *it - '0';
    ++it;
  }
  --y;

  if (it == trax_notation.end()) {
    return false;
  }

  // If this is the first move, then the color is limited.
  if (previous_position.max_x() == 0 && previous_position.max_y() == 0) {
    if (*it == '/') {
      piece = PIECE_RWWR;
    } else if (*it == '+') {
      piece = PIECE_RWRW;
    } else {
      return false;
    }
  } else {
    if (previous_position.at(x, y) != PIECE_EMPTY) {
      // Trying to place on already placed coordinate.
      return false;
    }

    PieceSet candidates = previous_position.GetPossiblePieces(x, y);

    // Exclude EMPTY piece for added piece candidates.
    candidates.reset(PIECE_EMPTY);

    piece = PIECE_EMPTY;
    for (int i = 0; i < NUM_PIECES; ++i) {
      if (kPieceNotations[i] == *it && candidates.test(i)) {
        piece = static_cast<Piece>(i);
        break;
      }
    }
    if (piece == PIECE_EMPTY) {
      return false;
    }
  }

  ++it;
  if (it != trax_notation.end()) {
    return false;
  }

  return true;
}

std::string Move::notation() const {
  std::stringstream trax_notation;
  if (x == -1) {
    trax_notation << '@';
  } else {
    // This way of encoding is called Bijective base-26.
    // https://github.com/alexfeseto/hexavigesimal
    // Specified in: http://fpt.massey.ac.nz/design_competition_rules.asp
    std::string columns;
    int num = x + 1;
    while (num > 0) {
      --num;
      columns += static_cast<char>(num % ('Z' - 'A' + 1) + 'A');
      num /= 'Z' - 'A' + 1;
    }
    std::reverse(columns.begin(), columns.end());
    trax_notation << columns;
  }

  trax_notation << y + 1;
  trax_notation << kPieceNotations[piece];
  return trax_notation.str();
}

std::vector<Move> Position::GenerateMoves() const {
  std::vector<Move> moves;

  moves.reserve(32);

  if (finished()) {
    // This is a finished game.
    return moves;
  }

  // If this is the first step then they are only valid moves.
  if (max_x_ == 0 && max_y_ == 0) {
    moves.emplace_back("@0/", *this);
    moves.emplace_back("@0+", *this);
    return moves;
  }

  // TODO(tetsui): This is very naive implementation but maybe we can
  // progressively update the possible move list.
  // (such as using UnionFind tree?)
  for (int i_x = -1; i_x <= max_x_; ++i_x) {
    for (int j_y = -1; j_y <= max_y_; ++j_y) {
      if (at(i_x, j_y) != PIECE_EMPTY) {
        continue;
      }

      if (FLAGS_trax8x8 &&
          ((max_x_ >= 8 && (i_x == -1 || i_x == max_x_)) ||
           (max_y_ >= 8 && (j_y == -1 || j_y == max_y_)))) {
        // Invalid move for 8x8 Trax.
        continue;
      }

      PieceSet pieces = GetPossiblePieces(i_x, j_y);
      // Exclude EMPTY piece for added piece candidates.
      pieces.reset(PIECE_EMPTY);

      for (int k = 0; k < NUM_PIECES; ++k) {
        if (pieces.test(k)) {
          moves.emplace_back(i_x, j_y, static_cast<Piece>(k));
        }
      }
    }
  }

  return moves;
}

bool Position::DoMove(Move move, Position *next_position) const {
  assert(next_position != nullptr);
  assert(next_position != this);
  assert(move.piece != PIECE_EMPTY);

  if (FLAGS_trax8x8 &&
      ((max_x_ >= 8 && (move.x == -1 || move.x == max_x_)) ||
       (max_y_ >= 8 && (move.y == -1 || move.y == max_y_)))) {
    // Invalid move for 8x8 Trax.
    return false;
  }

  if (finished()) {
    // Every move after the game finished is illegal.
    return false;
  }

  // Be aware of the memory leak!
  delete[] next_position->board_;
  next_position->board_ = nullptr;

  // Flip the side to move.
  next_position->red_to_move_ = !red_to_move_;

  // Extend the field width and height if it is required by the move.
  next_position->max_x_ = max_x_;
  next_position->max_y_ = max_y_;

  int offset_x = 0, offset_y = 0;

  if (move.x < 0) {
    ++offset_x;
    ++next_position->max_x_;
  } else if (move.x >= max_x_) {
    ++next_position->max_x_;
  }

  if (move.y < 0) {
    ++offset_y;
    ++next_position->max_y_;
  } else if (move.y >= max_y_) {
    ++next_position->max_y_;
  }

  // Sentinels with its depth 2 is used here.
  next_position->board_ = new Piece[next_position->board_size()];

  if (max_x_ == next_position->max_x_ && max_y_ == next_position->max_y_) {
    // Hope it will be vectorized :)
    std::copy(board_, board_ + board_size(), next_position->board_);
  } else {
    // Fill the board with empty pieces.
    std::fill(next_position->board_,
              next_position->board_ + next_position->board_size(),
              PIECE_EMPTY);

    // Copy the board (if exists).
    if (board_ != nullptr) {
      for (int i_x = 0; i_x < max_x_; ++i_x) {
        for (int j_y = 0; j_y < max_y_; ++j_y) {
          next_position->at(i_x + offset_x, j_y + offset_y) = at(i_x, j_y);
        }
      }
    }
  }

  // Don't forget to add the new piece!
  next_position->at(move.x + offset_x, move.y + offset_y) = move.piece;

  // The move is illegal when forced play is applied.
  if (!next_position->FillForcedPieces(move.x + offset_x,
                                       move.y + offset_y)) {
    return false;
  }

  return true;
}

PieceSet Position::GetPossiblePieces(int x, int y) const {
  assert(at(x, y) == PIECE_EMPTY);

  NeighborKey key = EncodeNeighborKey(
      at(x + kDx[0], y + kDy[0]),
      at(x + kDx[1], y + kDy[1]),
      at(x + kDx[2], y + kDy[2]),
      at(x + kDx[3], y + kDy[3]));
  return g_possible_pieces_table[key];
}

void Position::EnumerateLines(std::vector<Line> *lines) const {
  lines->clear();
  if (finished()) {
    return;
  }

  // Clockwisely traced external facing edges.
  std::map<std::pair<int, int>, int> indexed_edges;
  int total_index;

  // Set of <endpoint_a, endpoint_b, is_red> to keep traced lines unique.
  std::set<std::tuple<std::pair<int, int>, std::pair<int, int>, bool>> traced;

  // Iterate over cells with external facing edge.
  for (int i_x = 0; i_x < max_x_; ++i_x) {
    for (int j_y = 0; j_y < max_y_; ++j_y) {
      if (at(i_x, j_y) == PIECE_EMPTY) {
        continue;
      }

      bool is_edge = false;
      for (int k = 0; k < 4; ++k) {
        const int nx = i_x + kDx[k];
        const int ny = j_y + kDy[k];
        if (at(nx, ny) == PIECE_EMPTY) {
          // The cell at <i_x, j_y> has external facing edge.
          is_edge = true;

          // Generate indexed_edges if not yet generated.
          if (indexed_edges.empty()) {
            TraceAndIndexEdges(nx, ny, &indexed_edges, &total_index);
          }
          break;
        }
      }

      // Nothing to do if the cell does not have external facing edge.
      if (!is_edge) {
        continue;
      }

      // Trace line from the cell for each color.
      for (int k_red = 0; k_red < 2; ++k_red) {
        bool is_red = static_cast<bool>(k_red);

        std::pair<int, int> endpoint_a, endpoint_b;

        // Trace the line.
        TraceLineToEndpoints(i_x, j_y, is_red, &endpoint_a, &endpoint_b);

        if (endpoint_a > endpoint_b) {
          std::swap(endpoint_a, endpoint_b);
        }

        // Skip to add to list if already added.
        if (traced.count(make_tuple(endpoint_a, endpoint_b, is_red))) {
          continue;
        }
        traced.insert(make_tuple(endpoint_a, endpoint_b, is_red));

        if (!indexed_edges.count(endpoint_a) ||
            !indexed_edges.count(endpoint_b)) {
          // It is possible that the board has empty region inside.
          // We ignore these cases for now.
          continue;
        }
        Line line(endpoint_a, endpoint_b, is_red, *this,
                  indexed_edges, total_index);

        // Add to the list.
        lines->push_back(line);
      }
    }
  }

#if 0
  // Map endpoints with clockwise index to the line index by the colors.
  std::map<int, int> endpoints_by_color[2];
  for (int i = 0; i < lines->size(); ++i) {
    Line& line = (*lines)[i];
    const bool is_red = line.is_red;

    endpoints_by_color[is_red][line.endpoint_index_a] = i;
    endpoints_by_color[is_red][line.endpoint_index_b] = i;
  }

  // Fill Line.is_inner by referencing neighboring endpoints.
  for (int i = 0; i < lines->size(); ++i) {
    Line& line = (*lines)[i];

    const bool is_red = line.is_red;

    // Just tracing from endpoint_a is sufficient.
    auto it = endpoints_by_color[is_red].find(line.endpoint_index_a);
    assert(it != endpoints_by_color[is_red].end());

    auto previous_it = it;
    if (previous_it == endpoints_by_color[is_red].begin()) {
      previous_it = endpoints_by_color[is_red].end();
    }
    --previous_it;

    auto next_it = it;
    ++next_it;
    if (next_it == endpoints_by_color[is_red].end()) {
      next_it = endpoints_by_color[is_red].begin();
    }

    // If more than one of its neighboring points is the line's,
    // it's not inner line.
    if (previous_it->second == i || next_it->second == i) {
      line.is_inner = false;
    } else {
      line.is_inner = true;
    }
  }

  // Fill loop_distances for lines with is_inner == true.
  for (Line& line : *lines) {
    if (!line.is_inner) {
      continue;
    }

    const bool is_red = line.is_red;

    auto it_a = endpoints_by_color[is_red].find(line.endpoint_index_a);
    auto it_b = endpoints_by_color[is_red].find(line.endpoint_index_b);
    assert(it_a != endpoints_by_color[is_red].end());
    assert(it_b != endpoints_by_color[is_red].end());

    // Calculate loop distances.
    // It moves to opposite directions for different endpoints so that
    // they form possible loops.
    // One inner line can form two loops so we first try
    // clockwise-anticlockwise for endpoint_a, endpoint_b, then flip to
    // anticlockwise-clockwise for endpoint_a, endpoint_b.
    for (int i = 0; i < 2; ++i) {
      int loop_distance = 0;

      auto it = it_a;
      while (true) {
        auto forward_it = it;
        ++forward_it;
        if (forward_it == endpoints_by_color[is_red].end()) {
          forward_it = endpoints_by_color[is_red].begin();
        }

        if (it->second != forward_it->second) {
          loop_distance += abs(forward_it->first - it->first);
        }

        assert(forward_it->second < lines->size());

        it = forward_it;

        if ((*lines)[it->second].is_inner) {
          break;
        }
      }

      // If reached endpoint is same as endpoint_b, you don't have to trace
      // again. Otherwise, it would be double counted.

      if (it != it_b) {
        it = it_b;
        while (true) {
          auto backward_it = it;
          if (it == endpoints_by_color[is_red].begin()) {
            backward_it = endpoints_by_color[is_red].end();
          }
          --backward_it;

          if (it->second != backward_it->second) {
            loop_distance += abs(it->first - backward_it->first);
          }

          assert(backward_it->second < lines->size());

          it = backward_it;

          if ((*lines)[it->second].is_inner) {
            break;
          }
        }
      }

      line.loop_distances[i] = loop_distance;
      swap(it_a, it_b);
    }
  }
#endif
}

void Position::Dump() const {
  if (board_ != nullptr) {
    if (FLAGS_enable_pretty_dump) {
      {
        std::vector<std::string> line(3, "   ");
        for (int i_x = -1; i_x <= max_x_; ++i_x) {
          line[0] += "   ";
          line[1] += "   ";
          line[2] += " ";
          if (i_x == -1) {
            line[2] += "@";
          } else {
            if (i_x + 'A' > 'Z') {
              line[2] += " ";
            } else {
              line[2] += i_x + 'A';
            }
          }
          line[2] += " ";
        }

        for (int i = 0; i < 3; ++i) {
          std::cerr << line[i] << std::endl;
        }
      }
      for (int i_y = -1; i_y <= max_y_; ++i_y) {
        std::vector<std::string> line(3);
        line[0] += "   ";
        std::stringstream num;
        num << std::setw(3) << std::right << i_y + 1;
        line[1] += num.str();
        line[2] += "   ";

        for (int j_x = -1; j_x <= max_x_; ++j_x) {
          Piece piece = at(j_x, i_y);
          assert(
              at(j_x, i_y) <
              sizeof(kPieceNotations) / sizeof(kPieceNotations[0]));
          for (int k = 0; k < 3; ++k) {
            line[k] += kLargePieceNotations[piece][k];
          }
        }

        for (int k = 0; k < 3; ++k) {
          std::cerr << line[k] << std::endl;
        }
      }
    } else {
      for (int i_y = -1; i_y <= max_y_; ++i_y) {
        for (int j_x = -1; j_x <= max_x_; ++j_x) {
          assert(
              at(j_x, i_y) <
              sizeof(kPieceNotations) / sizeof(kPieceNotations[0]));
          std::cerr << kPieceNotations[at(j_x, i_y)];
        }
        std::cerr << std::endl;
      }
    }
  }
  std::cerr << "red_to_move = " << (red_to_move_ ? "true" : "false");
  if (finished()) {
    std::cerr << ", finished = true, winner = ";
    if (winner() > 0) {
      std::cerr << "red";
    } else if (winner() < 0) {
      std::cerr << "white";
    } else {
      std::cerr << "draw";
    }
    std::cerr << ", winning_reason = ";
    switch (winning_reason()) {
      case WINNING_REASON_UNKNOWN:
        std::cerr << "UNKNOWN";
        break;
      case WINNING_REASON_LOOP:
        std::cerr << "LOOP";
        break;
      case WINNING_REASON_LINE:
        std::cerr << "LINE";
        break;
      case WINNING_REASON_FULL:
        std::cerr << "FULL";
        break;
      case WINNING_REASON_RESIGN:
        std::cerr << "RESIGN";
        break;
    }
    std::cerr << std::endl;
  } else {
    std::cerr << ", finished = false";
  }
  std::cerr << std::endl;
}

bool Position::FillForcedPieces(int move_x, int move_y) {
  // Winner flags can be filled by performing checking from some checkpoints,
  // but we have to do them after all the forced plays are done,
  // due to some corner cases.
  //
  // Example: the left side and the right side is connected by victory line,
  // but suddenly forced play filled the rightmost cells.
  //
  // Thus, we have to enumerate all of them first.
  std::pair<int8_t, int8_t> winner_flag_checkpoints[64];
  int num_checkpoints = 0;

  winner_flag_checkpoints[num_checkpoints].first = move_x;
  winner_flag_checkpoints[num_checkpoints].second = move_y;
  ++num_checkpoints;

  std::pair<int8_t, int8_t> possible_queue[64];
  int queue_begin = 0;
  int queue_end = 0;

  // Add neighboring cells to the queue as forced play candidates.
  for (int j = 0; j < 4; ++j) {
    const int nx = move_x + kDx[j];
    const int ny = move_y + kDy[j];

    // Forced plays should not happen outside the current board region.
    // Therefore checking inside [0, max_x) [0, max_y) is enough.
    if (nx < 0 || ny < 0 || nx >= max_x_ || ny >= max_y_) {
      continue;
    }

    if (at(nx, ny) == PIECE_EMPTY) {
      assert(
          queue_end + 1 < sizeof(possible_queue) / sizeof(possible_queue[0]));
      possible_queue[queue_end].first = nx;
      possible_queue[queue_end].second = ny;
      ++queue_end;
    }
  }

  // Loop while chain of forced plays is happening.
  // while (!possible_queue.empty()) {
  while (queue_begin < queue_end) {
    const int x = possible_queue[queue_begin].first;
    const int y = possible_queue[queue_begin].second;
    ++queue_begin;

    // A place may be filled after the coordinate is pushed to the queue,
    // before the coordinate is popped.
    if (at(x, y) != PIECE_EMPTY) {
      continue;
    }

    const PieceSet pieces = GetPossiblePieces(x, y);
    // No possible piece including empty one for the location.
    // The whole position is invalid.
    if (pieces.count() == 0) {
      return false;
    }

    // This is forced play.
    const bool is_forced_play = g_forced_play_table[
        EncodeNeighborKey(
          at(x + kDx[0], y + kDy[0]),
          at(x + kDx[1], y + kDy[1]),
          at(x + kDx[2], y + kDy[2]),
          at(x + kDx[3], y + kDy[3]))];

    if (!is_forced_play) {
      continue;
    }

    assert(pieces.count() == 2);

    for (int i = 1; i < NUM_PIECES; ++i) {
      if (pieces.test(i)) {
        assert(at(x, y) == PIECE_EMPTY);

        // Place the forced piece.
        at(x, y) = static_cast<Piece>(i);
        break;
      }
    }

    // Add the coordinate to winner flag checkpoints, because
    // it may constitute new loop or victory line.
    assert(
        num_checkpoints + 1 <
        sizeof(winner_flag_checkpoints) / sizeof(winner_flag_checkpoints[0]));
    winner_flag_checkpoints[num_checkpoints].first = x;
    winner_flag_checkpoints[num_checkpoints].second = y;
    ++num_checkpoints;

    // Add neighboring cells to the queue as new forced play candidates.
    for (int j = 0; j < 4; ++j) {
      const int nx = x + kDx[j];
      const int ny = y + kDy[j];

      // Forced plays should not happen outside the current board region.
      // Therefore checking inside [0, max_x) [0, max_y) is enough.
      if (nx < 0 || ny < 0 || nx >= max_x_ || ny >= max_y_) {
        continue;
      }

      if (at(nx, ny) == PIECE_EMPTY) {
        assert(
            queue_end + 1 <
            sizeof(possible_queue) / sizeof(possible_queue[0]));
        possible_queue[queue_end].first = nx;
        possible_queue[queue_end].second = ny;
        ++queue_end;
      }
    }
  }

  // Fill winner flags based on the position state.
  for (int i = 0; i < num_checkpoints; ++i) {
    FillWinnerFlags(winner_flag_checkpoints[i].first,
                    winner_flag_checkpoints[i].second);
  }

  if (red_winner_ && white_winner_) {
    // This is a win for the player who made the last move.
    // See http://www.traxgame.com/about_faq.php for detail.
    if (red_to_move_) {
      red_winner_ = false;
    } else {
      white_winner_ = false;
    }
  }

  if (FLAGS_trax8x8 && !finished() && max_x_ >= 8 && max_y_ >= 8) {
    // For 8x8 Trax, if region is filled without any victory lines or loops,
    // the game is considered draw.

    bool all_filled = true;
    for (int i_x = 0; i_x < max_x_; ++i_x) {
      for (int j_y = 0; j_y < max_y_; ++j_y) {
        if (at(i_x, j_y) == PIECE_EMPTY) {
          all_filled = false;
          break;
        }
      }
      if (all_filled) {
        break;
      }
    }

    if (all_filled) {
      red_winner_ = true;
      white_winner_ = true;

      red_winning_reason_ = WINNING_REASON_FULL;
      white_winning_reason_ = WINNING_REASON_FULL;
    }
  }

  return true;
}

void Position::FillWinnerFlags(int x, int y) {
  assert(at(x, y) != PIECE_EMPTY);

  WinningReason reason;

  if (!red_winner_ &&
      (reason = TraceVictoryLineOrLoop(x, y, /* red_line = */ true)) !=
      WINNING_REASON_UNKNOWN) {
    red_winner_ = true;
    red_winning_reason_ = reason;
  }

  if (!white_winner_ &&
      (reason = TraceVictoryLineOrLoop(x, y, /* red_line = */ false)) !=
      WINNING_REASON_UNKNOWN) {
    white_winner_ = true;
    white_winning_reason_ = reason;
  }
}

WinningReason Position::TraceVictoryLineOrLoop(int start_x, int start_y,
                                      bool red_line) {
  assert(at(start_x, start_y) != PIECE_EMPTY);
  assert(0 <= start_x && start_x < max_x_ && 0 <= start_y && start_y < max_y_);

  const char traced_color = red_line ? 'R' : 'W';

  // Omit edge hit detection for most of the boards.
  if (max_x_ < 8 && max_y_ < 8) {
    // Traces line to two directions with the edges of the given color.
    for (int i = 0; i < 4; ++i) {
      if (kPieceColors[at(start_x, start_y)][i] != traced_color) {
        // continue if the i-th edge color of the starting piece is
        // not trace_color.
        continue;
      }

      int x = start_x + kDx[i];
      int y = start_y + kDy[i];
      int previous_direction = (i + 2) & 3;

      assert(-2 <= x && x < max_x_ + 2 && -2 <= y && y < max_y_ + 2);

      // Trace the line until it hits an empty cell.
      while (at(x, y) != PIECE_EMPTY) {
        if (x == start_x && y == start_y) {
          // This is loop.
          return WINNING_REASON_LOOP;
        }

        assert(0 <= x && x < max_x_ && 0 <= y && y < max_y_);

        // Continue tracing the line.
        const int next_direction =
          g_track_direction_table[at(x, y)][previous_direction];

        x += kDx[next_direction];
        y += kDy[next_direction];

        previous_direction = (next_direction + 2) & 3;
      }
    }

    return WINNING_REASON_UNKNOWN;
  }

  // See also: definition of kDx[] and kDy[]
  //                    x           y  x  y
  const int edges[4] = {max_x_ - 1, 0, 0, max_y_ - 1};

  // hits becomes true if the line *hits* the edge of the board,
  // which is necessary for checking victory line.
  bool hits[4] = {false};

  // Traces line to two directions with the edges of the given color.
  for (int i = 0; i < 4; ++i) {
    if (kPieceColors[at(start_x, start_y)][i] != traced_color) {
      // continue if the i-th edge color of the starting piece is
      // not trace_color.
      continue;
    }

    {
      const int xy[2] = {start_x, start_y};
      if (edges[i] == xy[i & 1]) {
        // You hit the edge at the beggining.
        hits[i] = true;
      }
    }

    int x = start_x + kDx[i];
    int y = start_y + kDy[i];
    int previous_direction = (i + 2) & 3;

    assert(-2 <= x && x < max_x_ + 2 && -2 <= y && y < max_y_ + 2);

    // Trace the line until it hits an empty cell.
    while (at(x, y) != PIECE_EMPTY) {
      if (x == start_x && y == start_y) {
        // This is loop.
        return WINNING_REASON_LOOP;
      }

      assert(0 <= x && x < max_x_ && 0 <= y && y < max_y_);

      const int xy[2] = {x, y};

      // Continue tracing the line.
      const int next_direction =
        g_track_direction_table[at(x, y)][previous_direction];

      if (edges[next_direction] == xy[next_direction & 1]) {
        // You hit the edge.
        hits[next_direction] = true;
      }

      x += kDx[next_direction];
      y += kDy[next_direction];

      previous_direction = (next_direction + 2) & 3;
    }
  }

  // If the victory line is enabled i.e. the axis is longer than 8 or not.
  // 0: x-axis 1: y-axis
  const bool victory_line_enabled[2] = {max_x_ >= 8, max_y_ >= 8};
  for (int i = 0; i < 2; ++i) {
    if (victory_line_enabled[i] && hits[i] && hits[(i + 2) & 3]) {
      // This is victory line.
      return WINNING_REASON_LINE;
    }
  }

  return WINNING_REASON_UNKNOWN;
}

void Position::TraceLineToEndpoints(int x, int y, bool red_line,
                                    std::pair<int, int> *endpoint_a,
                                    std::pair<int, int> *endpoint_b) const {
  const char traced_color = red_line ? 'R' : 'W';

  bool first = true;
  for (int i = 0; i < 4; ++i) {
    Piece piece = at(x, y);
    if (kPieceColors[piece][i] != traced_color) {
      continue;
    }

    int nx = x + kDx[i];
    int ny = y + kDy[i];
    int previous_direction = (i + 2) & 3;
    while (at(nx, ny) != PIECE_EMPTY) {
      // Tracing the line.
      const int next_direction =
        g_track_direction_table[at(nx, ny)][previous_direction];

      nx += kDx[next_direction];
      ny += kDy[next_direction];
      previous_direction = (next_direction + 2) & 3;
    }

    if (first) {
      *endpoint_a = std::make_pair(nx, ny);
    } else {
      *endpoint_b = std::make_pair(nx, ny);
    }

    first = false;
  }
}

void Position::TraceAndIndexEdges(
    int start_x, int start_y,
    std::map<std::pair<int, int>, int> *indexed_edges,
    int *total_index) const {
  int x = start_x;
  int y = start_y;
  int previous_direction = -1;
  for (int i = 0; i < 4; ++i) {
    // Find these directions:
    //
    //   ->
    // P P P
    //
    // or
    //
    //     P
    //   ^ P
    // P P P
    //
    // or
    //
    // >
    //   P P P
    //   P
    //   P
    //
    // (P are pieces.)
    //
    // Piece in the front is empty while piece on the right side is not empty.
    if (at(x + kDx[i], y + kDy[i]) == PIECE_EMPTY &&
        at(x + kDx[(i - 1 + 4) & 3],
           y + kDy[(i - 1 + 4) & 3]) != PIECE_EMPTY) {
      previous_direction = i;
      break;
    }
  }

  assert(previous_direction >= 0);

  int current_index = 0;

  while (true) {
    indexed_edges->insert(std::make_pair(std::make_pair(x, y), current_index));
    ++current_index;

    // Trace in clockwise order.
    for (int i = -1; i < 3; ++i) {
      const int next_direction = (previous_direction + i + 4) & 3;
      const int nx = x + kDx[next_direction];
      const int ny = y + kDy[next_direction];
      if (at(nx, ny) == PIECE_EMPTY) {
        x = nx;
        y = ny;
        previous_direction = next_direction;

        if (i == -1) {
          ++current_index;
        }
        break;
      }
    }

    if (x == start_x && y == start_y) {
      break;
    }
  }

  *total_index = current_index;
}

Line::Line(const std::pair<int, int>& endpoint_a,
           const std::pair<int, int>& endpoint_b,
           bool is_red,
           const Position& position,
           const std::map<std::pair<int, int>, int>& indexed_edges,
           int total_index)
    : is_red(is_red)
    , is_inner(false) {
  loop_distances[0] = 0;
  loop_distances[1] = 0;

  int lowers[2] = {endpoint_a.first, endpoint_a.second};
  int uppers[2] = {endpoint_b.first, endpoint_b.second};
  int maxs[2] = {std::max(position.max_x(), 8), std::max(position.max_y(), 8)};
  for (int i = 0; i < 2; ++i) {
    if (lowers[i] > uppers[i]) {
      std::swap(lowers[i], uppers[i]);
    }
    edge_distances[i] = maxs[i] - 1 - (uppers[i] - 1) + (lowers[i] + 1);
  }

  assert(indexed_edges.find(endpoint_a) != indexed_edges.end());
  assert(indexed_edges.find(endpoint_b) != indexed_edges.end());

  endpoint_index_a = indexed_edges.find(endpoint_a)->second;
  endpoint_index_b = indexed_edges.find(endpoint_b)->second;

  int lower_index = endpoint_index_a;
  int upper_index = endpoint_index_b;
  if (lower_index > upper_index) {
    std::swap(lower_index, upper_index);
  }

  // std::cerr << lower_index << " vs " << upper_index << std::endl;

  endpoint_distance =
    std::min(
        upper_index - lower_index,
        lower_index + total_index - upper_index);
#if 0
  manhattan_distance = abs(endpoint_a.first - endpoint_b.first) +
    abs(endpoint_a.second - endpoint_b.second);
#endif
}

void StartTraxClient(Searcher* searcher) {
  assert(searcher != nullptr);

  std::cerr << "Searcher: " << searcher->name() << std::endl;

  bool success = false;

  Position position;
  Position next_position;

  std::string command;

  bool handshaken = false;
  bool i_am_red = true;

  while (std::cin >> command) {
    if (command == "-T") {
      // Announce specified client ID.
      std::cout << FLAGS_player_id << "\n";
      std::cout << std::flush;
      continue;
    }

    if (command == "-B" || command == "-R") {
      // Nothing to do for now.
      // Black == Red for Trax.
      i_am_red = true;
      handshaken = true;
      std::cerr << "The opponent is white." << std::endl;
      continue;
    }

    if (command == "-W") {
      // Skip Trax notation read if you put the move first.
      i_am_red = false;
      handshaken = true;
      std::cerr << "The opponent is red." << std::endl;
    } else {
      if (!handshaken) {
        i_am_red = true;
        handshaken = true;
        std::cerr
          << "Warning: handshake incomplete; "
          << "Suppose the opponent is white." << std::endl;
      }

      // Otherwise command is raw Trax notation. Apply the received trax move.
      success = position.DoMove(Move(command, position), &next_position);
      if (!success) {
        std::cerr
          << "Hey the opponent is returning the illegal move!" << std::endl;
        exit(EXIT_FAILURE);
      }
      position.Swap(&next_position);
      position.Dump();

      // The game is finished by the opponent's move.
      if (position.finished()) {
        break;
      }
    }

    // Search the best move.
    Timer timer(FLAGS_thinking_time_ms - 100);
    Move best_move = searcher->SearchBestMove(position, &timer);
    success = position.DoMove(best_move, &next_position);
    if (!success) {
      std::cerr
        << "What the ... the engine returned illegal move. " << std::endl;
      exit(EXIT_FAILURE);
    }

    // Apply the move.
    position.Swap(&next_position);
    position.Dump();

    // Send the move back to stdout.
    std::cout << best_move.notation() << "\n";
    std::cout << std::flush;

    // The game is finished by our move.
    if (position.finished()) {
      break;
    }
  }

  if (position.finished()) {
    const int winner = i_am_red ? position.winner() : -position.winner();
    if (winner > 0) {
      std::cerr << "I won the game!" << std::endl;
    } else if (winner < 0) {
      std::cerr << "The opponent won..." << std::endl;
    } else {
      std::cerr << "The game was draw." << std::endl;
    }
  } else {
    std::cerr << "The game is unfinished." << std::endl;
  }
}

void StartSelfGame(Searcher* white_searcher, Searcher* red_searcher,
                   Game *game_result, bool verbose) {
  assert(white_searcher != nullptr && red_searcher != nullptr);

  game_result->Clear();
  Position position;

  for (int step = 0; !position.finished(); ++step) {
    Timer overall_timer(FLAGS_thinking_time_ms - 50);
    Timer searcher_timer(FLAGS_thinking_time_ms - 100);

    if (verbose) {
      std::cerr << "Step " << step << ": ";
    }
    Move best_move;
    Position next_position;
    bool success = false;

    if (position.red_to_move()) {
      best_move = red_searcher->SearchBestMove(position, &searcher_timer);
    } else {
      best_move = white_searcher->SearchBestMove(position, &searcher_timer);
    }

    game_result->average_search_depths[position.red_to_move()]
      += searcher_timer.completed_depth();
    game_result->average_nps[position.red_to_move()]
      += searcher_timer.nps();

    success = position.DoMove(best_move, &next_position);
    if (!success) {
      std::cerr << "searcher returned invalid move in self play." << std::endl;
      exit(EXIT_FAILURE);
    }
    position.Swap(&next_position);

    game_result->moves.push_back(best_move);

    if (overall_timer.CheckTimeout() && FLAGS_enable_strict_timer) {
      if (!position.red_to_move()) {
        std::cerr << red_searcher->name();
      } else {
        std::cerr << white_searcher->name();
      }
      std::cerr << " violated time constraint "
        << "(elapsed: " << overall_timer.elapsed_ms() << ")" << std::endl;
      exit(EXIT_FAILURE);
    }

    if (verbose) {
      std::cerr << best_move.notation() << std::endl;
      position.Dump();
      std::cerr
        << "Elapsed time: " << overall_timer.elapsed_ms() << "ms "
        << "Speed: " << searcher_timer.nps() <<" node/s "
        << "Completed depth: " << searcher_timer.completed_depth()
        << std::endl;
      std::cerr << std::endl;
    }
  }

  if (verbose) {
    std::string result;
    if (position.winner() > 0) {
      result = "Red wins";
    } else if (position.winner() < 0) {
      result = "White wins";
    } else {
      result = "Tie";
    }
    std::cerr
      << result << " in total "
      << game_result->num_moves() << " steps. " << std::endl;
  }

  game_result->winner = position.winner();
  game_result->winning_reason = position.winning_reason();

  game_result->average_search_depths[0] /= game_result->num_moves() / 2;
  game_result->average_search_depths[1] /= game_result->num_moves() / 2;
  game_result->average_nps[0] /= game_result->num_moves() / 2;
  game_result->average_nps[1] /= game_result->num_moves() / 2;
}

void StartMultipleSelfGames(Searcher* white_searcher, Searcher* red_searcher,
                            int num_games, std::vector<Game>* game_results,
                            bool verbose) {
  game_results->clear();

  std::cerr
    << "white: " << white_searcher->name()
    << " red: " << red_searcher->name() << std::endl;

  int white = 0;
  int red = 0;

  for (int i = 0; i < num_games; ++i) {
    Game game;
    StartSelfGame(white_searcher, red_searcher, &game, verbose);
    game_results->push_back(game);

    if (game.winner > 0) {
      ++red;
    } else if (game.winner < 0) {
      ++white;
    }

    if (verbose || i == num_games - 1) {
      std::cerr
        << "white(" << white_searcher->name() << "): " << white << " "
        << "red(" << red_searcher->name() << "): " << red << " "
        << (white * 100.0 / (white + red)) << "% "
        << std::endl;
    }
  }
}

void ParseCommentedGames(const std::string& filename,
                         std::vector<Game> *games) {
  games->clear();

  std::ifstream ifs(filename);
  if (ifs.fail()) {
    std::cerr << "Cannot open commented game file " << filename << std::endl;
    exit(EXIT_FAILURE);
  }

  std::string line;
  Position position;
  Game game;
  std::string game_kind;

  // TODO(tetsui): This function is dirty.

  while (std::getline(ifs, line)) {
    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());

    if (line[0] == '#') {
      if (!game.moves.empty()) {
        if (game_kind == "Trax") {
          games->push_back(game);
        }
        game_kind = "";
        position.Clear();
        game.Clear();
      }
      continue;
    }

    if (line == "8x8 Trax" || line == "8x8Trax" ||
        line == "Loop Trax" || line == "Trax") {
      game_kind = line;
      continue;
    }

    if (line.size() < 7) {
      continue;
    }

    // 0123456789012
    //   1 @0/   ; comment
    if (line[0] == ' ' && '0' <= line[2] && line[2] <= '9') {
      std::string notation = line.substr(4, 6);
      notation.erase(std::remove(notation.begin(), notation.end(), ' '),
                     notation.end());
      notation.erase(std::remove(notation.begin(), notation.end(), ';'),
                     notation.end());
      notation.erase(std::remove(notation.begin(), notation.end(), ':'),
                     notation.end());
      if (notation == "Resign" || notation == "Time") {
        game.winning_reason = WINNING_REASON_RESIGN;
        // If the last player is red then white resigns so the winner is red.
        game.winner = (game.num_moves() % 2 == 0 ? 1 : -1);
        continue;
      } else if (notation == "win") {
        game.winning_reason = WINNING_REASON_RESIGN;
        game.winner = (game.num_moves() % 2 == 0 ? -1 : 1);
        continue;
      }

      Move move(notation, position);
      game.moves.push_back(move);

      Position next_position;
      position.DoMove(move, &next_position);
      position.Swap(&next_position);

      if (position.finished()) {
        game.winner = position.winner();
        game.winning_reason = position.winning_reason();
      }

      game.comments.push_back("");
    }

    if (line.size() < 11) {
      continue;
    }

    if (line[10] == ';') {
      game.comments.back() += line.substr(11) + "\n";
    }
    if (line[11] == ';') {
      game.comments.back() += line.substr(11) + "\n";
    }
  }

  if (!game.moves.empty()) {
    if (game_kind == "Trax") {
      games->push_back(game);
    }
  }
}

int Game::CountMatchingMoves(Searcher *searcher) {
  int match_count = 0;

  Position position;
  for (Move actual_move : moves) {
    Timer timer(FLAGS_thinking_time_ms - 100);
    Move best_move = searcher->SearchBestMove(position, &timer);

    Position best_position;
    if (!position.DoMove(best_move, &best_position)) {
      std::cerr << "illegal move returned by searcher" << std::endl;
      exit(EXIT_FAILURE);
    }

    Position next_position;
    if (!position.DoMove(actual_move, &next_position)) {
      std::cerr << "illegal move in game" << std::endl;
      exit(EXIT_FAILURE);
    }

    // Count different moves that result into same position as same.
    // This significantly improves reliability of prediction.
    // Pointed out by Nakahara senpai. Thanks!

    // if (best_move == actual_move) {
    if (best_position.Hash() == next_position.Hash()) {
       ++match_count;
    }

    position.Swap(&next_position);
    // position.Dump();
  }

  return match_count;
}


void Game::ContinueBySearcher(Searcher *searcher) {
  Position position;
  for (Move actual_move : moves) {
    Position next_position;
    if (!position.DoMove(actual_move, &next_position)) {
      std::cerr << "illegal move in game" << std::endl;
      exit(EXIT_FAILURE);
    }

    position.Swap(&next_position);
  }

  while (!position.finished()) {
    Timer timer(FLAGS_thinking_time_ms - 100);
    Move best_move = searcher->SearchBestMove(position, &timer);
    moves.push_back(best_move);
    if (!comments.empty()) {
      comments.push_back("");
    }

    Position next_position;
    position.DoMove(best_move, &next_position);
    position.Swap(&next_position);
    position.Dump();
  }

  winner = position.winner();
  winning_reason = position.winning_reason();
}

void Book::Init(const std::vector<Game>& games, int max_steps) {
  for (const Game& game : games) {
    Position position;
    for (int i = 0; i < game.num_moves(); ++i) {
      if (i > max_steps) {
        break;
      }

      Move move = game.moves[i];

      // Only add moves that lead to the win of that player
      if ((i % 2 ? 1 : -1) == game.winner) {
        // TODO(tetsui): Register all the rotated moves of this move.
        books_[position.Hash()].push_back(move);
      }

      Position next_position;
      position.DoMove(move, &next_position);
      position.Swap(&next_position);
    }
  }
}

bool Book::Select(const Position& position, Move *next_move) {
  auto it = books_.find(position.Hash());
  if (it == books_.end()) {
    return false;
  }
  *next_move = it->second[Random() % it->second.size()];
  return true;
}

// Read current board configuration from stdin and return the best move to
// stdout.
void ReadAndFindBestMove(Searcher* searcher) {
  int num_moves = 0;
  std::cin >> num_moves;

  std::vector<std::string> moves_notation(num_moves);
  for (std::string& move_notation : moves_notation) {
    std::cin >> move_notation;
  }

  Position position;
  for (const std::string& move_notation : moves_notation) {
    Move move;
    if (!move.Parse(move_notation, position)) {
      std::cerr
        << "Illegal move. Please go back and try another." << std::endl;
      exit(EXIT_FAILURE);
    }

    Position next_position;
    if (!position.DoMove(move, &next_position)) {
      std::cerr
        << "Illegal move. Please go back and try another." << std::endl;
      exit(EXIT_FAILURE);
    }

    position.Swap(&next_position);
  }

  if (position.finished()) {
    // Otherwise SearchBestMove may crash! Reported by takiyu. (thx!)
    return;
  }

  Timer timer(FLAGS_thinking_time_ms - 100);
  Move best_move = searcher->SearchBestMove(position, &timer);
  std::cout << best_move.notation();
}

// See the description of the corresponding flag.
void ShowPosition() {
  int num_moves = 0;
  std::cin >> num_moves;

  std::vector<std::string> moves_notation(num_moves);
  for (std::string& move_notation : moves_notation) {
    std::cin >> move_notation;
  }

  Position position;
  for (const std::string& move_notation : moves_notation) {
    Move move;
    if (!move.Parse(move_notation, position)) {
      exit(EXIT_FAILURE);
    }

    Position next_position;
    if (!position.DoMove(move, &next_position)) {
      exit(EXIT_FAILURE);
    }

    position.Swap(&next_position);
  }

  if (position.finished()) {
    std::cout << position.winner() << std::endl;
  } else {
    std::cout << std::endl;
  }

  for (int i_y = -1; i_y < position.max_y() + 1; ++i_y) {
    for (int j_x = -1; j_x < position.max_x() + 1; ++j_x) {
      std::cout << static_cast<const Position &>(position).at(j_x, i_y);
      if (j_x == position.max_x()) {
        std::cout << std::endl;
      } else {
        std::cout << " ";
      }
    }
  }
}
