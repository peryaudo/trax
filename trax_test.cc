#include <cassert>
#include <string>
#include <vector>

#include "trax.h"

void SupplyNotations(const std::vector<std::string>& notations,
                     Position *position) {
  for (auto&& notation : notations) {
    Position next_position;
    bool success = position->DoMove(Move(notation, *position), &next_position);
    assert(success);
    position->Swap(&next_position);
    position->Dump();
  }
}

int main(int argc, char *argv[]) {
  // Otherwise Position::GetPossiblePieces() doesn't work.
  GeneratePossiblePiecesTable();
  // Otherwise Position::TraceVictoryLineOrLoop() doesn't work.
  GenerateTrackDirectionTable();

  if (1) {
    Position position;
    Move move("@0+", position);
    assert(move.x == -1 && move.y == -1);
  }

  // Testing Forced plays.

  if (1) {
    Position position;
    SupplyNotations({"@0+", "B1+", "A2\\"}, &position);
    assert(static_cast<const Position&>(position).at(1, 1) == PIECE_RWWR);
  }

  if (1) {
    Position position;
    SupplyNotations({"@0/", "B1\\", "A2\\"}, &position);
    assert(position.finished() && position.winner() == 1);
  }

  // Testing victory line detections.

  if (1) {
    Position position;
    SupplyNotations({"@0+", "B1+", "C1+", "D1+", "E1+", "F1+", "G1+", "H1+"},
                    &position);
    assert(position.finished() && position.winner() == 1);
  }

  if (1) {
    Position position;
    SupplyNotations({"@0+", "B1+", "C1+", "D1+", "E1+", "F1+", "G1+", "H1\\"},
                    &position);
    assert(!position.finished());
  }

  if (1) {
    Position position;
    SupplyNotations({"@0+", "B1+", "C1+", "D1+", "E1+", "F1+", "G1+", "H1\\",
                     "H2\\"},
                    &position);
    assert(position.finished() && position.winner() == 1);
  }

  if (1) {
    Position position;
    SupplyNotations({"@0+", "B1+", "C1+", "D1+", "E1\\",
                     "E2\\", "@1/", "A2/", "@2+", "@2+"},
                    &position);
    assert(position.finished() && position.winner() == 1);
  }

  if (1) {
    Position position;
    SupplyNotations({"@0+", "B1+", "C1+", "D1+", "E1\\",
                     "E2\\", "@1/", "A2/", "@2+", "@2\\"},
                    &position);
    assert(!position.finished());
  }

  if (1) {
    Position position;
    SupplyNotations({"@0+", "A2+", "A3+", "A4+", "A5+", "A6+", "A7+", "A8+"},
                    &position);
    assert(position.finished() && position.winner() == -1);
  }

  if (1) {
    Position position;
    SupplyNotations({"@0+", "A2+", "A3+", "A4+", "A5+", "A6+", "A7+", "A8/"},
                    &position);
    assert(!position.finished());
  }

  return 0;
}
