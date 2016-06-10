// Copyright (C) 2016 Tetsui Ohkubo.

#include "./perft.h"

#ifdef __MACH__
#include <mach/mach_time.h>
#endif

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
    // TODO(tetsui): Implement using clock_gettime(CLOCK_MONOTONIC) for Linux

#ifdef __MACH__
    uint64_t before = mach_absolute_time();
#endif

    const int leaves = Perft(i_depth);
    sum_leaves += leaves;

    std::cerr
      << "depth: " << i_depth << " leaves: " << leaves << std::endl;

#ifdef __MACH__
    uint64_t after = mach_absolute_time();

    mach_timebase_info_data_t base;
    mach_timebase_info(&base);
    uint64_t elapsed = (after - before) / base.denom / 1000 / 1000;

    std::cerr << "time: " << elapsed << " ms ";

    if (elapsed == 0) {
      std::cerr << "nps: Inf" << std::endl;
    } else {
      uint64_t nps = sum_leaves * 1000 / elapsed;
      std::cerr << "nps: " << nps << std::endl;
    }
#endif
  }
}
