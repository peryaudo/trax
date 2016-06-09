// Copyright (C) 2016 Tetsui Ohkubo.

#include "trax.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <set>

#include "gflags/gflags.h"

DEFINE_bool(enable_pretty_dump, true,
            "Position::Dump() will return pretty colored boards.");

DEFINE_bool(enable_strict_notation, true,
            "Exit immediately if Trax::notation() fail.");

DEFINE_bool(trax8x8, false, "Run as 8x8 Trax.");

DEFINE_string(player_id, "PR",
            "Contest issued player ID.");


uint32_t Random() { 
  // TODO(tetsui): Make it thread safe and remove static.
  static uint32_t x = 123456789;
  static uint32_t y = 362436069;
  static uint32_t z = 521288629;
  static uint32_t w = 88675123; 
  uint32_t t;

  t = x ^ (x << 11);
  x = y; y = z; z = w;
  return w = (w ^ (w >> 19)) ^ (t ^ (t >> 8)); 
}

using NeighborKey = uint32_t;

// Encode neighboring pieces into key.
NeighborKey EncodeNeighborKey(int right, int top, int left, int bottom) {
  return static_cast<NeighborKey>(
      right + (top << 8) + (left << 16) + (bottom << 24));
}

// Table of possible piece kinds for the certain neighboring piece combination.
// Using this gives significant performance improvement on
// Position::GetPossiblePieces().
std::unordered_map<NeighborKey, PieceSet> g_possible_pieces_table;

// Should be called before Position::GetPossiblePieces().
void GeneratePossiblePiecesTable() {
  g_possible_pieces_table.clear();

  for (int i_right = 0; i_right < NUM_PIECES; ++i_right) {
    for (int j_top = 0; j_top < NUM_PIECES; ++j_top) {
      for (int k_left = 0; k_left < NUM_PIECES; ++k_left) {
        for (int l_bottom = 0; l_bottom < NUM_PIECES; ++l_bottom) {
          PieceSet pieces;
          // Empty piece is always legal.
          pieces.set(PIECE_EMPTY);

          if (i_right == PIECE_EMPTY &&
              j_top == PIECE_EMPTY &&
              k_left == PIECE_EMPTY &&
              l_bottom == PIECE_EMPTY) {
            // If all the neighboring pieces are empty then empty piece is 
            // the only legal piece.
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
                    candidate[n] != neighbors[n][(n + 2) % 4]) {
                  valid = false;
                  break;
                }
              }

              if (valid) {
                pieces.set(m_candidate);
              }
            }
          }

          g_possible_pieces_table[
            EncodeNeighborKey(i_right, j_top, k_left, l_bottom)] = pieces;
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

Move::Move(const std::string& trax_notation, const Position& previous_position)
    : x(0)
    , y(0)
    , piece(PIECE_EMPTY) {
  auto it = trax_notation.begin();

  if (it == trax_notation.end()) {
    goto fail;
  }

  if (*it == '@') {
    x = -1;
  } else {
    x = static_cast<int>(*it - 'A');
  }

  ++it;

  if (it == trax_notation.end()) {
    goto fail;
  }

  y = 0;
  while (it != trax_notation.end() && '0' <= *it && *it <= '9') {
    y *= 10;
    y += *it - '0';
    ++it;
  }
  --y;

  if (it == trax_notation.end()) {
    goto fail;
  }

  // If this is the first move, then the color is limited. 
  if (previous_position.max_x() == 0 && previous_position.max_y() == 0) {
    if (*it == '/') {
      piece = PIECE_RWWR;
    } else if (*it == '+') {
      piece = PIECE_RWRW;
    } else {
      goto fail;
    }
  } else {
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
      goto fail;
    }
  }

  ++it;
  if (it != trax_notation.end()) {
    goto fail;
  }
  return;

fail:
  std::cerr << "cannot parse trax notation" << std::endl;
  std::cerr << '"' << trax_notation << '"'
    << " position: " << (it - trax_notation.begin()) << std::endl;
  exit(EXIT_FAILURE);
}

std::string Move::notation() const {
  std::stringstream trax_notation;
  if (x == -1) {
    trax_notation << '@';
  } else {
    if (static_cast<char>(x + 'A') >= 'Z') {
      if (FLAGS_enable_strict_notation) {
        std::cerr << "cannot encode trax notation" << std::endl;
        exit(EXIT_FAILURE);
      } else {
        return "(N/A)";
      }
    }
    trax_notation << static_cast<char>(x + 'A');
  }

  trax_notation << y + 1;
  trax_notation << kPieceNotations[piece];
  return trax_notation.str();
}

std::vector<Move> Position::GenerateMoves() const {
  std::vector<Move> moves;

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
  auto it = g_possible_pieces_table.find(key);
  if (it == g_possible_pieces_table.end()) {
    return PieceSet();
  } else {
    return it->second;
  }
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
      std::cerr << "tie";
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
  std::set<std::pair<int, int>> winner_flag_checkpoints;

  winner_flag_checkpoints.insert(std::make_pair(move_x, move_y));

  std::queue<std::pair<int, int>> possible_queue;

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
      possible_queue.push(std::make_pair(nx, ny));
    }
  }

  // Loop while chain of forced plays is happening.
  while (!possible_queue.empty()) {
    const int x = possible_queue.front().first;
    const int y = possible_queue.front().second;
    possible_queue.pop();

    // A place may be filled after the coordinate is pushed to the queue,
    // before the coordinate is popped.
    if (at(x, y) != PIECE_EMPTY) {
      continue;
    }

    PieceSet pieces = GetPossiblePieces(x, y);
    // No possible piece including empty one for the location.
    // The position is invalid.
    if (pieces.count() == 0) {
      return false;
    }

    // Exclude empty piece.
    pieces.reset(PIECE_EMPTY);

    if (pieces.count() != 1) {
      // If more than one piece kind is possible, forced play
      // does not happen.
      continue;
    }

    // But not all place with only one possible piece is forced play.

    // TODO(tetsui): This can also be implemented as a table like
    // GetPossiblePieces().
    int red_count = 0;
    int white_count = 0;

    for (int i = 0; i < 4; ++i) {
      const int nx = x + kDx[i];
      const int ny = y + kDy[i];
      Piece neighbor = at(nx, ny);
      if (neighbor == PIECE_EMPTY) {
        continue;
      }

      if (kPieceColors[neighbor][(i + 2) % 4] == 'R') {
        ++red_count;
      } else if (kPieceColors[neighbor][(i + 2) % 4] == 'W') {
        ++white_count;
      }
    }

    // This is forced play.
    if (red_count >= 2 || white_count >= 2) {
      for (int i = 1; i < NUM_PIECES; ++i) {
        if (!pieces.test(i)) {
          continue;
        }

        at(x, y) = static_cast<Piece>(i);

        winner_flag_checkpoints.insert(std::make_pair(x, y));

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
            possible_queue.push(std::make_pair(nx, ny));
          }
        }
        break;
      }
    }
  }

  // Fill winner flags based on the position state.
  
  for (auto&& coordinate : winner_flag_checkpoints) {
    FillWinnerFlags(coordinate.first, coordinate.second);
  }

  return true;
}

void Position::FillWinnerFlags(int x, int y) {
  if (!red_winner_ && TraceVictoryLineOrLoop(x, y, /* red_line = */ true)) {
    red_winner_ = true;
  }

  if (!white_winner_ && TraceVictoryLineOrLoop(x, y, /* red_line = */ false)) {
    white_winner_ = true;
  }

  if (FLAGS_trax8x8 && !finished() && max_x_ >= 8 && max_y_ >= 8) {
    // For 8x8 Trax, if region is filled without any victory lines or loops,
    // the game is considered tie.

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
    }
  }
}

bool Position::TraceVictoryLineOrLoop(int start_x, int start_y,
                                      bool red_line) {
  assert(at(start_x, start_y) != PIECE_EMPTY);

  const char traced_color = red_line ? 'R' : 'W';

  int finish_xs[4] = {0};
  int finish_ys[4] = {0};
  bool hits[4] = {false};

  // Traces line to two directions with the edges of the given color.
  for (int i = 0; i < 4; ++i) {
    if (kPieceColors[at(start_x, start_y)][i] != traced_color) {
      continue;
    }

    finish_xs[i] = start_x;
    finish_ys[i] = start_y;
    hits[i] = true;

    int x = start_x + kDx[i];
    int y = start_y + kDy[i];
    int previous_direction = (i + 2) % 4;

    while (at(x, y) != PIECE_EMPTY) {
      if (x == start_x && y == start_y) {
        // This is loop.
        if (Hash() == 6357593829197971889ULL) {
          std::cerr << "This is loop." << std::endl;
        }
        return true;
      }

      // Continue tracing the line.
      const int next_direction =
        g_track_direction_table[at(x, y)][previous_direction];

      finish_xs[next_direction] = x;
      finish_ys[next_direction] = y;
      hits[next_direction] = true;

      x += kDx[next_direction];
      y += kDy[next_direction];
      previous_direction = (next_direction + 2) % 4;
    }
  }

  // Check if it constitutes vertical or horizontal victory line.
  bool has_victory_line = false;

  has_victory_line = has_victory_line || (
      hits[0] && hits[2] &&
      finish_xs[2] == 0 && finish_xs[0] == max_x_ - 1 &&
      finish_xs[0] - finish_xs[2] >= 7);

  if (Hash() == 6357593829197971889ULL && has_victory_line) {
    std::cerr << "This is horizontal victory line." << std::endl;
    std::cerr << finish_xs[2] << " " << finish_xs[0] << std::endl;
  }

  has_victory_line = has_victory_line || (
      hits[3] && hits[1] &&
      finish_ys[1] == 0 && finish_ys[3] == max_y_ - 1 &&
      finish_ys[3] - finish_ys[1] >= 7);

  if (Hash() == 6357593829197971889ULL && has_victory_line) {
    std::cerr << "This is vertical victory line." << std::endl;
    std::cerr << finish_ys[3] << " " << finish_ys[1] << std::endl;
  }


  return has_victory_line;
}

// Enumerate all possible positions within the given depth.
int DoPerft(const Position& position, int depth) {
  // position.Dump();
  if (depth <= 0) {
    return 1;
  }

  int total_positions = 0;

  for (auto&& move : position.GenerateMoves()) {
    Position next_position;
    if (!position.DoMove(move, &next_position)) {
      // The move was illegal.
      continue;
    }
    total_positions += DoPerft(next_position, depth - 1);
  }
  return total_positions;
}

void Perft(int max_depth) {
  Position position;
  for (int i_depth = 0; i_depth <= max_depth; ++i_depth) {
    std::cerr
      << "depth: " << i_depth
      << " leaves: " << DoPerft(position, i_depth) << std::endl;
  }
}

// Return true if red is the winner.
int StartSelfGame(Searcher* white_searcher, Searcher* red_searcher,
                  bool verbose) {
  assert(white_searcher != nullptr && red_searcher != nullptr);
  Position position;

  int step = 0;
  while (!position.finished()) {
    if (verbose) {
      std::cerr << "Step " << step << ": ";
    }
    Move best_move;
    Position next_position;
    bool success = false;

    if (position.red_to_move()) {
      best_move = red_searcher->SearchBestMove(position);
    } else {
      best_move = white_searcher->SearchBestMove(position);
    }

    success = position.DoMove(best_move, &next_position);
    position.Swap(&next_position);

    if (verbose) {
      std::cerr << best_move.notation() << std::endl;
      position.Dump();
      std::cerr << std::endl;
    }
    ++step;
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
    std::cerr << result << " in total " << step << " steps. " << std::endl;
  }

  return position.winner();
}

void StartMultipleSelfGames(Searcher* white_searcher, Searcher* red_searcher,
                            int num_games, bool verbose) {
  std::cerr
    << "white: " << white_searcher->name()
    << " red: " << red_searcher->name() << std::endl;
  int white = 0, red = 0;
  for (int i = 0; i < num_games; ++i) {
    const int result = StartSelfGame(white_searcher, red_searcher, verbose);
    if (result > 0) {
      ++red;
    } else if (result < 0) {
      ++white;
    }

    if (verbose) {
      std::cerr << "white: " << white << " red: " << red << " "
        << (white * 100.0 / (white + red)) << "%" << std::endl;
    }
  }
  std::cerr << "white: " << white << " red: " << red << " "
    << (white * 100.0 / (white + red)) << "%" << std::endl;
}

void StartTraxClient(Searcher* searcher) {
  assert(searcher != nullptr);

  if (!FLAGS_enable_strict_notation) {
    std::cerr
      << "You must enable strict notation for real game." << std::endl;
    exit(EXIT_FAILURE);
  }

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
    Move best_move = searcher->SearchBestMove(position);
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
      std::cerr << "The game was tie." << std::endl;
    }
  } else {
    std::cerr << "The game is unfinished." << std::endl;
  }
}
