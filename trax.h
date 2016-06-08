#ifndef TRAX_TRAX_H_
#define TRAX_TRAX_H_

#include <bitset>
#include <cassert>
#include <cstdint>
#include <queue>
#include <unordered_map>
#include <vector>

// Infinite.
static const int kInf = 1000000000;

// Be aware that pieces are defined in terms of anti-clockwise edge colors,
// while y-axis of board index is flipped against mathematical definition.
static const int kDx[] = {1, 0, -1, 0};
static const int kDy[] = {0, -1, 0, 1};

// Xorshift 128.
uint32_t Random();

// Should be called before Position::GetPossiblePieces().
void GeneratePossiblePiecesTable();

// Should be called before Position::TraceVictoryLineOrLoop().
void GenerateTrackDirectionTable();

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

// bitset for set of different kind of pieces.
using PieceSet = std::bitset<NUM_PIECES>;

// Edge colors of the pieces as explained above.
static const char* kPieceColors[] = {
  "____",
  "RWRW",
  "WRWR",
  "RWWR",
  "RRWW",
  "WRRW",
  "WWRR"
};

// Trax notations of the pieces.
static const char kPieceNotations[] = ".++/\\/\\";

static const char kLargePieceNotations[][3][32] = {
  {"...",
   "...",
   "..."},
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
};

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

  std::string notation() const;

  int x;
  int y;
  Piece piece;
};

// Integer hash of position. Can be used for transposition table, etc.
// Needless to say, user must care about conflicts.
using PositionHash = uint64_t;

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
  std::vector<Move> GenerateMoves() const;

  // Return true if the move is legal.
  bool DoMove(Move move, Position *next_position) const;

  // Return set of pieces that are possible to be put on the given coordinate
  // based on neighboring edge colors.
  // It may still return true for illegal moves, because forced play is
  // not considered here.
  PieceSet GetPossiblePieces(int x, int y) const;

  // Hash current board configuration.
  // Current implementation uses simple rolling hash.
  PositionHash Hash() const {
    const PositionHash kPrime = 100000007;
    PositionHash result = red_to_move_;
    for (int i_x = 0; i_x < max_x_; ++i_x) {
      for (int j_y = 0; j_y < max_y_; ++j_y) {
        result *= kPrime;
        result += at(i_x, j_y);
      }
    }
    return result;
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
  void Dump() const;

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
  bool FillForcedPieces(int move_x, int move_y);

  // Fill winner flags based on the previously updated piece.
  // Updated variables are finished_ and winner_.
  // This is only called from FillForcedPieces().
  void FillWinnerFlags(int x, int y);

  // Return true if the line of the given color starts from (x, y)
  // constitutes victory line or loop, i.e. the given color wins.
  bool TraceVictoryLineOrLoop(int start_x, int start_y, bool red_line);

  // Reference access to board is only allowed from other instances of the
  // class.
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

class Searcher {
 public:
  virtual Move SearchBestMove(const Position& position) = 0;
  virtual ~Searcher() {
  };
};

void StartMultipleSelfGames(Searcher* white_searcher, Searcher* red_searcher,
                            int num_games, bool verbose=false);


class RandomSearcher : public Searcher {
 public:
  virtual Move SearchBestMove(const Position& position);
};

class NegaMaxSearcher : public Searcher {
 public:
  // depth=2: white: 146 red: 54 73%
  // depth=3: white: 153 red: 47 76.5%
  NegaMaxSearcher(int max_depth) : max_depth_(max_depth) {
  }

  virtual Move SearchBestMove(const Position& position);

 private:
  int NegaMax(const Position& position,
              int depth, int alpha=-kInf, int beta=kInf);

  int max_depth_;
  std::unordered_map<PositionHash, int> transposition_table_;
};

#endif // TRAX_TRAX_H_

