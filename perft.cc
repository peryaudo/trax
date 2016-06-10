#include "perft.h"

#ifdef __MACH__
#include <mach/mach_time.h>
#endif

#include "gflags/gflags.h"
#include "trax.h"

// Enumerate all possible positions within the given depth.
int Perft(const Position& position, int depth) {
  // position.Dump();
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
    total_positions += Perft(next_position, depth - 1);
  }
  return total_positions;
}

int Perft(int depth) {
  Position position;
  return Perft(position, depth);
}

void ShowPerft(int max_depth) {
  for (int i_depth = 0; i_depth <= max_depth; ++i_depth) {
    // TODO(tetsui): Implement using clock_gettime(CLOCK_MONOTONIC) for Linux

#ifdef __MACH__
    uint64_t before = mach_absolute_time();
#endif

    std::cerr
      << "depth: " << i_depth << " leaves: " << Perft(i_depth) << std::endl;

#ifdef __MACH__
    uint64_t after = mach_absolute_time();

    mach_timebase_info_data_t base;
    mach_timebase_info(&base);
    uint64_t elapsed = (after - before) / base.denom / 1000 / 1000;

    std::cerr << "time: " << elapsed << " ms" << std::endl;
#endif
  }
}
