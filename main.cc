// Copyright (C) 2016 Tetsui Ohkubo.

#include <gflags/gflags.h>

#include <cstdlib>
#include <iostream>

#include "./perft.h"
#include "./search.h"
#include "./trax.h"

DEFINE_bool(client, false, "Run as contest client.");

DEFINE_bool(self, false, "Run self play.");

DEFINE_bool(perft, false, "Run perft.");

DEFINE_bool(accuracy, false, "Measure accuracy against human game log.");

DEFINE_bool(dump_factors, false, "Dump factors for human game log.");

DEFINE_bool(best_move, false,
            "Get the current board configuration from stdin"
            " and return the best move in trax notation."
            " Expected to be used for trax-daemon.");

DEFINE_bool(show_position, false,
            "Ad hoc solution not to implement game rules inside trax-daemon.");

DEFINE_int32(seed, 0, "Random seed.");

DEFINE_int32(depth, 1, "Perft depth.");

DEFINE_int32(num_games, 100, "How many times to self play.");

DEFINE_string(white, "simple-la", "Searcher name of white player (first)");

DEFINE_string(red, "negamax1-la", "Searcher name of red player (second)");

DEFINE_string(contest_player, "iter10-fe",
              "Searcher name of contest client player");

DEFINE_bool(silent, false, "Self play silently.");

DEFINE_int32(num_monte_carlo_trial, 100, "Number of Monte Carlo sampling.");

DEFINE_string(commented_games,
              "vendor/commented/Comment.txt",
              "File name of commented game data");

namespace {

Searcher *GetSearcherFromName(const std::string& name) {
  if (name == "random") {
    return new RandomSearcher();
  } else if (name == "simple-la") {
    return new SimpleSearcher<LeafAverageEvaluator>();
  } else if (name == "simple-mc") {
    return new SimpleSearcher<MonteCarloEvaluator>();
  } else if (name == "simple-ec") {
    return new SimpleSearcher<EdgeColorEvaluator>();
  } else if (name == "simple-cb") {
    return new SimpleSearcher<CombinedEvaluator>();
  } else if (name == "simple-ll") {
    return new SimpleSearcher<LongestLineEvaluator>();
  } else if (name == "simple-fe") {
    return new SimpleSearcher<FactorEvaluator>();
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
  } else if (name == "negamax0-ec") {
    return new NegaMaxSearcher<EdgeColorEvaluator>(0);
  } else if (name == "negamax1-ec") {
    return new NegaMaxSearcher<EdgeColorEvaluator>(1);
  } else if (name == "negamax2-ec") {
    return new NegaMaxSearcher<EdgeColorEvaluator>(2);
  } else if (name == "negamax3-ec") {
    return new NegaMaxSearcher<EdgeColorEvaluator>(3);
  } else if (name == "negamax4-ec") {
    return new NegaMaxSearcher<EdgeColorEvaluator>(4);
  } else if (name == "negamax0-ll") {
    return new NegaMaxSearcher<LongestLineEvaluator>(0);
  } else if (name == "negamax1-ll") {
    return new NegaMaxSearcher<LongestLineEvaluator>(1);
  } else if (name == "negamax2-ll") {
    return new NegaMaxSearcher<LongestLineEvaluator>(2);
  } else if (name == "negamax3-ll") {
    return new NegaMaxSearcher<LongestLineEvaluator>(3);
  } else if (name == "negamax0-cb") {
    return new NegaMaxSearcher<CombinedEvaluator>(0);
  } else if (name == "negamax1-cb") {
    return new NegaMaxSearcher<CombinedEvaluator>(1);
  } else if (name == "negamax2-cb") {
    return new NegaMaxSearcher<CombinedEvaluator>(2);
  } else if (name == "negamax3-cb") {
    return new NegaMaxSearcher<CombinedEvaluator>(3);
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
  } else if (name == "iter5-na") {
    return new NegaMaxSearcher<NoneEvaluator>(5, true);
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

  Move best_move = searcher->SearchBestMove(position);
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

double Average(const std::vector<double>& v) {
  double ans = 0.0;
  for (double x : v) {
    ans += x;
  }
  return ans / v.size();
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

  g_num_monte_carlo_trial = FLAGS_num_monte_carlo_trial;

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
  } else if (FLAGS_accuracy) {
    Searcher *searcher = GetSearcherFromName(FLAGS_contest_player);

    std::vector<Game> games;
    ParseCommentedGames(FLAGS_commented_games, &games);

    int numerator = 0;
    int denominator = 0;
    int loop_count = 0;
    int victory_line_count = 0;
    int resigns_count = 0;
    for (Game& game : games) {
      numerator += game.CountMatchingMoves(searcher);
      denominator += game.num_moves();

      if (game.winning_reason == WINNING_REASON_LOOP) {
        ++loop_count;
      } else if (game.winning_reason == WINNING_REASON_LINE) {
        ++victory_line_count;
      } else if (game.winning_reason == WINNING_REASON_RESIGN) {
        ++resigns_count;
      }
    }

    double accuracy = numerator * 100.0 / denominator;

    std::cerr << "Total: " << games.size() << std::endl;
    std::cerr << "Resigns: " << resigns_count << std::endl;
    std::cerr << "Loop: " << loop_count << std::endl;
    std::cerr << "Victory Line: " << victory_line_count << std::endl;
    std::cerr << "Accuracy: " << accuracy
      << "(" << numerator << "/" << denominator << ")" << std::endl;

    delete searcher;
  } else if (FLAGS_dump_factors) {
    std::vector<Game> games;
    ParseCommentedGames(FLAGS_commented_games, &games);
#if 0
    NegaMaxSearcher<LeafAverageEvaluator> searcher(1);
    for (Game& game : games) {
      game.ContinueBySearcher(&searcher);
    }
    for (int i = 0; i < 1000; ++i) {
      SimpleSearcher<LeafAverageEvaluator> searcher1;
      SimpleSearcher<LeafAverageEvaluator> searcher2;
      // NegaMaxSearcher<LeafAverageEvaluator> searcher2(1);
      Game game;
      StartSelfGame(&searcher1, &searcher2, &game, true);
      games.push_back(game);
    }
#endif

    std::cout << "step,winner,endpoint_factor,edge_factor" << std::endl;

    for (Game& game : games) {
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

        double endpoint_factor = 0.0;
        double edge_factor = 0.0;
        std::vector<Line> lines;
        position.EnumerateLines(&lines);
#if 1
        for (Line& line : lines) {
          // double endpoint = line.endpoint_distance;
          // double edge = line.edge_distances[0] + line.edge_distances[1];
          double endpoint = 1.0 / (1.0 + line.endpoint_distance);
          double edge = (1.0 / (1.0 + line.edge_distances[0]) +
                         1.0 / (1.0 + line.edge_distances[1]));
          // double edge = std::max(1.0 / (1.0 + line.edge_distances[0]),
          //                        1.0 / (1.0 + line.edge_distances[1]));
          if (!line.is_red) {
            endpoint *= -1.0;
            edge *= -1.0;
          }
          endpoint_factor += endpoint;
          edge_factor += edge;
        }
#elif 0
        std::vector<double> endpoints;
        std::vector<double> edges;
        for (Line& line : lines) {
          double endpoint = 1.0 / (1.0 + line.endpoint_distance);
          // double edge = (1.0 / (1.0 + line.edge_distances[0]) +
          //                1.0 / (1.0 + line.edge_distances[1]));
          double edge = std::max(1.0 / (1.0 + line.edge_distances[0]),
                                 1.0 / (1.0 + line.edge_distances[1]));
          if (!line.is_red) {
            endpoint *= -1.0;
            edge *= -1.0;
          }
          endpoints.push_back(endpoint);
          edges.push_back(edge);
        }
        endpoint_factor =
          *std::max_element(endpoints.begin(), endpoints.end()) +
          *std::min_element(endpoints.begin(), endpoints.end());
        edge_factor =
          *std::max_element(edges.begin(), edges.end()) +
          *std::min_element(edges.begin(), edges.end());
#else
        std::vector<double> red_endpoints, white_endpoints;
        std::vector<double> red_edges, white_edges;
        for (Line& line : lines) {
          double endpoint = 1.0 / (1.0 + line.endpoint_distance);
          // double edge = (1.0 / (1.0 + line.edge_distances[0]) +
          //                1.0 / (1.0 + line.edge_distances[1]));
          double edge = std::max(1.0 / (1.0 + line.edge_distances[0]),
                                 1.0 / (1.0 + line.edge_distances[1]));
          if (!line.is_red) {
            endpoint *= -1.0;
            edge *= -1.0;
          }
          if (line.is_red) {
            red_endpoints.push_back(endpoint);
            red_edges.push_back(edge);
          } else {
            white_endpoints.push_back(endpoint);
            white_edges.push_back(edge);
          }
        }
        endpoint_factor = Average(red_endpoints) + Average(white_endpoints);
        edge_factor = Average(red_edges) + Average(white_edges);
#endif

        std::cout
          << i << ","
          << game.winner << ","
          << endpoint_factor << ","
          << edge_factor << std::endl;
      }
    }
  } else if (FLAGS_best_move) {
    Searcher *player = GetSearcherFromName(FLAGS_contest_player);
    ReadAndFindBestMove(player);
    delete player;
  } else if (FLAGS_show_position) {
    ShowPosition();
  } else {
    google::ShowUsageWithFlags(argv[0]);
    std::cerr << std::endl;
    std::cerr << "Move struct size: " << sizeof(Move) << std::endl;
  }

  return 0;
}
