#include "trax.h"

#include <cstdlib>
#include <iostream>

#include "gflags/gflags.h"
#include "search.h"

DEFINE_bool(client, false, "Run as contest client.");

DEFINE_bool(self, false, "Run self play.");

DEFINE_bool(perft, false, "Run perft.");

DEFINE_int32(seed, 0, "Random seed.");

DEFINE_int32(depth, 1, "Search depth.");

DEFINE_int32(num_games, 100, "How many times to self play.");

DEFINE_bool(random_random, false,
            "Self play between random-random");

DEFINE_bool(simple_simple, false,
            "Self play between simple-simple");

DEFINE_bool(simple_negmax, false,
            "Self play between simple-negmax");

DEFINE_bool(negamax_random, false,
            "Self play between negamax-random");

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
  // Otherwise Position::FillForcedPieces() doesn't work.
  GenerateForcedPlayTable();

  // Initialize random seed.
  for (int i = 0; i < FLAGS_seed; ++i) {
    Random();
  }

  NegaMaxSearcher<LeafAverageEvaluator> negamax_searcher(FLAGS_depth);
  SimpleSearcher<LeafAverageEvaluator> simple_searcher;
  RandomSearcher random_searcher;

  if (FLAGS_client) {
    // Contest client.
    StartTraxClient(&negamax_searcher);

  } else if (FLAGS_self) {
    // Perform self play.
    if (FLAGS_random_random) {
      StartMultipleSelfGames(&random_searcher, &random_searcher,
                             FLAGS_num_games, !FLAGS_silent);
    } else if (FLAGS_simple_simple) {
      StartMultipleSelfGames(&simple_searcher, &simple_searcher,
                             FLAGS_num_games, !FLAGS_silent);
    } else if (FLAGS_simple_negmax) {
      StartMultipleSelfGames(&simple_searcher, &negamax_searcher,
                             FLAGS_num_games, !FLAGS_silent);
    } else if (FLAGS_negamax_random) {
      StartMultipleSelfGames(&negamax_searcher, &random_searcher,
                             FLAGS_num_games, !FLAGS_silent);
    } else {
#if 0
      StartMultipleSelfGames(&simple_searcher, &random_searcher,
                             FLAGS_num_games, !FLAGS_silent);
#endif

      NegaMaxSearcher<LeafAverageEvaluator> negamax_searcher1(1);
      NegaMaxSearcher<LeafAverageEvaluator> negamax_searcher2(2);
      StartMultipleSelfGames(&negamax_searcher1, &negamax_searcher2,
                             FLAGS_num_games, !FLAGS_silent);
    }
  } else if (FLAGS_perft) {
    // Do perft (performance testing by counting all the possible moves
    // within the given depth.)
    ShowPerft(FLAGS_depth);
  } else {
    google::ShowUsageWithFlags(argv[0]);
  }

  return 0;
}
