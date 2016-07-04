// Copyright (C) 2016 Tetsui Ohkubo.

#include <gflags/gflags.h>

#include <cstdlib>
#include <iostream>
#include <memory>

#include "./perft.h"
#include "./search.h"
#include "./trax.h"

DEFINE_bool(client, false, "Run as contest client.");

DEFINE_bool(self, false, "Run self play.");

DEFINE_bool(use_log, false, "Load human game log.");

DEFINE_bool(perft, false, "Run perft.");

DEFINE_bool(prediction, false,
            "Measure prediction rate against human game log.");

DEFINE_bool(factors_csv, false, "Output factors to stdout (self/use_log.)");

DEFINE_bool(stats_csv, false,
            "Output game statistics to stdout (self/use_log.)");

DEFINE_bool(tournament, false, "Run tournament.");

DEFINE_bool(best_move, false,
            "Get the current board configuration from stdin"
            " and return the best move in trax notation."
            " Expected to be used for trax-daemon.");

DEFINE_bool(show_position, false,
            "Ad hoc solution not to implement game rules inside trax-daemon.");

DEFINE_int32(seed, 0, "Random seed.");

DEFINE_int32(perft_depth, 6, "Perft depth.");

DEFINE_int32(num_games, 100, "How many times to self play.");

DEFINE_string(white, "simple-la", "Searcher name of white player (first)");

DEFINE_string(red, "negamax1-la", "Searcher name of red player (second)");

DEFINE_string(searcher, "iter10-fe", "Searcher name of contest client player");

DEFINE_bool(verbose, true, "Verbose output on self play.");

DEFINE_string(commented_games,
              "vendor/commented/Comment.txt",
              "File name of commented game data");

DEFINE_bool(
    interpolate,
    false,
    "Finish resigned games in human game log by using searcher.");


namespace {

Searcher *GetSearcherFromName(const std::string& name) {
  if (name == "random") {
    return new RandomSearcher();
  } else if (name == "simple-la") {
    return new SimpleSearcher<LeafAverageEvaluator>();
  } else if (name == "simple-mc") {
    return new SimpleSearcher<MonteCarloEvaluator>();
  } else if (name == "simple-fe") {
    return new SimpleSearcher<FactorEvaluator>();
  } else if (name == "simple-afe") {
    return new SimpleSearcher<AdvancedFactorEvaluator>();
  } else if (name == "negamax0-la") {
    return new NegaMaxSearcher<LeafAverageEvaluator>(0);
  } else if (name == "negamax1-la") {
    return new NegaMaxSearcher<LeafAverageEvaluator>(1);
  } else if (name == "iter1-la") {
    return new NegaMaxSearcher<LeafAverageEvaluator>(1, true);
  } else if (name == "negamax2-la") {
    return new NegaMaxSearcher<LeafAverageEvaluator>(2);
  } else if (name == "iter10-la") {
    return new NegaMaxSearcher<LeafAverageEvaluator>(10, true);
  } else if (name == "negamax1-mc") {
    return new NegaMaxSearcher<MonteCarloEvaluator>(1);
  } else if (name == "negamax2-mc") {
    return new NegaMaxSearcher<MonteCarloEvaluator>(2);
  } else if (name == "negamax0-na") {
    return new NegaMaxSearcher<NoneEvaluator>(0);
  } else if (name == "negamax1-na") {
    return new NegaMaxSearcher<NoneEvaluator>(1);
  } else if (name == "negamax2-na") {
    return new NegaMaxSearcher<NoneEvaluator>(2);
  } else if (name == "negamax3-na") {
    return new NegaMaxSearcher<NoneEvaluator>(3);
  } else if (name == "negamax4-na") {
    return new NegaMaxSearcher<NoneEvaluator>(4);
  } else if (name == "iter10-na") {
    return new NegaMaxSearcher<NoneEvaluator>(10, true);
  } else if (name == "negamax0-fe") {
    return new NegaMaxSearcher<FactorEvaluator>(0);
  } else if (name == "negamax1-fe") {
    return new NegaMaxSearcher<FactorEvaluator>(1);
  } else if (name == "iter1-fe") {
    return new NegaMaxSearcher<FactorEvaluator>(1, true);
  } else if (name == "iter10-fe") {
    return new NegaMaxSearcher<FactorEvaluator>(10, true);
  } else if (name == "negamax2-fe") {
    return new NegaMaxSearcher<FactorEvaluator>(2);
  } else if (name == "negamax3-fe") {
    return new NegaMaxSearcher<FactorEvaluator>(3);
  } else if (name == "negamax4-fe") {
    return new NegaMaxSearcher<FactorEvaluator>(4);
  } else if (name == "iter10-afe") {
    return new NegaMaxSearcher<AdvancedFactorEvaluator>(10, true);
  } else {
    std::cerr << "cannot find searcher with name " << name << std::endl;
    exit(EXIT_FAILURE);
    return nullptr;
  }
}

// Read current board configuration from stdin and return the best move to
// stdout.
void ReadAndFindBestMove(Searcher* searcher) {
  int num_moves = 0;
  std::cin >> num_moves;

  std::vector<std::string> moves_notation(num_moves);
  for (std::string& move_notation : moves_notation) {
    std::cin >> move_notation;
  }

  Position position;
  for (const std::string& move_notation : moves_notation) {
    Move move;
    if (!move.Parse(move_notation, position)) {
      std::cerr
        << "Illegal move. Please go back and try another." << std::endl;
      exit(EXIT_FAILURE);
    }

    Position next_position;
    if (!position.DoMove(move, &next_position)) {
      std::cerr
        << "Illegal move. Please go back and try another." << std::endl;
      exit(EXIT_FAILURE);
    }

    position.Swap(&next_position);
  }

  if (position.finished()) {
    // Otherwise SearchBestMove may crash! Reported by takiyu. (thx!)
    return;
  }

  Timer timer(800);
  Move best_move = searcher->SearchBestMove(position, &timer);
  std::cout << best_move.notation();
}

// See the description of the corresponding flag.
void ShowPosition() {
  int num_moves = 0;
  std::cin >> num_moves;

  std::vector<std::string> moves_notation(num_moves);
  for (std::string& move_notation : moves_notation) {
    std::cin >> move_notation;
  }

  Position position;
  for (const std::string& move_notation : moves_notation) {
    Move move;
    if (!move.Parse(move_notation, position)) {
      exit(EXIT_FAILURE);
    }

    Position next_position;
    if (!position.DoMove(move, &next_position)) {
      exit(EXIT_FAILURE);
    }

    position.Swap(&next_position);
  }

  if (position.finished()) {
    std::cout << position.winner() << std::endl;
  } else {
    std::cout << std::endl;
  }

  for (int i_y = -1; i_y < position.max_y() + 1; ++i_y) {
    for (int j_x = -1; j_x < position.max_x() + 1; ++j_x) {
      std::cout << static_cast<const Position &>(position).at(j_x, i_y);
      if (j_x == position.max_x()) {
        std::cout << std::endl;
      } else {
        std::cout << " ";
      }
    }
  }
}

void DumpGamesStatistics(const std::vector<Game>& games) {
  int loop_count = 0;
  int victory_line_count = 0;
  int resigns_count = 0;

  double average_moves = 0;

  for (const Game& game : games) {
    average_moves += game.num_moves();

    if (game.winning_reason == WINNING_REASON_LOOP) {
      ++loop_count;
    } else if (game.winning_reason == WINNING_REASON_LINE) {
      ++victory_line_count;
    } else if (game.winning_reason == WINNING_REASON_RESIGN) {
      ++resigns_count;
    }
  }

  average_moves /= games.size();

  std::cerr << "Total: " << games.size() << std::endl;
  std::cerr << "  Resigns: " << resigns_count << std::endl;
  std::cerr << "  Loop: " << loop_count << std::endl;
  std::cerr << "  Victory Line: " << victory_line_count << std::endl;
  std::cerr << "Average moves: " << average_moves << std::endl;
}

void DumpGamesStatisticsCSV(const std::vector<Game>& games) {
  std::cout << "total_step,winner,winning_reason" << std::endl;
  for (const Game& game : games) {
    std::cout
      << game.num_moves() << ","
      << game.winner << ",";
    switch (game.winning_reason) {
      case WINNING_REASON_UNKNOWN:
        std::cout << "UNKNOWN";
        break;
      case WINNING_REASON_LOOP:
        std::cout << "LOOP";
        break;
      case WINNING_REASON_LINE:
        std::cout << "LINE";
        break;
      case WINNING_REASON_FULL:
        std::cout << "FULL";
        break;
      case WINNING_REASON_RESIGN:
        std::cout << "RESIGN";
        break;
    }
    std::cout << std::endl;
  }
}

void DumpFactors(const std::vector<Game>& games) {
  std::string first_line = "step,winner";
  bool first = true;

  for (const Game& game : games) {
    if (game.winner == 0) {
      continue;
    }

    Position position;
    for (int i = 0; i < game.num_moves(); ++i) {
      Position next_position;
      Move move = game.moves[i];
      bool success = position.DoMove(move, &next_position);
      if (!success) {
        std::cerr << "something went wrong!" << std::endl;
        exit(EXIT_FAILURE);
      }
      position.Swap(&next_position);

      if (position.finished()) {
        continue;
      }

      std::vector<std::pair<std::string, double>> factors;
      GenerateFactors(position, &factors);

      if (first) {
        for (std::pair<std::string, double>& factor : factors) {
          first_line += "," + factor.first;
        }
        std::cout << first_line << std::endl;

        first = false;
      }

      std::cout << i << "," << game.winner;
      for (std::pair<std::string, double>& factor : factors) {
        std::cout << "," << factor.second;
      }

      std::cout << std::endl;
    }
  }
}

// Ranks searchers by simplified version of Elo rating.
// https://en.wikipedia.org/wiki/Elo_rating_system
//
// The method is based on that of Shogi Club 24 or floodgate's one.
void RunTournament() {
  Searcher *searchers[] = {
    new RandomSearcher(),
    new SimpleSearcher<LeafAverageEvaluator>(),
    new SimpleSearcher<FactorEvaluator>(),
    new NegaMaxSearcher<LeafAverageEvaluator>(1),
    new NegaMaxSearcher<LeafAverageEvaluator>(10, true),
    new NegaMaxSearcher<FactorEvaluator>(10, true),
    new NegaMaxSearcher<AdvancedFactorEvaluator>(10, true)
  };

  const int num_searchers = sizeof(searchers) / sizeof(searchers[0]);
  std::vector<double> rates(num_searchers, 1500.0);

  for (int i = 0; i < FLAGS_num_games; ++i) {
    std::cerr << "Game " << i << ": ";

    int white_index = Random() % num_searchers;
    int red_index = white_index;
    while (white_index == red_index) {
      red_index = Random() % num_searchers;
    }

    Game game;
    StartSelfGame(searchers[white_index],
                  searchers[red_index], &game, FLAGS_verbose);

    if (game.winner > 0) {
      std::cerr << "[LOSE]white: " << searchers[white_index]->name() <<
        " [WIN]red: " << searchers[red_index]->name() << std::endl;

      double delta_r =
        16 + (rates[white_index] - rates[red_index]) * 0.04;
      delta_r = std::min(31.0, std::max(1.0, delta_r));
      rates[red_index] += delta_r;
      rates[white_index] -= delta_r;
    } else if (game.winner < 0) {
      std::cerr << "[WIN]white: " << searchers[white_index]->name() <<
        " [LOSE]red: " << searchers[red_index]->name() << std::endl;

      double delta_r =
        16 + (rates[red_index] - rates[white_index]) * 0.04;
      delta_r = std::min(31.0, std::max(1.0, delta_r));
      rates[white_index] += delta_r;
      rates[red_index] -= delta_r;
    } else {
      std::cerr << "[DRAW]white: " << searchers[white_index]->name() <<
        " [DRAW]red: " << searchers[red_index]->name() << std::endl;
    }

    std::vector<std::pair<double, std::string>> ranking;
    for (int i = 0; i < num_searchers; ++i) {
      ranking.emplace_back(rates[i], searchers[i]->name());
    }

    std::sort(ranking.rbegin(), ranking.rend());
    for (auto& ranked : ranking) {
      std::cerr << ranked.first << "\t" << ranked.second << std::endl;
    }
    std::cerr << std::endl;
    std::cerr << std::endl;
  }
}

}  // namespace

int main(int argc, char *argv[]) {
  google::SetUsageMessage(
      "Trax artificial intelligence.\n\n"
      "usage: ./trax (--client|--perft|--prediction|--self|--use_log|"
      "--tournament)");
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

  //// Realtime playing facilities.

  // These modes are for trax-daemon (Trax playing online frontend) and
  // the interface is private and subject to change.
  if (FLAGS_best_move) {
    std::unique_ptr<Searcher> searcher;
    searcher.reset(GetSearcherFromName(FLAGS_searcher));

    ReadAndFindBestMove(searcher.get());

    return 0;
  }

  if (FLAGS_show_position) {
    ShowPosition();
    return 0;
  }

  // Official contest client.
  if (FLAGS_client) {
    std::unique_ptr<Searcher> searcher;
    searcher.reset(GetSearcherFromName(FLAGS_searcher));

    StartTraxClient(searcher.get());

    return 0;
  }

  //// Benchmarking utilities.

  // Benchmark its performance by counting all the possible moves within
  // the given depth.
  if (FLAGS_perft) {
    ShowPerft(FLAGS_perft_depth);
    return 0;
  }

  // Measure prediction accuracy of the evaluation function against
  // human game log.
  if (FLAGS_prediction) {
    std::unique_ptr<Searcher> searcher;
    searcher.reset(GetSearcherFromName(FLAGS_searcher));

    std::vector<Game> games;
    ParseCommentedGames(FLAGS_commented_games, &games);

    int numerator = 0;
    int denominator = 0;
    for (Game& game : games) {
      numerator += game.CountMatchingMoves(searcher.get());
      denominator += game.num_moves();
    }

    DumpGamesStatistics(games);

    const double prediction = numerator * 100.0 / denominator;
    std::cerr << "Prediction: " << prediction << "% "
      << "(" << numerator << "/" << denominator << ")" << std::endl;

    return 0;
  }

  //// Strategy evaluation and improvement tools.

  if (FLAGS_self || FLAGS_use_log) {
    std::vector<Game> games;

    if (FLAGS_self) {
      // Perform self play.
      std::unique_ptr<Searcher> white_player;
      std::unique_ptr<Searcher> red_player;
      white_player.reset(GetSearcherFromName(FLAGS_white));
      red_player.reset(GetSearcherFromName(FLAGS_red));

      StartMultipleSelfGames(white_player.get(), red_player.get(),
                             FLAGS_num_games, &games, FLAGS_verbose);
    } else {
      // Parse human played game logs.
      ParseCommentedGames(FLAGS_commented_games, &games);
      if (FLAGS_interpolate) {
        std::unique_ptr<Searcher> searcher;
        searcher.reset(GetSearcherFromName(FLAGS_searcher));

        for (Game& game : games) {
          game.ContinueBySearcher(searcher.get());
        }
      }
    }

    if (FLAGS_factors_csv) {
      DumpFactors(games);
    } else if (FLAGS_stats_csv) {
      DumpGamesStatisticsCSV(games);
    }

    DumpGamesStatistics(games);

    return 0;
  }

  if (FLAGS_tournament) {
    RunTournament();
    return 0;
  }

  google::ShowUsageWithFlags(argv[0]);
  std::cerr << std::endl;
  std::cerr << "Move struct size: " << sizeof(Move) << std::endl;

  return EXIT_FAILURE;
}
