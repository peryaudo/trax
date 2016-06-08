#include "trax.h"

#include <cstdlib>
#include <iostream>

#include "gflags/gflags.h"
#include "search.h"

DEFINE_bool(client, false, "Run as contest client.");

DEFINE_bool(self, false, "Do self play.");

DEFINE_bool(perft, false, "Do perft.");

DEFINE_int32(seed, 0, "Random seed.");

DEFINE_int32(depth, 1, "Search depth.");

DEFINE_int32(num_games, 100, "How many times to self play.");

DEFINE_bool(random_random, false,
            "Self play with random players on both side.");

DEFINE_bool(negamax_negamax, false,
            "Self play with negamax players on both side.");

DEFINE_bool(negamax1_negamax3, false,
            "Self play with negamax players on both side. "
            "(depth=1 and depth=3 depth flag is ignored)");

DEFINE_bool(silent, false, "Self play silently.");

int main(int argc, char *argv[]) {
  google::SetUsageMessage(
      "Trax artificial intelligence.\n\n"
      "usage: ./trax (--client|--self|--perft)");
  google::ParseCommandLineFlags(&argc, &argv, true);

  // Otherwise Position::GetPossiblePieces() doesn't work.
  GeneratePossiblePiecesTable();
  // Otherwise Position::TraceVictoryLineOrLoop() doesn't work.
  GenerateTrackDirectionTable();

  // Initialize random seed.
  for (int i = 0; i < FLAGS_seed; ++i) {
    Random();
  }

  if (FLAGS_negamax1_negamax3) {
    NegaMaxSearcher negamax1(1);
    NegaMaxSearcher negamax2(3);
    StartMultipleSelfGames(&negamax1, &negamax2,
                           FLAGS_num_games, !FLAGS_silent);

    return 0;
  }

  NegaMaxSearcher negamax_searcher(FLAGS_depth);
  RandomSearcher random_searcher;

  if (FLAGS_client) {
    // Contest client.
    StartTraxClient(&negamax_searcher);

  } else if (FLAGS_self) {
    // Perform self play.
    if (FLAGS_random_random) {
      StartMultipleSelfGames(&random_searcher, &random_searcher,
                             FLAGS_num_games, !FLAGS_silent);
    } else if (FLAGS_negamax_negamax) {
      std::cerr << "negamax-negamax" << std::endl;
      StartMultipleSelfGames(&negamax_searcher, &negamax_searcher,
                             FLAGS_num_games, !FLAGS_silent);
    } else {
      StartMultipleSelfGames(&negamax_searcher, &random_searcher,
                             FLAGS_num_games, !FLAGS_silent);
    }
  } else if (FLAGS_perft) {
    // Do perft (performance testing by counting all the possible moves
    // within the given depth.)
    Perft(FLAGS_depth);
  } else {
    google::ShowUsageWithFlags(argv[0]);
  }

  return 0;
}
