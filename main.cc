#include "trax.h"

#include <cstdlib>

#include "gflags/gflags.h"

DEFINE_bool(client, false, "Run as contest client.");

DEFINE_bool(self, false, "Do self play.");

DEFINE_bool(perft, false, "Do perft.");

DEFINE_int32(seed, 0, "Random seed.");

DEFINE_int32(depth, 3, "Search depth.");

DEFINE_int32(num_games, 100, "How many times to self play.");

DEFINE_bool(random_random, false,
            "Self play with random players on both side.");

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

  NegaMaxSearcher negamax_searcher(FLAGS_depth);
  RandomSearcher random_searcher;

  if (FLAGS_client) {
    // Contest client.
    StartTraxClient(&negamax_searcher);

  } else if (FLAGS_self) {
    // Perform self play.
    if (FLAGS_random_random) {
      StartMultipleSelfGames(&random_searcher, &random_searcher,
                             FLAGS_num_games, /* verbose = */ true);
    } else {
      StartMultipleSelfGames(&negamax_searcher, &random_searcher,
                             FLAGS_num_games, /* verbose = */ true);
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
