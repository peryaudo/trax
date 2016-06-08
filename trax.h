#ifndef TRAX_TRAX_H_
#define TRAX_TRAX_H_

#include <cstdint>
#include <unordered_map>

// Infinite.
static const int kInf = 1000000000;

// Xorshift 128.
uint32_t Random();

// Should be called before Position::GetPossiblePieces().
void GeneratePossiblePiecesTable();

// Should be called before Position::TraceVictoryLineOrLoop().
void GenerateTrackDirectionTable();

struct Move;

class Position;

// Integer hash of position. Can be used for transposition table, etc.
// Needless to say, user must care about conflicts.
using PositionHash = uint64_t;

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

