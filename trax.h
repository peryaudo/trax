// Copyright (C) 2016 Tetsui Ohkubo.

#ifndef TRAX_H_
#define TRAX_H_

#include <algorithm>
#include <bitset>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <queue>
#include <string>
#include <utility>
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

// Should be called before Position::FillForcedPieces().
void GenerateForcedPlayTable();

// Piece kinds. It includes color information so it is more specific than
// Trax notation. The alphabets after the prefix specify colors on the edges
// in anti-clockwise order from the rightmost one.
enum Piece {
  PIECE_EMPTY = 0,

  //  W
  // R R
  //  W
  PIECE_RWRW,

  //  R
  // W W
  //  R
  PIECE_WRWR,

  //  W
  // W R
  //  R
  PIECE_RWWR,

  //  R
  // W R
  //  W
  PIECE_RRWW,

  //  R
  // R W
  //  W
  PIECE_WRRW,

  //  W
  // R W
  //  R
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

// Denote a move.
struct Move {
  Move() : x(0), y(0), piece(PIECE_EMPTY) {
  }

  // Constructor that internally calls Parse().
  // It immediately quits the program instead of returning error.
  Move(const std::string& trax_notation, const Position& previous_position);

  // Constructor to create a move from coordinate and the piece kind.
  // x and y are 0-indexed i.e. if you want to place a piece to the leftmost,
  // you have to set x to -1.
  Move(int x, int y, Piece piece) : x(x), y(y), piece(piece) {
    // Nothing to do.
    assert(-1 <= x && -1 <= y);
  }

  // Parse a move from Trax notation.
  // Board position is required because Trax notation is not enough to
  // determine the piece's color.
  // Return true if the parsing is successful.
  bool Parse(const std::string& trax_notation,
             const Position& previous_position);

  const bool operator==(const Move to) const {
    return x == to.x && y == to.y && piece == to.piece;
  }

  // Return Trax notation of the move.
  std::string notation() const;

  // Use bit field so that Move struct will fit into 4 bytes.
  // I'm not sure if it is very effective but at least it's not harmful and
  // slight performance improvement in perft is achieved.
  int x : 14;
  int y : 14;
  Piece piece : 4;
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

// Reason of win.
enum WinningReason {
  WINNING_REASON_UNKNOWN = 0,
  WINNING_REASON_LOOP,
  WINNING_REASON_LINE,

  // The game is draw because the board is full. Only possible in 8x8 trax.
  WINNING_REASON_FULL,

  // Only possible in human played games.
  WINNING_REASON_RESIGN
};

// Denotes decompoesed line information returned by Position::EnumerateLines().
struct Line {
  Line() : is_red(false), endpoint_distance(0) {
    edge_distances[0] = 0;
    edge_distances[1] = 0;
  }

  Line(const std::pair<int, int>& endpoint_a,
       const std::pair<int, int>& endpoint_b,
       bool is_red,
       const Position& position,
       const std::map<std::pair<int, int>, int>& indexed_edges);

  bool is_mate() const {
    return (endpoint_distance <= 2 ||
            edge_distances[0] <= 1 ||
            edge_distances[1] <= 1);
  }

  bool is_red;
  int edge_distances[2];
  int endpoint_distance;
};

// Integer hash of position. Can be used for transposition table, etc.
// Needless to say, user must care about conflicts.
using PositionHash = uint64_t;

const PositionHash kPositionHashPrimeA = 100000007ULL;
const PositionHash kPositionHashPrimeB = 100000037ULL;

// Denote a board configuration, or Position.
class Position {
 public:
  // Constructor to create empty Trax board.
  Position()
      : board_(nullptr)
      , max_x_(0)
      , max_y_(0)
      , red_to_move_(false)  // White places first.
      , red_winner_(false)
      , white_winner_(false)
      , red_longest_(0)
      , white_longest_(0)
      , has_loop_(false)
      , has_victory_line_(false) {
  }

  // Disable copy and assign.
  Position(Position&) = delete;
  void operator=(Position) = delete;

  // Disable comparison.
  const bool operator==(const Position& to) = delete;

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
  PositionHash Hash(PositionHash hash_prime) const {
    PositionHash result = 0;
    result += red_to_move_;
    result *= hash_prime;
    result += max_x_;
    result *= hash_prime;
    result += max_y_;

    for (int i_x = 0; i_x < max_x_; ++i_x) {
      for (int j_y = 0; j_y < max_y_; ++j_y) {
        result *= hash_prime;
        result += at(i_x, j_y);
      }
    }
    return result;
  }

  void EnumerateLines(std::vector<Line> *lines) const;

  // Swap
  void Swap(Position* to) {
    std::swap(board_, to->board_);
    std::swap(max_x_, to->max_x_);
    std::swap(max_y_, to->max_y_);
    std::swap(red_to_move_, to->red_to_move_);
    std::swap(red_winner_, to->red_winner_);
    std::swap(white_winner_, to->white_winner_);
    std::swap(red_longest_, to->red_longest_);
    std::swap(white_longest_, to->white_longest_);
    std::swap(has_loop_, to->has_loop_);
    std::swap(has_victory_line_, to->has_victory_line_);
  }

  void Clear() {
    Position position;
    Swap(&position);
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
  // Return 0 if the game is draw (only happens in 8x8Trax.)
  //
  // It always return 0 if finished is not yet true, but the method is
  // not supposed to be used for checking if the game is finished.
  int winner() const {
    return static_cast<int>(red_winner_) - static_cast<int>(white_winner_);
  }

  // Length of the longest red line.
  int red_longest() const { return red_longest_; }
  // Length of the longest white line.
  int white_longest() const { return white_longest_; }

  // TODO(tetsui): implement.
  WinningReason winning_reason() const {
    assert(false);
    return WINNING_REASON_UNKNOWN;
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

  void TraceLineToEndpoints(int x, int y, bool red_line,
                            std::pair<int, int> *endpoint_a,
                            std::pair<int, int> *endpoint_b) const;

  void TraceAndIndexEdges(
      int start_x, int start_y,
      std::map<std::pair<int, int>, int> *indexed_edges) const;

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

  int red_longest_;
  int white_longest_;

  bool has_loop_;
  bool has_victory_line_;
};

// Base abstract class for searchers.
// The interface may be subject to change.
class Searcher {
 public:
  // Return the best move from the position, from the perspective of
  // position.red_to_move(), or more simply, you are red inside the method
  // if position.red_to_move() == true.
  virtual Move SearchBestMove(const Position& position) = 0;
  virtual ~Searcher() {
  }

  // Searcher name that is shown in the debug messages of
  // StartSelfGame() and StartTraxClient().
  // Supposed to describe important configuration information of the searcher,
  // such as the depth of the search, etc.
  virtual std::string name() = 0;
};

// Start Trax client which connects through stdin / stdout.
void StartTraxClient(Searcher* searcher);

// Uniformly denote both human played games and computer generated games.
struct Game {
  Game()
      : moves()
      , comments()
      , winner(0)
      , winning_reason(WINNING_REASON_UNKNOWN) {
  }

  void Clear() {
    moves.clear();
    comments.clear();
    winner = 0;
    winning_reason = WINNING_REASON_UNKNOWN;
  }

  // void Dump();

  // Count number of matching moves between the actual game and the searcher.
  int CountMatchingMoves(Searcher *searcher);

  // Continue the game whose WinningReason is WINNING_REASON_RESIGN using
  // self play using the searcher and determine if the game is
  // WINNING_REASON_LOOP or WINNING_REASON_LINE.
  // void ContinueBySearcher(Searcher *searcher);

  int num_moves() { return moves.size(); }

  // Moves.
  std::vector<Move> moves;

  // The size is zero if the game does not have comment,
  // otherwise the size is the same as plays.
  std::vector<std::string> comments;

  // Return 1 if red is the winner.
  // Return -1 if white is the winner.
  // Return 0 if the game is draw (only happens in 8x8Trax.)
  int winner;

  // Winning reason such as loop or line.
  WinningReason winning_reason;
};

// Start the self game between white_searcher and red_searcher and
// return its result to game_result.
void StartSelfGame(Searcher* white_searcher, Searcher* red_searcher,
                   Game *game_result, bool verbose = false);

void StartMultipleSelfGames(Searcher* white_searcher, Searcher* red_searcher,
                            int num_games, bool verbose = false);

// Parse commented games.
// High quality commented game data can be obtained from
// http://www.traxgame.com/games_comment.php .
void ParseCommentedGames(const std::string& filename,
                         std::vector<Game> *games);

#endif  // TRAX_H_
