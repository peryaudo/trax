#include "trax.h"

#include <cstdlib>

#include "gflags/gflags.h"

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);

  // Otherwise Position::GetPossiblePieces() doesn't work.
  GeneratePossiblePiecesTable();
  // Otherwise Position::TraceVictoryLineOrLoop() doesn't work.
  GenerateTrackDirectionTable();

  NegaMaxSearcher negamax_searcher(3);
  RandomSearcher random_searcher;

  if (argc > 1) {
    for (int i = 0; i < atoi(argv[1]); ++i) {
      Random();
    }
  }

#if 0
  StartSelfGame(&negamax_searcher, &random_searcher, /* verbose = */ true);
#endif

#if 1
  StartMultipleSelfGames(&negamax_searcher, &random_searcher,
                         /* num_games = */ 100, /* verbose = */ true);
  // StartMultipleSelfGames(&negamax_searcher, &negamax_searcher,
  //                        /* num_games = */ 200, /* verbose = */ true);
#endif
#if 0
  StartMultipleSelfGames(&random_searcher, &random_searcher,
                         /* num_games = */ 10000, /* verbose = */ false);
#endif

  // Perft();

  return 0;
}
