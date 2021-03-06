// Copyright (C) 2016 Tetsui Ohkubo.

#include <gtest/gtest.h>

#include <cassert>
#include <string>
#include <unordered_map>
#include <vector>

#include "./perft.h"
#include "./search.h"
#include "./timer.h"
#include "./trax.h"

void SupplyNotations(const std::vector<std::string>& notations,
                     Position *position) {
  for (const std::string& notation : notations) {
    Position next_position;
    ASSERT_TRUE(position->DoMove(Move(notation, *position), &next_position));
    position->Swap(&next_position);
    // Comment out to use for debug.
    // position->Dump();
  }
}

using NeighborKey = uint32_t;
extern NeighborKey EncodeNeighborKey(int right, int top, int left, int bottom);
extern PieceSet g_possible_pieces_table[1 << 12];

TEST(PossiblePieceTest, NoHitForInvalidPlace) {
  //  /
  // / +
  //  /
  NeighborKey key = EncodeNeighborKey(PIECE_WRWR,
                                      PIECE_WRRW,
                                      PIECE_RWWR,
                                      PIECE_RWWR);
  ASSERT_EQ(PieceSet(), g_possible_pieces_table[key]);
}

TEST(MoveTest, PutInitialPiece) {
  Position position;
  Move move("@0+", position);
  ASSERT_EQ(-1, move.x);
  ASSERT_EQ(-1, move.y);
}

TEST(MoveTest, UsualAlphabetEncode) {
  Move move(0, 0, PIECE_RWWR);
  ASSERT_EQ("A1/", move.notation());
}

TEST(MoveTest, LargeAlphabetEncode) {
  Move move(79, 80, PIECE_RWWR);
  ASSERT_EQ("CB81/", move.notation());

  Move move2(702, 80, PIECE_RWWR);
  ASSERT_EQ("AAA81/", move2.notation());
}

TEST(MoveTest, LargeAlphabetDecode) {
  std::vector<std::string> notations =
      {"@0/", "B1+", "B2/", "C2+", "C3/", "D3+", "D4/", "E4+", "E5/", "F5+",
      "F6/",
      "G6+", "G7/", "H7+", "H8/", "I8+", "I9/", "J9+", "J10/", "K10+", "K11/",
      "L11+", "L12/", "M12+", "M13/", "N13+", "N14/", "O14+", "O15/", "P15+",
      "P16/", "Q16+", "Q17/", "R17+", "R18/", "S18+", "S19/", "T19+", "T20/",
      "U20+", "U21/", "V21+", "V22/", "W22+", "W23/", "X23+", "X24/", "Y24+",
      "Y25/", "Z25+", "Z26/", "AA26+"};

  Position position;
  for (const std::string& notation : notations) {
    Position next_position;
    Move move;
    ASSERT_TRUE(move.Parse(notation, position));
    ASSERT_EQ(notation, move.notation());
    ASSERT_TRUE(position.DoMove(move, &next_position));
    position.Swap(&next_position);
  }
  position.Dump();
}

TEST(PositionTest, TriggerForcedPlay) {
  Position position;
  SupplyNotations({"@0+", "B1+", "A2\\"}, &position);
  ASSERT_EQ(PIECE_RWWR, static_cast<const Position&>(position).at(1, 1));
}

TEST(PositionTest, TriggerForcedPlayAndWinByLoop) {
  Position position;
  SupplyNotations({"@0/", "B1\\", "A2\\"}, &position);
  ASSERT_TRUE(position.finished());
  ASSERT_EQ(1, position.winner());
  ASSERT_EQ(WINNING_REASON_LOOP, position.winning_reason());
}

TEST(PositionTest, WinByHorizontalVictoryLineSimple) {
  Position position;
  SupplyNotations({"@0+", "B1+", "C1+", "D1+", "E1+", "F1+", "G1+", "H1+"},
                  &position);
  ASSERT_TRUE(position.finished());
  ASSERT_EQ(1, position.winner());
  ASSERT_EQ(WINNING_REASON_LINE, position.winning_reason());
}

TEST(PositionTest, NotHorizontalVictoryLineSimple) {
  Position position;
  SupplyNotations({"@0+", "B1+", "C1+", "D1+", "E1+", "F1+", "G1+", "H1\\"},
                  &position);
  ASSERT_FALSE(position.finished());
}

TEST(PositionTest, WinByHorizontalVictoryLineComplex1) {
  Position position;
  SupplyNotations({"@0+", "B1+", "C1+", "D1+", "E1+", "F1+", "G1+", "H1\\",
                   "H2\\"},
                  &position);
  ASSERT_TRUE(position.finished());
  ASSERT_EQ(1, position.winner());
  ASSERT_EQ(WINNING_REASON_LINE, position.winning_reason());
}

TEST(PositionTest, WinByHorizontalVictoryLineComplex2) {
  Position position;
  SupplyNotations({"@0+", "B1+", "C1+", "D1+", "E1\\",
                   "E2\\", "@1/", "A2/", "@2+", "@2+"},
                  &position);
  ASSERT_TRUE(position.finished());
  ASSERT_EQ(1, position.winner());
  ASSERT_EQ(WINNING_REASON_LINE, position.winning_reason());
}

TEST(PositionTest, NotHorizontalVictoryLineComplex) {
  Position position;
  SupplyNotations({"@0+", "B1+", "C1+", "D1+", "E1\\",
                   "E2\\", "@1/", "A2/", "@2+", "@2\\"},
                  &position);
  ASSERT_FALSE(position.finished());
}

TEST(PositionTest, WinByVerticalVictoryLineSimple) {
  Position position;
  SupplyNotations({"@0+", "A2+", "A3+", "A4+", "A5+", "A6+", "A7+", "A8+"},
                  &position);
  ASSERT_TRUE(position.finished());
  ASSERT_EQ(-1, position.winner());
  ASSERT_EQ(WINNING_REASON_LINE, position.winning_reason());
}

TEST(PositionTest, NotVerticalVictoryLineSimple) {
  Position position;
  SupplyNotations({"@0+", "A2+", "A3+", "A4+", "A5+", "A6+", "A7+", "A8/"},
                  &position);
  ASSERT_FALSE(position.finished());
}

TEST(PositionTest, NotHorizontalVictoryLineRealWorld1) {
  Position position;
  // Found wild on the way of debugging transposition table.
  SupplyNotations({"@0+", "A2+", "B1+", "A3/", "A4+", "B4+", "B5\\",
                   "C1\\", "@1+", "@1/", "A2/", "C0/", "@2\\", "C1\\",
                   "B0/", "G5\\", "F7/",
                   // Editing below could reproduce the problem.
                   "A2\\", "@3/", "C5\\"}, &position);
  ASSERT_TRUE(position.finished());
  ASSERT_EQ(-1, position.winner());
}

TEST(PositionTest, NotHorizontalVictoryLineRealWorld2) {
  Position position;
  // Same as NotHorizontalVictoryLineRealWorld2 but in different order.
  SupplyNotations({"@0+", "A2+", "B1+", "A3/", "A4+", "B4+", "B5\\",
                   "C1\\", "@1+", "@1/", "A2/", "C0/", "@2\\", "C1\\",
                   "B0/", "G5\\", "F7/",
                   // Editing below could reproduce the problem.
                   "B5\\", "A2\\", "@3/"}, &position);
  ASSERT_TRUE(position.finished());
  ASSERT_EQ(-1, position.winner());
}

TEST(PositionTest, ForcedPlays) {
  Position position;
  SupplyNotations(
      {"@0+", "B1/", "C1/", "A2/", "A3+", "A4/", "B4+", "C4/"}, &position);

  Move illegal_move("C3/", position);
  Move legal_move("C3+", position);

  Position next_position;
  ASSERT_FALSE(position.DoMove(illegal_move, &next_position));
  ASSERT_TRUE(position.DoMove(legal_move, &next_position));
}

TEST(PositionTest, VictoryIsLastPlayers1) {
  // There's no such thing like tie or draw in unlimited Trax (8x8Trax does.)
  // See http://www.traxgame.com/about_faq.php for detail.
  //   Q. What happens if we both win on the same turn?
  //   A. This is a win for the player who made the move (see the rules).
  //
  // (Reported by @snuke_. Thanks!)

  Position position;
  SupplyNotations({"@0+", "B1\\", "B2/", "A2+", "A3/", "A0\\",
                   "@3+", "@3\\", "B2+"},
                  &position);
  ASSERT_TRUE(position.finished());
  ASSERT_EQ(-1, position.winner());
  ASSERT_EQ(WINNING_REASON_LOOP, position.winning_reason());
}

TEST(PositionTest, VictoryIsLastPlayers2) {
  Position position;
  SupplyNotations(
      {"@0+", "A0+", "B2\\", "A3+", "@2\\", "D2\\", "E2\\", "A0/", "D2+",
       "@1\\", "A0\\", "C1+", "@2+", "E0/", "F3+", "G2\\", "H5\\", "F7/",
       "G8+", "G1/", "E7/"},
      &position);
  ASSERT_TRUE(position.finished());
  ASSERT_EQ(-1, position.winner());
}

TEST(PositionTest, NotHorizontalVictoryLineBecauesNotRightmost) {
  // Based on the example here:
  // http://lut.eee.u-ryukyu.ac.jp/traxjp/rules.html

  Position position;
  SupplyNotations(
      {"@0/", "B1\\", "C1/", "D1+", "E1\\", "F1\\", "G1/", "A2/", "B2+",
       "C2\\", "D2\\", "E2\\", "F2+", "G2+", "H2/", "A3\\", "B3\\", "C3+",
       "F3\\", "H3+",
       "I3/", "H1/"},
       &position);
  ASSERT_FALSE(position.finished());
}

TEST(PositionTest, WinByHorizontalVictoryLineBecauesRightmost) {
  // Ditto.
  Position position;
  SupplyNotations(
      {"@0/", "B1\\", "C1/", "D1+", "E1\\", "F1\\", "G1/", "A2/", "B2+",
       "C2\\", "D2\\", "E2\\", "F2+", "G2+", "H2/", "A3\\", "B3\\", "C3+",
       "F3\\", "H3+",
       "H1/"},
       &position);
  ASSERT_TRUE(position.finished());
  ASSERT_EQ(1, position.winner());
  ASSERT_EQ(WINNING_REASON_LINE, position.winning_reason());
}

TEST(PositionTest, EnumerateLines1) {
  Position position;
  SupplyNotations({"@0+"}, &position);
  std::vector<Line> lines;
  position.EnumerateLines(&lines);
  int red_count = 0, white_count = 0;
  for (Line& line : lines) {
    if (line.is_red) {
      ++red_count;
      ASSERT_EQ(7, line.edge_distances[0]);
      ASSERT_EQ(9, line.edge_distances[1]);
    } else {
      ++white_count;
      ASSERT_EQ(9, line.edge_distances[0]);
      ASSERT_EQ(7, line.edge_distances[1]);
    }
    ASSERT_EQ(6, line.endpoint_distance);
  }
  ASSERT_EQ(1, red_count);
  ASSERT_EQ(1, white_count);
}

TEST(PositionTest, EnumerateLines2) {
  Position position;
  SupplyNotations(
      {"@0/", "B1\\", "C1/", "D1+", "E1\\", "F1\\", "G1/", "A2/", "B2+",
       "C2\\", "D2\\", "E2\\", "F2+", "G2+", "H2/", "A3\\", "B3\\", "C3+",
       "F3\\", "H3+",
       "I3/", "H1/"},
       &position);

  std::vector<Line> lines;
  position.EnumerateLines(&lines);
  int red_count = 0, white_count = 0;
  for (Line& line : lines) {
    if (line.is_red) {
      ++red_count;
    } else {
      ++white_count;
    }
  }

  ASSERT_EQ(5, red_count);
  ASSERT_EQ(7, white_count);
}

TEST(PositionTest, EnumerateLines3) {
  Position position;
  SupplyNotations({"@0/", "B1\\", "C1/", "D1\\", "B0+"}, &position);

  std::vector<Line> lines;
  position.EnumerateLines(&lines);

  int red_mates = 0;
  int white_mates = 0;
  for (const Line& line : lines) {
    if (line.is_mate()) {
      if (line.is_red) {
        ++red_mates;
      } else {
        ++white_mates;
      }
    }
  }

  ASSERT_EQ(6, lines.size());

  ASSERT_EQ(2, red_mates);
  ASSERT_EQ(0, white_mates);
  ASSERT_LT(0, CalcMateScore(position, lines));
}

TEST(PositionTest, EnumerateLines4) {
  Position position;
  SupplyNotations({"@0/", "B1\\", "C1/", "D1\\"}, &position);
  std::vector<Line> lines;
  position.EnumerateLines(&lines);
  ASSERT_LT(0, CalcMateScore(position, lines));
}

TEST(PositionTest, EnumerateLines5) {
  Position position;
  SupplyNotations({"@0/", "B1\\", "C1/", "D1\\", "B0+", "@2+"}, &position);
  std::vector<Line> lines;
  position.EnumerateLines(&lines);
  ASSERT_GT(0, CalcMateScore(position, lines));
}

TEST(PositionTest, EnumerateLines6) {
  Position position;
  SupplyNotations(
      {"@0/", "B1+", "C1\\", "D1/", "E1\\", "E0\\"}, &position);
  std::vector<Line> lines;
  position.EnumerateLines(&lines);

  ASSERT_GT(0, CalcMateScore(position, lines));
}

TEST(PositionTest, EnumerateLines7) {
  Position position;
  SupplyNotations({"@0+", "B1+", "C1+", "D1+", "E1+", "F1+", "G1+"},
                  &position);
  std::vector<Line> lines;
  position.EnumerateLines(&lines);

  int red_mates = 0;
  int white_mates = 0;
  for (const Line& line : lines) {
    if (line.is_mate()) {
      if (line.is_red) {
        ++red_mates;
      } else {
        ++white_mates;
      }
    }
  }

  ASSERT_EQ(8, lines.size());

  ASSERT_EQ(1, red_mates);
  ASSERT_EQ(0, white_mates);
}

#if 0
TEST(PositionTest, EnumreateLinesLoop1) {
  Position position;
  SupplyNotations({"@0/", "B1+", "C1/"}, &position);

  std::vector<Line> lines;
  position.EnumerateLines(&lines);
  int inner_count = 0;
  for (const Line& line : lines) {
    if (line.is_inner) {
      ASSERT_EQ(5, line.loop_distances[0]);
      ASSERT_EQ(5, line.loop_distances[1]);

      ++inner_count;
    }
  }

  ASSERT_EQ(1, inner_count);
}

TEST(PositionTest, EnumreateLinesLoop2) {
  Position position;
  SupplyNotations({"@0/", "B1+", "C1+", "D1/"}, &position);

  std::vector<Line> lines;
  position.EnumerateLines(&lines);
  int inner_count = 0;

  for (const Line& line : lines) {
    if (line.is_inner) {
      int loop_distance_a = line.loop_distances[0];
      int loop_distance_b = line.loop_distances[1];
      if (loop_distance_a > loop_distance_b) {
        std::swap(loop_distance_a, loop_distance_b);
      }

      ASSERT_EQ(2, loop_distance_a);
      ASSERT_EQ(5, loop_distance_b);

      ++inner_count;
    }
  }

  ASSERT_EQ(2, inner_count);
}
#endif

TEST(PerftTest, PerftReturnsCorrectNumberIn4) {
  Timer timer(-1);
  ASSERT_EQ(246888, Perft(5, &timer));
}

TEST(TimerTest, Measure800Ms) {
  for (int i = 0; i < 10; ++i) {
    Timer timer(800);
    while (!timer.CheckTimeout()) {
    }
    ASSERT_LE(800, timer.elapsed_ms());
    ASSERT_GE(870, timer.elapsed_ms());
  }
}

TEST(RandomSearcherTest, OneTime) {
  RandomSearcher random_searcher;
  Game game;
  StartSelfGame(&random_searcher, &random_searcher,
                &game, /* verbose = */ false);
}

TEST(RandomSearcherTest, MultiTime) {
  RandomSearcher random_searcher;
  std::vector<Game> games;
  StartMultipleSelfGames(&random_searcher, &random_searcher,
                         /* num_games = */ 100,  &games,
                         /* verbose = */ false);
}

TEST(ParseCommentedGameTest, Parse) {
  std::vector<Game> games;
  ParseCommentedGames("./vendor/commented/Comment.txt", &games);
  // Number of games excluding 8x8 Trax and LoopTrax is 277.
  // $ grep ^Trax vendor/commented/Comment.txt | wc -l
  ASSERT_EQ(277, games.size());
}

// TODO(tetsui): Do some kind of performance regression test,
// because performance of basic operations are pretty important.

// TODO(tetsui): Do StartTraxClient() test? Do we have any stdin / stdout test
// facility?

int main(int argc, char *argv[]) {
  // Otherwise Position::GetPossiblePieces() doesn't work.
  GeneratePossiblePiecesTable();
  // Otherwise Position::TraceVictoryLineOrLoop() doesn't work.
  GenerateTrackDirectionTable();
  // Otherwise Position::FillForcedPieces() doesn't work.
  GenerateForcedPlayTable();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
