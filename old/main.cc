#include <vector>
#include <string>
#include <iostream>
#include <cassert>
#include <map>

using namespace std;

enum Piece {
  // W
  //R R
  // W
	PIECE_RWRW = 0,

  // R
  //W W
  // R
	PIECE_WRWR,

  // W
  //W R
  // R
	PIECE_RWWR,

  // Ditto
	PIECE_RRWW,
	PIECE_WRRW,
	PIECE_WWRR,

	PIECE_EMPTY
};

int dx[] = {1, 0, -1, 0};
int dy[] = {0, 1, 0, -1};

map<unsigned int, unsigned int> GenerateLegalMoveTable() {
  vector<string> edges = {
    "RWRW",
    "WRWR",
    "RWWR",
    "RRWW",
    "WRRW",
    "WWRR",
    "____"};

  map<unsigned int, unsigned int> ma;

  for (int i = 0; i < edges.size(); ++i) {
    for (int j = 0; j < edges.size(); ++j) {
      for (int k = 0; k < edges.size(); ++k) {
        for (int l = 0; l < edges.size(); ++l) {
          vector<string> neighbors = {edges[i], edges[j], edges[k], edges[l]};

          unsigned int key = (l<<24) + (k<<16) + (j<<8) + i;
          unsigned int value = 0;

          for (int m = 0; m < edges.size() - 1; ++m) {
            string center = edges[m];
            bool valid = true;
            for (int n = 0; n < neighbors.size(); ++n) {
              if (neighbors[n][(n + 2) % 4] != '_' &&
                  center[n] != neighbors[n][(n + 2) % 4]) {
                valid = false;
                break;
              }
            }
            if (valid) {
              value |= (1<<m);
            }
          }

          if (i == 6 && j == 6 && k == 6 && l == 6) {
            value = 0;
            ma[key] = value;
          }

          if (value > 0) {
            ma[key] = value;
            // for (auto&& neighbor : neighbors) {
            //   cout<<neighbor<<" ";
            // }
            // cout<<key<<"="<<value<<endl;
          }
        }
      }
    }
  }

  return ma;
}

map<unsigned int, unsigned int> legal_move_table;

using Move = unsigned int;

class Position {
 public:
  Position(char init_move)
      : red_to_move_(true)
      , width_(5)
      , height_(5)
      , board_(width_ * height_, PIECE_EMPTY) {
    assert(init_move == '+' || init_move == '/');
    if (init_move == '+') {
      board_[2 * height_ + 2] = PIECE_RWRW;
    } else if (init_move == '/') {
      board_[2 * height_ + 2] = PIECE_RWWR;
    }
  }

  vector<Move> LegalMoves() {
    vector<Move> moves;
    for (int i = 1; i < width_ - 1; ++i) {
      for (int j = 1; j < height_ - 1; ++j) {
        if (board_[i * height_ + j] != PIECE_EMPTY) {
          continue;
        }
        unsigned int neighbor_key = 0;
        for (int k = 0; k < 4; ++k) {
          neighbor_key |= (
              board_[(i + dx[k]) * height_ + j + dy[k]] << (8 * k));
        }
        auto it = legal_move_table.find(neighbor_key);
        assert(it != legal_move_table.end());
        auto legal_pieces = it->second;
        assert (__builtin_popcount(legal_pieces) != 1);
        for (int k = 0; k < 6; ++k) {
          if (legal_pieces & (1 << k)) {
            // cout<<"i: "<<i<<" j: "<<j<<" k: "<<k<<endl;
            moves.push_back((i << 18) + (j << 4) + k);
          }
        }
      }
    }
    return moves;
  }

  Position Place(Move move) {
    const int x = (move >> 18) & ((1 << 14) - 1);
    const int y = (move >> 4) & ((1 << 14) - 1);
    const Piece piece = static_cast<Piece>(move & ((1 << 4) - 1));
    int offset_x = 0, offset_y = 0;
    int width = width_, height = height_;
    if (x <= 1) {
      ++offset_x;
      ++width;
    }
    if (y <= 1) {
      ++offset_y;
      ++height;
    }
    if (x >= width_ - 2) {
      ++width;
    }
    if (y >= height_ - 2) {
      ++height;
    }

    Position next(!red_to_move_, width, height);

    for (int i = 1; i < width_ - 1; ++i) {
      for (int j = 1; j < height_ - 1; ++j) {
        next.board_[(i + offset_x) * height + j + offset_y] =
          board_[i * height_ + j];
      }
    }
    //cout<<x<<" "<<y<<" "<<piece<<endl;
    next.board_[(x + offset_x) * height + y + offset_y] = piece;
    return next;
  }

  void Dump() {
    for (int j = 1; j < height_ - 1; ++j) {
      for (int i = 1; i < width_ - 1; ++i) {
        auto b = board_[i * height_ + j];
        if (b == PIECE_RWRW || b == PIECE_WRWR) {
          cout<<"+";
        } else if (b == PIECE_RWWR || b == PIECE_WRRW) {
          cout<<"/";
        } else if (b == PIECE_EMPTY) {
          cout<<".";
        } else {
          cout<<"\\";
        }
      }
      cout<<endl;
    }
    cout<<endl;
  }

  // Return true if the position is still valid after forced placements.
  bool PlaceForced() {
    for (int i = 1; i < width_ - 1; ++i) {
      for (int j = 1; j < height_ - 1; ++j) {
        if (board_[i * height_ + j] != PIECE_EMPTY) {
          continue;
        }
        unsigned int neighbor_key = 0;
        for (int k = 0; k < 4; ++k) {
          neighbor_key |= (
              board_[(i + dx[k]) * height_ + j + dy[k]] << (8 * k));
        }
        auto it = legal_move_table.find(neighbor_key);
        if (it == legal_move_table.end()) {
          return false;
        }
        auto legal_pieces = it->second;
        if (__builtin_popcount(legal_pieces) == 1) {
          for (int k = 0; k < 6; ++k) {
            if (legal_pieces & (1 << k)) {
              Position next = Place((i << 18) + (j << 4) + k);
              board_.swap(next.board_);
              width_ = next.width_;
              height_ = next.height_;
              return PlaceForced();
            }
          }
        }
      }
    }
    return true;
  }

 private:
  Position(bool red_to_move, int width, int height)
      : red_to_move_(red_to_move)
      , width_(width)
      , height_(height)
      , board_(width * height, PIECE_EMPTY) {
  }

  bool red_to_move_;
  int width_;
  int height_;

  // sentinels
  vector<Piece> board_;
};

int counter = 0;

void Search(Position& position, int depth) {
  ++counter;
  // position.Dump();
  if (depth <= 0) return;

  for (auto&& move : position.LegalMoves()) {
    auto moved = position.Place(move);
    if (!moved.PlaceForced()) {
      return;
    }
    Search(moved, depth - 1);
  }

}

int main() {
  legal_move_table = GenerateLegalMoveTable();

  Position position('/');
  for (int i = 0; i < 8; ++i) {
    counter = 0;
    Search(position, i);
    cout<<"depth:"<<i<<" counter: "<<counter<<endl;
  }
  return 0;
}
