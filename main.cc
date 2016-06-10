#include "trax.h"

#include <cstdlib>
#include <iostream>

#include "gflags/gflags.h"
#include "perft.h"
#include "search.h"

DEFINE_bool(client, false, "Run as contest client.");

DEFINE_bool(self, false, "Run self play.");

DEFINE_bool(perft, false, "Run perft.");

DEFINE_int32(seed, 0, "Random seed.");

DEFINE_int32(depth, 1, "Perft depth.");

DEFINE_int32(num_games, 100, "How many times to self play.");

DEFINE_string(white, "simple-la", "Searcher name of white player (first)");

DEFINE_string(red, "negamax1-la", "Searcher name of red player (second)");

DEFINE_string(contest_player, "negamax1-la",
              "Searcher name of contest client player");

DEFINE_bool(silent, false, "Self play silently.");


namespace {

Searcher *GetSearcherFromName(const std::string& name) {
  if (name == "random") {
    return new RandomSearcher();
  } else if (name == "simple-la") {
    return new SimpleSearcher<LeafAverageEvaluator>();
  } else if (name == "simple-mc") {
    return new SimpleSearcher<MonteCarloEvaluator>();
  } else if (name == "negamax0-la") {
    return new NegaMaxSearcher<LeafAverageEvaluator>(0);
  } else if (name == "negamax1-la") {
    return new NegaMaxSearcher<LeafAverageEvaluator>(1);
  } else if (name == "negamax2-la") {
    return new NegaMaxSearcher<LeafAverageEvaluator>(2);
  } else if (name == "negamax1-mc") {
    return new NegaMaxSearcher<MonteCarloEvaluator>(1);
  } else {
    std::cerr << "cannot find searcher with name " << name << std::endl;
    exit(EXIT_FAILURE);
    return nullptr;
  }
}

}  // namespace

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

  if (FLAGS_client) {
    // Contest client.
    Searcher *player = GetSearcherFromName(FLAGS_contest_player);
    StartTraxClient(player);
    delete player;
  } else if (FLAGS_self) {
    // Perform self play.
    Searcher *white_player = GetSearcherFromName(FLAGS_white);
    Searcher *red_player = GetSearcherFromName(FLAGS_red);
    StartMultipleSelfGames(white_player, red_player,
                           FLAGS_num_games, !FLAGS_silent);
    delete white_player;
    delete red_player;
  } else if (FLAGS_perft) {
    // Do perft (performance testing by counting all the possible moves
    // within the given depth.)
    ShowPerft(FLAGS_depth);
  } else {
    google::ShowUsageWithFlags(argv[0]);
    std::cerr << std::endl;
    std::cerr << "Move struct size: " << sizeof(Move) << std::endl;
  }

  return 0;
}
