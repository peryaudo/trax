#ifndef TRAX_TRAX_H_
#define TRAX_TRAX_H_

#include <bitset>
#include <cassert>
#include <cstdint>
#include <iostream>
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
  {". .",
   " . ",
   ". ."},
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

// Move with score.
// For sorting possible moves with their scores through search.
// Named ExtMove in Stockfish (and YaneuraOu.)
struct ScoredMove {
  int score;
  Move move;
  ScoredMove(int score, Move move) : score(score), move(move) {
  }

  const bool operator<(const ScoredMove& rhs) const {
    return score < rhs.score;
  }

  // Implicit type conversion to Move is allowed.
  operator Move() const { return move; }
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
      , red_winner_(false)
      , white_winner_(false) {
  }

// #ifdef DISABLE_COPYABLE_POSITION
#if 1
  // Disable copy and assign.
  Position(Position&) = delete;
  void operator=(Position) = delete;

  // Disable comparison.
  const bool operator==(const Position& to) = delete;

#else
  // Copy constructor.
  // It is not recommended to copy Position object because it is enough large
  // but it is unavoidable when implementing transposition table.
  Position(const Position& position)
      : board_(nullptr)
      , max_x_(position.max_x_)
      , max_y_(position.max_y_)
      , red_to_move_(position.red_to_move_)
      , finished_(position.finished_)
      , winner_(position.winner_) {
    if (position.board_ != nullptr) {
      board_ = new Piece[position.board_size()];
      std::copy(position.board_, position.board_ + position.board_size(),
                board_);
    }
  }

  void operator=(const Position& position) {
    delete[] board_;
    board_ = nullptr;

    max_x_ = position.max_x_;
    max_y_ = position.max_y_;
    red_to_move_ = position.red_to_move_;
    finished_ = position.finished_;
    winner_ = position.winner_;
    if (position.board_ != nullptr) {
      board_ = new Piece[position.board_size()];
      std::copy(position.board_, position.board_ + position.board_size(),
                board_);
    }
  }
  
  // Compare two Position objects.
  const bool operator==(const Position& to) const {
    if (max_x_ != to.max_x_ ||
        max_y_ != to.max_y_ ||
        red_to_move_ != to.red_to_move_) {
      return false;
    }

    for (int i = 0; i < to.board_size(); ++i) {
      if (board_[i] != to.board_[i]) {
        return false;
      }
    }

    return true;
  }
#endif

  // Destructor.
  ~Position() {
    delete[] board_;
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
    const PositionHash kPrime = 100000007ULL;

    PositionHash result = 0;
    result += red_to_move_;
    result *= kPrime;
    result += max_x_;
    result *= kPrime;
    result += max_y_;

    for (int i_x = 0; i_x < max_x_; ++i_x) {
      for (int j_y = 0; j_y < max_y_; ++j_y) {
        result *= kPrime;
        result += at(i_x, j_y);
      }
    }
    return result;
  }

  // Return all the hashes of isomorphic positions.
  // For example, rotated positions are isomorphic.
  // If flip is true, it returns isomorphic hashes with flipped turn to move.
  std::vector<PositionHash> Hashes(bool flip) const {
    // TODO(tetsui): implement
    return std::vector<PositionHash>();
  }

  // Swap
  void Swap(Position* to) {
    std::swap(board_, to->board_);
    std::swap(max_x_, to->max_x_);
    std::swap(max_y_, to->max_y_);
    std::swap(red_to_move_, to->red_to_move_);
    std::swap(red_winner_, to->red_winner_);
    std::swap(white_winner_, to->white_winner_);
  }

  // Debug output.
  void Dump() const;

  // Return piece kind at the given coordinate.
  // [0, max_x) and [0, max_y) holds, but sentinels are used,
  // so additional bounary access [-2, +2] is also allowed.
  const Piece at(int x, int y) const {
    assert(-2 <= x && x < max_x_ + 2 && -2 <= y && y < max_y_ + 2);
    assert((x + 2) * (max_y_ + 4) + (y + 2) >= 0);
    assert((x + 2) * (max_y_ + 4) + (y + 2) < (max_y_ + 4) * (max_x_ + 4));
    assert(board_ != nullptr);
    return board_[(x + 2) * (max_y_ + 4) + (y + 2)];
  }

  int max_x() const { return max_x_; }
  int max_y() const { return max_y_; }

  // Return true if red is the side to move for the NEXT turn,
  // i.e. if red_to_move() == true, the last player that put a piece is white.
  //
  // This is formal definition, but simply you can think you are red
  // if you are passed Position object with red_to_move() == true.
  //
  // FYI: white is the first player to put piece, so red_to_move() == false in
  // the beggining.
  bool red_to_move() const { return red_to_move_; }

  // Return true if the game is finished in this position.
  bool finished() const { return red_winner_ || white_winner_; }

  // Return 1 if red is the winner.
  // Return -1 if white is the winner.
  // Return 0 if the game is tie.
  //
  // It always return 0 if finished is not yet true, but the method is
  // not supposed to be used for checking if the game is finished.
  int winner() const {
    return static_cast<int>(red_winner_) - static_cast<int>(white_winner_);
  }

 private:
  // Fill forced play pieces. Return true if placements are successful,
  // i.e. the position is still legal after forced plays.
  // This also fills winner flags internally.
  // This is only called from DoMove().
  bool FillForcedPieces(int move_x, int move_y);

  // Fill winner flags based on the previously updated piece.
  // Updated variables are red_winner_ and white_winner_.
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

  // Board array size including sentinels. Used internally.
  int board_size() const {
    return (max_x_ + 4) * (max_y_ + 4);
  }

  Piece* board_;

  int max_x_;
  int max_y_;

  bool red_to_move_;

  bool red_winner_;
  bool white_winner_;
};

#ifndef DISABLE_COPYABLE_POSITION
namespace std {
  template<>
  struct hash<Position>
  {
    std::size_t operator()(const Position& position) const {
      return position.Hash();
    }
  };
}  // namespace std
#endif

class Searcher {
 public:
  // Return the best move from the position, from the perspective of
  // position.red_to_move(), or more simply, you are red inside the method
  // if position.red_to_move() == true.
  virtual Move SearchBestMove(const Position& position) = 0;
  virtual ~Searcher() {
  };

  // Searcher name that is shown in the debug messages of
  // StartSelfGame() and StartTraxClient().
  // Supposed to describe important configuration information of the searcher,
  // such as the depth of the search, etc.
  virtual std::string name() = 0;
};

// Return 1 if red is the winner.
// Return -1 if white is the winner.
// Return 0 if the game is tie.
int StartSelfGame(Searcher* white_searcher, Searcher* red_searcher,
                  bool verbose=false);

void StartMultipleSelfGames(Searcher* white_searcher, Searcher* red_searcher,
                            int num_games, bool verbose=false);

// Start Trax client which connects through stdin / stdout.
void StartTraxClient(Searcher* searcher);

// Do performance testing by counting all the possible moves
// within the given depth.
void Perft(int max_depth);

#endif // TRAX_TRAX_H_

