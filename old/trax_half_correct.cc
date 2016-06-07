#include <algorithm>
#include <bitset>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

// TODO(tetsui): Maybe this is upside down. Need careful fix.

const int kDx[] = {1, 0, -1, 0};
const int kDy[] = {0, 1, 0, -1};

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

//  |
// -+-
//  |
// 
//  /
// / /
//  / 
//
//  \
// \ \
//  \
//

// bitset for set of different kind of pieces.
using PieceSet = std::bitset<NUM_PIECES>;

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
  /*
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
   */
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
};

class Position;

// Hold a move.
struct Move {
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

  int x, y;
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
      , red_winner_(false) {
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
    if (!next_position->FillForcedPieces()) {
      return false;
    }

    // Fill winner flags based on the position state.
    next_position->FillWinnerFlags(move.x + offset_x, move.y + offset_y);

    return true;
  }

  // Return set of pieces that are possible to be put on the given coordinate
  // based on neighboring edge colors.
  // It may still return true for illegal moves, because forced play is
  // not considered here.

  // TODO(tetsui): This method can be completely memoized.
  PieceSet GetPossiblePieces(int x, int y) const {
    const char *neighbors[4];
    PieceSet neighbor_kinds;
    for (int i = 0; i < 4; ++i) {
      const int nx = x + kDx[i];
      const int ny = y + kDy[i];
      neighbors[i] = kPieceColors[at(nx, ny)];
      neighbor_kinds.set(at(nx, ny));
    }

    PieceSet pieces;
    // Empty piece is always legal.
    pieces.set(PIECE_EMPTY);

    // If all the neighboring pieces are empty then empty piece is the only
    // legal piece.
    if (neighbor_kinds.count() == 1 && neighbor_kinds.test(PIECE_EMPTY)) {
      return pieces;
    }

    // Exclude empty piece for this loop.
    for (int i_candidate = 1; i_candidate < NUM_PIECES; ++i_candidate) {
      const char *candidate = kPieceColors[i_candidate];

      bool valid = true;
      for (int j = 0; j < 4; ++j) {
        // If the neighboring cell isn't empty,
        // and the color of the shared edges are different, 
        // then the candidate is invalid piece placement.
        if (neighbors[j][0] != '_' &&
            candidate[j] != neighbors[j][(j + 2) % 4]) {
          valid = false;
          break;
        }
      }

      if (valid) {
        pieces.set(i_candidate);
      }
    }

    return pieces;
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

  // Return true if red is the winner.
  // It does not return any meaningful value if finished is not yet true.
  bool red_winner() const { return red_winner_; }

 private:
  // Fill forced play pieces. Return true if placements are successful,
  // i.e. the position is still legal after forced plays.
  // This is only called from DoMove().
  bool FillForcedPieces() {
    // TODO(tetsui): This is too naive and inefficient.
    bool updated = true;

    // Loop while chain of forced plays is happening.
    while (updated) {
      updated = false;

      // Forced plays should not happen outside the current board region.
      // Therefore checking inside [0, max_x) [0, max_y) is enough.
      for (int i_x = 0; i_x < max_x_; ++i_x) {
        for (int j_y = 0; j_y < max_y_; ++j_y) {
          if (at(i_x, j_y) != PIECE_EMPTY) {
            continue;
          }

          PieceSet pieces = GetPossiblePieces(i_x, j_y);
          // No possible piece including empty one for the location.
          // The position is invalid.
          if (pieces.count() == 0) {
            return false;
          }

          if (pieces.count() == 1) {
            // Not all place with only one possible piece is forced play.

            // TODO(tetsui): This can also be implemented as a table like
            // GetPossiblePieces().
            int red_count = 0;
            int white_count = 0;

            for (int k = 0; k < 4; ++k) {
              const int nx = i_x + kDx[k];
              const int ny = j_y + kDy[k];
              Piece neighbor = at(nx, ny);
              if (neighbor == PIECE_EMPTY) {
                continue;
              }

              if (kPieceColors[neighbor][(k + 2) % 4] == 'R') {
                ++red_count;
              } else {
                ++white_count;
              }
            }

            // This is forced play.
            if (red_count >= 2 || white_count >= 2) {
              updated = true;
              for (int k = 1; k < NUM_PIECES; ++k) {
                if (pieces.test(k)) {
                  at(i_x, j_y) = static_cast<Piece>(k);
                  break;
                }
              }
            }
          }
        }
      }
    }

    return true;
  }

  // Fill winner flags based on the previously updated piece.
  // Updated variables are finished_ and red_winner_.
  // This is only called from DoMove().
  void FillWinnerFlags(int x, int y) {
    if (TraceVictoryLineOrLoop(x, y, /* red_line = */ true)) {
      finished_ = true;
      red_winner_ = true;
      return;
    }

    if (TraceVictoryLineOrLoop(x, y, /* red_line = */ false)) {
      finished_ = true;
      red_winner_ = false;
      return;
    }
  }

  // Return true if line starts from (x, y) determines the given color wins.
  bool TraceVictoryLineOrLoop(int x, int y, bool red_line) {
    // TODO(tetsui): FIXME UNIMPLEMENTED
    return false;
  }

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
  int max_x_, max_y_;

  bool red_to_move_;

  bool finished_;

  bool red_winner_;
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
      if (kPieceNotations[i] != trax_notation[2] &&
          candidates.test(i)) {
        piece = static_cast<Piece>(i);
        break;
      }
    }
  }
}

// Enumerate all possible positions within the given depth.
int DoPerft(const Position& position, int depth) {
  position.LargeDump();
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
  for (int i_depth = 0; i_depth < 4; ++i_depth) {
    std::cerr
      << "depth: " << i_depth
      << " leaves: " << DoPerft(position, i_depth) << std::endl;
  }
}

int main(int argc, char *argv[]) {
  Perft();
  return 0;
}
