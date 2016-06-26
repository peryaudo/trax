// Copyright (C) 2016 Tetsui Ohkubo.

#include "./perft.h"

#include "./timer.h"
#include "./trax.h"

// Enumerate all possible positions within the given depth.
int Perft(const Position& position, int depth) {
  // position.Dump();
  if (depth <= 0) {
    return 1;
  }

  int total_positions = 0;

  for (Move move : position.GenerateMoves()) {
    Position next_position;
    if (!position.DoMove(move, &next_position)) {
      // The move was illegal.
      continue;
    }
    total_positions += Perft(next_position, depth - 1);
  }
  return total_positions;
}

int Perft(int depth) {
  Position position;
  return Perft(position, depth);
}

void ShowPerft(int max_depth) {
  uint64_t sum_leaves = 0;
  for (int i_depth = 0; i_depth <= max_depth; ++i_depth) {
    Timer timer;
    const int leaves = Perft(i_depth);
    timer.CheckTimeout();
    sum_leaves += leaves;

    std::cerr
      << "depth: " << i_depth << " leaves: " << leaves
      << " time: " << timer.elapsed_ms() << "ms"
      << " nps: " << timer.nps() << std::endl;
  }
}
