// Copyright (C) 2016 Tetsui Ohkubo.


#define NDEBUG


#include <algorithm>
#include <bitset>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <queue>
#include <unordered_map>
#include <string>
#include <vector>


const char *kPlayerId = "PR";

const int kInf = 1000000000;

// Xorshift 128.
// TODO(tetsui): Make it thread safe.
uint32_t Random() { 
  static uint32_t x = 123456789;
  static uint32_t y = 362436069;
  static uint32_t z = 521288629;
  static uint32_t w = 88675123; 
  uint32_t t;

  t = x ^ (x << 11);
  x = y; y = z; z = w;
  return w = (w ^ (w >> 19)) ^ (t ^ (t >> 8)); 
}

// Be aware that pieces are defined in terms of anti-clockwise edge colors,
// while y-axis of board index is flipped against mathematical definition.
const int kDx[] = {1, 0, -1, 0};
const int kDy[] = {0, -1, 0, 1};

// Piece kinds. It includes color information so it is more specific than
// Trax notation. The alphabets after the prefix specifies colors on the edges
// in anti-clockwise order from the rightmost one.
enum Piece {
	PIECE_EMPTY = 0,

  // W
  //R R
  // W
	PIECE_RWRW,

  // R
  //W W
  // R
	PIECE_WRWR,

  // W
  //W R
  // R
	PIECE_RWWR,

  // R
  //W R
  // W
	PIECE_RRWW,

  // R
  //R W
  // W
	PIECE_WRRW,

  // W
  //R W
  // R
	PIECE_WWRR,

  NUM_PIECES
};

// Edge colors of the pieces as explained above.
const char* kPieceColors[] = {
  "____",
  "RWRW",
  "WRWR",
  "RWWR",
  "RRWW",
  "WRRW",
  "WWRR"
};

// Trax notations of the pieces.
const char kPieceNotations[] = ".++/\\/\\";

const char kLargePieceNotations[][3][32] = {
  {"...",
   "...",
   "..."},
#if 1
  {" \33[37m|\33[0m ",
   "\33[31m---\33[0m",
   " \33[37m|\33[0m "},
  {" \33[31m|\33[0m ",
   "\33[37m-\33[31m|\33[37m-\33[0m",
   " \33[31m|\33[0m "},
  {" \33[37m/\33[0m ",
   "\33[37m/\33[0m \33[31m/\33[0m",
   " \33[31m/\33[0m "},
  {" \33[31m\\\33[0m ",
   "\33[37m\\\33[0m \33[31m\\\33[0m",
   " \33[37m\\\33[0m "},
  {" \33[31m/\33[0m ",
   "\33[31m/\33[0m \33[37m/\33[0m",
   " \33[37m/\33[0m "},
  {" \33[37m\\\33[0m ",
   "\33[31m\\\33[0m \33[37m\\\33[0m",
   " \33[31m\\\33[0m "},
#else
  {" W ",
   "R R",
   " W "},
  {" R ",
   "W W",
   " R "},
  {" W ",
   "W R",
   " R "},
  {" R ",
   "W R",
   " W "},
  {" R ",
   "R W",
   " W "},
  {" W ",
   "R W",
   " R "}
#endif
};

// bitset for set of different kind of pieces.
using PieceSet = std::bitset<NUM_PIECES>;

using NeighborKey = uint32_t;

// Encode neighboring pieces into key.
NeighborKey EncodeNeighborKey(int right, int top, int left, int bottom) {
  return static_cast<NeighborKey>(
      right + (top << 8) + (left << 16) + (bottom << 24));
}

// Table of possible piece kinds for the certain neighboring piece combination.
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

class Position;

// Hold a move.
struct Move {
  Move() : x(0), y(0), piece(PIECE_EMPTY) {
  }

  // Constructor to create a move from Trax notation.
  // Board position is required because Trax notation is not enough to
  // determine the piece's color.
  Move(const std::string& trax_notation, const Position& previous_position);

  // Constructor to create a move from coordinate and the piece kind.
  // x and y are 0-indexed i.e. if you want to place a piece to the leftmost,
  // you have to set x to -1.
  Move(int x, int y, Piece piece) : x(x), y(y), piece(piece) {
    // Nothing to do.
    assert(-1 <= x && -1 <= y);
  }

  std::string notation() const {
    std::string trax_notation(3, '*');
    assert(x <= 8 && y <= 8);
    if (x == -1) {
      trax_notation[0] = '@';
    } else {
      trax_notation[0] = static_cast<char>(x) + 'A';
    }

    trax_notation[1] = static_cast<char>(y + 1) + '0';
    trax_notation[2] = kPieceNotations[piece];
    return trax_notation;
  }

  int x;
  int y;
  Piece piece;
};

// Hold a board configuration, or Position.
class Position {
 public:
  // Constructor to create empty Trax board.
  Position()
      : board_(nullptr)
      , max_x_(0)
      , max_y_(0)
      , red_to_move_(false)  // White places first.
      , finished_(false)
      , winner_(0) {
  }

  // Disable copy and assign.
  Position(Position&) = delete;
  void operator=(Position) = delete;

  // Destructor.
  ~Position() {
    delete board_;
  }

  // Return possible moves. They may include illegal moves.
  // Only illegal moves that may be included in the list come from
  // forced plays. See FillForcedPieces().
  std::vector<Move> GenerateMoves() const {
    std::vector<Move> moves;

    if (finished_) {
      // This is a finished game.
      return moves;
    }

    // If this is the first step then they are only valid moves.
    if (max_x_ == 0 && max_y_ == 0) {
      moves.emplace_back("@0/", *this);
      moves.emplace_back("@0+", *this);
      return moves;
    }

    // This is very naive implementation but maybe we can progressively
    // update the possible move list (such as using UnionFind tree?)
    for (int i_x = -1; i_x <= max_x_; ++i_x) {
      for (int j_y = -1; j_y <= max_y_; ++j_y) {
        if (at(i_x, j_y) != PIECE_EMPTY) {
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

  // Return true if the move is legal.
  bool DoMove(Move move, Position *next_position) const {
    assert(next_position != nullptr);
    assert(next_position != this);

    // Be aware of the memory leak!
    // TODO(tetsui): this is no good
    delete next_position->board_;

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
    next_position->board_ =
        new Piece[(next_position->max_x_ + 4) * (next_position->max_y_ + 4)];

    // Fill the board with empty pieces.
    std::fill(next_position->board_,
              next_position->board_ +
              (next_position->max_x_ + 4) * (next_position->max_y_ + 4),
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

  // Return set of pieces that are possible to be put on the given coordinate
  // based on neighboring edge colors.
  // It may still return true for illegal moves, because forced play is
  // not considered here.

  PieceSet GetPossiblePieces(int x, int y) const {
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

  // Swap
  void Swap(Position* to) {
    std::swap(board_, to->board_);
    std::swap(max_x_, to->max_x_);
    std::swap(max_y_, to->max_y_);
    std::swap(red_to_move_, to->red_to_move_);
    std::swap(finished_, to->finished_);
    std::swap(winner_, to->winner_);
  }

  // Debug output.
  void Dump() const {
    if (board_ != nullptr) {
      for (int i_y = -1; i_y <= max_y_; ++i_y) {
        for (int j_x = -1; j_x <= max_x_; ++j_x) {
          std::cerr << kPieceNotations[at(j_x, i_y)];
        }
        std::cerr << std::endl;
      }
    }
    std::cerr << std::endl;
  }

  // Larger debug output.
  void LargeDump() const {
    if (board_ != nullptr) {
      for (int i_y = -1; i_y <= max_y_; ++i_y) {
        std::vector<std::string> line(3);

        for (int j_x = -1; j_x <= max_x_; ++j_x) {
          Piece piece = at(j_x, i_y);
          for (int k = 0; k < 3; ++k) {
            line[k] += kLargePieceNotations[piece][k];
          }
        }

        for (int k = 0; k < 3; ++k) {
          std::cerr << line[k] << std::endl;
        }
      }
    }
    std::cerr << std::endl;
  }

  // Return piece kind at the given coordinate.
  // [0, max_x) and [0, max_y) holds, but sentinels are used,
  // so additional bounary access is also allowed.
  const Piece at(int x, int y) const {
    assert(-2 <= x && x < max_x_ + 2 && -2 <= y && y < max_y_ + 2);
    assert((x + 2) * (max_y_ + 4) + (y + 2) >= 0);
    assert((x + 2) * (max_y_ + 4) + (y + 2) < (max_y_ + 4) * (max_x_ + 4));
    assert(board_ != nullptr);
    return board_[(x + 2) * (max_y_ + 4) + (y + 2)];
  }

  int max_x() const { return max_x_; }
  int max_y() const { return max_y_; }

  // Return true if red is the side to move for the next turn.
  bool red_to_move() const { return red_to_move_; }

  // Return true if the game is finished in this position.
  bool finished() const { return finished_; } 

  // Return 1 if red is the winner.
  // Return -1 if white is the winner.
  // Return 0 if the game is tie.
  // It does not return any meaningful value if finished is not yet true.
  int winner() const {
    assert(finished_);
    return winner_;
  }

 private:
  // Fill forced play pieces. Return true if placements are successful,
  // i.e. the position is still legal after forced plays.
  // This also fills winner flags internally.
  // This is only called from DoMove().
  bool FillForcedPieces(int move_x, int move_y) {
    // Fill winner flags based on the position state.
    FillWinnerFlags(move_x, move_y);

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

          // Fill winner flags based on the position state.
          FillWinnerFlags(x, y);

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

    return true;
  }

  // Fill winner flags based on the previously updated piece.
  // Updated variables are finished_ and winner_.
  // This is only called from FillForcedPieces().
  void FillWinnerFlags(int x, int y) {
#if 0
    return;
  }
#else
    if (finished_) {
      return;
    }

    winner_ = 0;
    if (TraceVictoryLineOrLoop(x, y, /* red_line = */ true)) {
      finished_ = true;
      ++winner_;
    }

    if (TraceVictoryLineOrLoop(x, y, /* red_line = */ false)) {
      finished_ = true;
      --winner_;
    }
  }

  // Return true if the line of the given color starts from (x, y)
  // constitutes victory line or loop, i.e. the given color wins.
  bool TraceVictoryLineOrLoop(int start_x, int start_y, bool red_line) {
    assert(at(start_x, start_y) != PIECE_EMPTY);

    const char traced_color = red_line ? 'R' : 'W';

    int line_min_x = start_x;
    int line_min_y = start_y;
    int line_max_x = start_x;
    int line_max_y = start_y;

    // Traces line to two directions with the edges of the given color.
    for (int i = 0; i < 4; ++i) {
      if (kPieceColors[at(start_x, start_y)][i] != traced_color) {
        continue;
      }

      int x = start_x + kDx[i];
      int y = start_y + kDy[i];
      int previous_direction = (i + 2) % 4;

      while (at(x, y) != PIECE_EMPTY) {
        // Updates min / max information.
        line_min_x = std::min(line_min_x, x);
        line_max_x = std::min(line_max_x, x);
        line_min_y = std::min(line_min_y, y);
        line_max_y = std::min(line_max_y, y);

        if (x == start_x && y == start_y) {
          // This is loop.
          return true;
        }

        // Continue tracing the line.
        const int next_direction =
          g_track_direction_table[at(x, y)][previous_direction];
        x += kDx[next_direction];
        y += kDy[next_direction];
        previous_direction = (next_direction + 2) % 4;
      }
    }

    // Check if it constitutes vertical or horizontal victory line.
    return (line_min_x == 0 &&
            line_max_x == max_x_ - 1 &&
            line_max_x - line_min_x + 1 >= 8) ||
           (line_min_y == 0 &&
            line_max_y == max_y_ - 1 &&
            line_max_y - line_min_y + 1 >= 8);
  }
#endif

  // Reference access to board is only allowed to other instances of the class.
  Piece& at(int x, int y) {
    assert(-2 <= x && x < max_x_ + 2 && -2 <= y && y < max_y_ + 2);
    assert((x + 2) * (max_y_ + 4) + (y + 2) >= 0);
    assert((x + 2) * (max_y_ + 4) + (y + 2) < (max_y_ + 4) * (max_x_ + 4));
    assert(board_ != nullptr);
    return board_[(x + 2) * (max_y_ + 4) + (y + 2)];
  }

  Piece* board_;

  // (max_x_ + 4) * (max_y_ + 4) is the size of board_.
  int max_x_;
  int max_y_;

  bool red_to_move_;

  bool finished_;

  int winner_;
};

Move::Move(const std::string& trax_notation, const Position& previous_position)
    : x(0)
    , y(0)
    , piece(PIECE_EMPTY) {
  assert(trax_notation.size() == 3);

  if (trax_notation[0] == '@') {
    x = -1;
  } else {
    x = static_cast<int>(trax_notation[0] - 'A');
  }

  y = static_cast<int>(trax_notation[1] - '0') - 1;

  // If this is the first move, then the color is limited. 
  if (previous_position.max_x() == 0 && previous_position.max_y() == 0) {
    assert(trax_notation[2] == '/' || trax_notation[2] == '+');
    if (trax_notation[2] == '/') {
      piece = PIECE_RWWR;
    } else if (trax_notation[2] == '+') {
      piece = PIECE_RWRW;
    }
  } else {
    PieceSet candidates = previous_position.GetPossiblePieces(x, y);

    // Exclude EMPTY piece for added piece candidates.
    candidates.reset(PIECE_EMPTY);

    for (int i = 0; i < NUM_PIECES; ++i) {
      if (kPieceNotations[i] == trax_notation[2] &&
          candidates.test(i)) {
        piece = static_cast<Piece>(i);
        break;
      }
    }
  }
}

// Enumerate all possible positions within the given depth.
int DoPerft(const Position& position, int depth) {
  // position.LargeDump();
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

void Perft() {
  Position position;
  for (int i_depth = 0; i_depth < 10; ++i_depth) {
    std::cerr
      << "depth: " << i_depth
      << " leaves: " << DoPerft(position, i_depth) << std::endl;
  }
}

class Searcher {
 public:
  virtual Move SearchBestMove(const Position& position) = 0;
  virtual ~Searcher() {
  };
};

class NegaMaxSearcher : public Searcher {
 public:
  NegaMaxSearcher(int max_depth) : max_depth_(max_depth) {
  }

  virtual Move SearchBestMove(const Position& position) {
    int max_score = -kInf;
    std::vector<std::pair<int, Move>> moves;

    for (auto&& move : position.GenerateMoves()) {
      Position next_position;
      if (!position.DoMove(move, &next_position)) {
        // This is illegal move.
        continue;
      }

      const int score = NegaMax(next_position, max_depth_);
      moves.emplace_back(score, move);
    }

    std::sort(moves.begin(), moves.end(),
              [](std::pair<int, Move>& lhs, std::pair<int, Move>& rhs) {
                return lhs.first < rhs.first;
              });

    for (auto&& score_move : moves) {
      std::cerr << score_move.first << " "
        << score_move.second.notation() << std::endl;
    }

    return moves.back().second;
  }

 private:
  int NegaMax(const Position& position,
              int depth, int alpha=-kInf, int beta=kInf) {
    int max_score = -kInf;
    if (position.finished()) {
      // Think the case where position.red_to_move() == true in SearchBestMove.
      // red_to_move() == false in NegaMax.
      // NegaMax should return positive score for winner() == 1.
      if (position.red_to_move()) {
        return kInf * -position.winner();
      } else {
        return kInf * position.winner();
      }
    }

    assert(depth >= 0);
    if (depth == 0) {
      return 0;
#if 0
      int red_sub_white = 0;
      for (auto&& move : position.GenerateMoves()) {
        Position next_position;
        if (!position.DoMove(move, &next_position)) {
          // This is illegal move.
          continue;
        }

        if (next_position.finished()) {
          if (next_position.red_winner()) {
            ++red_sub_white;
          } else {
            --red_sub_white;
          }
        }
      }
      if (position.red_to_move()) {
        red_sub_white = -red_sub_white;
      }
      return red_sub_white;
#endif
    }

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
    return max_score;
  }

  int max_depth_;
};

class RandomSearcher : public Searcher {
 public:
  virtual Move SearchBestMove(const Position& position) {
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
};

// Return true if red is the winner.
int StartSelfGame(Searcher* white_searcher, Searcher* red_searcher,
                  bool verbose=false) {
  assert(white_searcher != nullptr && red_searcher != nullptr);
  Position position;

  int step = 0;
  while (!position.finished()) {
    if (verbose) {
      std::cerr << "Step " << step << ":" << std::endl;
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
      position.LargeDump();
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
    std::cerr
      << result << " in total "
      << step << " steps. " << std::endl;
  }

  return position.winner();
}

// Start Trax client which connects through stdin / stdout.
void StartTraxClient(Searcher* searcher) {
  assert(searcher != nullptr);

  bool success = false;

  Position position;
  Position next_position;

  std::string command;

  while (true) {
    std::cin >> command;

    if (command == "-T") {
      std::cout << kPlayerId << " ";
      continue;
    }

    if (command == "-B") {
      // Nothing to do for now.
      continue;
    }

    // Skip Trax notation read if you put the move first.
    if (command != "-W") {
      // Otherwise command is raw Trax notation. Apply the received trax move.
      success = position.DoMove(Move(command, position), &next_position);
      assert(success);
      position.Swap(&next_position);

      // The game is finished by the opponent's move.
      if (position.finished()) {
        break;
      }
    }

    // Search the best move.
    Move best_move = searcher->SearchBestMove(position);
    success = position.DoMove(Move(command, position), &next_position);
    assert(success);

    // Apply the move.
    position.Swap(&next_position);

    // Send the move back to stdout.
    std::cout << best_move.notation() << " ";

    // The game is finished by our move.
    if (position.finished()) {
      break;
    }
  }
}

int main(int argc, char *argv[]) {
  // Otherwise Position::GetPossiblePieces() doesn't work.
  GeneratePossiblePiecesTable();
  // Otherwise Position::TraceVictoryLineOrLoop() doesn't work.
  GenerateTrackDirectionTable();

  NegaMaxSearcher negamax_searcher(2);
  RandomSearcher random_searcher;

#if 0
  if (argc > 1) {
    for (int i = 0; i < atoi(argv[1]); ++i) {
      Random();
    }
  }

  StartSelfGame(&negamax_searcher, &random_searcher, /* verbose = */ true);
#endif

#if 1
  int white = 0, red = 0;
  for (int i = 0; i < 20; ++i) {
    // const int result = StartSelfGame(&random_searcher, &random_searcher);
    const int result = StartSelfGame(&negamax_searcher, &random_searcher,
                                     /* verbose = */ true);
    if (result > 0) {
      ++red;
    } else if (result < 0) {
      ++white;
    }
  }
  std::cerr << "white: " << white << " red: " << red << " "
    << (white * 100.0 / (white + red)) << "%" << std::endl;

#endif

  // Perft();
#if 0
  Position position;

  std::vector<std::string> notations = {"@0+", "B1+", "A2\\"};
  for (auto&& notation : notations) {
    Position next_position;
    bool success = position.DoMove(Move(notation, position), &next_position);
    assert(success);
    position.Swap(&next_position);
    position.LargeDump();
  }
#endif
  return 0;
}
