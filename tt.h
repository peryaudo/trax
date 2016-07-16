// Copyright (C) 2016 Tetsui Ohkubo.

#ifndef TT_H_
#define TT_H_

#include <mutex>   // NOLINT
#include <thread>  // NOLINT

#include "./trax.h"

// Transposition Table boundary flags for combination with alpha-beta pruning.
enum Bound {
  BOUND_NONE = 0,
  BOUND_LOWER,
  BOUND_UPPER,
  BOUND_EXACT
};

class TranspositionTable {
 public:
  TranspositionTable();
  ~TranspositionTable();

  void NewSearch() {
    ++generation_;
  }

  // Entry of Transposition Table.
  //
  // See also:
  //
  // https://en.wikipedia.org/wiki/Negamax
  // #Negamax_with_alpha_beta_pruning_and_transposition_tables
  //
  // https://groups.google.com/forum/#!msg/
  // rec.games.chess.computer/p8GbiiLjp0o/81vZ3czsthIJ
  struct Entry {
    PositionHash key;
    Move best_move;
    int score;

    int generation;
    int depth;
    Bound bound;

    Entry()
        : key(0)
        , best_move()
        , score(0)
        , generation(0)
        , depth(0)
        , bound(BOUND_NONE) {
    }
  };

  // Return true if found.
  bool Probe(PositionHash key, TranspositionTable::Entry *entry) const;

  void Store(PositionHash key, Move best_move,
             int score, int depth, Bound bound);

  static const int kClusterSize = 4;

  // Clustered transposition table. See Stockfish or YaneuraOu.
  struct Cluster {
    // TODO(tetsui): Consider using alignas.
    Entry entries[kClusterSize];
  };

 private:
  int mask_;
  Cluster* table_;
  int generation_;

  mutable std::mutex mutex_;
};

#endif  // TT_H_
