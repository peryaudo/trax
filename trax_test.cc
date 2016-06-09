#include <cassert>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "search.h"
#include "trax.h"

void SupplyNotations(const std::vector<std::string>& notations,
                     Position *position) {
  for (auto&& notation : notations) {
    Position next_position;
    ASSERT_TRUE(position->DoMove(Move(notation, *position), &next_position));
    position->Swap(&next_position);
    // Comment out to use for debug.
    // position->Dump();
  }
}

TEST(MoveTest, PutInitialPiece) {
  Position position;
  Move move("@0+", position);
  ASSERT_EQ(-1, move.x);
  ASSERT_EQ(-1, move.y);
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
}

TEST(PositionTest, WinByHorizontalVictoryLineSimple) {
  Position position;
  SupplyNotations({"@0+", "B1+", "C1+", "D1+", "E1+", "F1+", "G1+", "H1+"},
                  &position);
  ASSERT_TRUE(position.finished());
  ASSERT_EQ(1, position.winner());
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
}

TEST(PositionTest, WinByHorizontalVictoryLineComplex2) {
  Position position;
  SupplyNotations({"@0+", "B1+", "C1+", "D1+", "E1\\",
                   "E2\\", "@1/", "A2/", "@2+", "@2+"},
                  &position);
  ASSERT_TRUE(position.finished());
  ASSERT_EQ(1, position.winner());
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
}

TEST(PositionTest, NotVerticalVictoryLineSimple) {
  Position position;
  SupplyNotations({"@0+", "A2+", "A3+", "A4+", "A5+", "A6+", "A7+", "A8/"},
                  &position);
  ASSERT_FALSE(position.finished());
}

TEST(PositionTest, Tie) {
  Position position;
  SupplyNotations({"@0+", "B1\\", "B2/", "A2+", "A3/", "A0\\",
                   "@3+", "@3\\", "B2+"},
                  &position);
  ASSERT_TRUE(position.finished());
  ASSERT_EQ(0, position.winner());
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
  // Same but different order.
  SupplyNotations({"@0+", "A2+", "B1+", "A3/", "A4+", "B4+", "B5\\",
                   "C1\\", "@1+", "@1/", "A2/", "C0/", "@2\\", "C1\\",
                   "B0/", "G5\\", "F7/",
                   // Editing below could reproduce the problem.
                   "B5\\", "A2\\", "@3/"}, &position);
  ASSERT_TRUE(position.finished());
  ASSERT_EQ(-1, position.winner());
}

TEST(RandomSearcherTest, OneTime) {
  RandomSearcher random_searcher;
  StartSelfGame(&random_searcher, &random_searcher, /* verbose = */ false);
}

TEST(RandomSearcherTest, MultiTime) {
  RandomSearcher random_searcher;
  StartMultipleSelfGames(&random_searcher, &random_searcher,
                         /* num_games = */ 100, /* verbose = */ false);
}



// TODO(tetsui): Write test to check not victory line example here:
// http://lut.eee.u-ryukyu.ac.jp/traxjp/rules.html

// TODO(tetsui): Supply some real game data for unit test, like:
// http://www.traxgame.com/games_archives.php?pid=162

// TODO(tetsui): Do some kind of performance regression test,
// because performance of basic operations are pretty important.

// TODO(tetsui): Do StartTraxClient() test? Do we have any stdin / stdout test
// facility?


// TODO(tetsui): Write test for ScoreFinishedPosition()

int main(int argc, char *argv[]) {
  // Otherwise Position::GetPossiblePieces() doesn't work.
  GeneratePossiblePiecesTable();
  // Otherwise Position::TraceVictoryLineOrLoop() doesn't work.
  GenerateTrackDirectionTable();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
