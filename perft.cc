// Copyright (C) 2016 Tetsui Ohkubo.

#include "./perft.h"

#include "./timer.h"
#include "./trax.h"

// Enumerate all possible positions within the given depth.
int Perft(const Position& position, int depth, Timer *timer) {
  // position.Dump();
  if (depth <= 0) {
    timer->IncrementNodeCounter();
    return 1;
  }

  int total_positions = 0;

  for (Move move : position.GenerateMoves()) {
    Position next_position;
    if (!position.DoMove(move, &next_position)) {
      // The move was illegal.
      continue;
    }
    total_positions += Perft(next_position, depth - 1, timer);
  }
  return total_positions;
}

int Perft(int depth, Timer* timer) {
  Position position;
  return Perft(position, depth, timer);
}

void ShowPerft(int max_depth) {
  uint64_t sum_leaves = 0;
  for (int i_depth = 0; i_depth <= max_depth; ++i_depth) {
    Timer timer;
    const int leaves = Perft(i_depth, &timer);
    timer.CheckTimeout();
    sum_leaves += leaves;

    std::cerr
      << "Depth: " << i_depth << " leaves: " << leaves
      << " Time: " << timer.elapsed_ms() << "ms"
      << " Speed: " << timer.nps() << " node/s " << std::endl;
  }
}
