// Copyright (C) 2016 Tetsui Ohkubo.

#include "./tt.h"

#include <gflags/gflags.h>

DEFINE_int32(tt_size_lg,
             23,
             "Logarithmic transposition table size. "
             "2^tt_size_lg * sizeof(one tt cluster) will be allocated.");

TranspositionTable::TranspositionTable()
    : mask_((1 << FLAGS_tt_size_lg) - 1)
    , table_(nullptr)
    , generation_(0)
    , mutex_() {
  table_ = new TranspositionTable::Cluster[1 << FLAGS_tt_size_lg];
}

TranspositionTable::~TranspositionTable() {
  delete table_;
}

bool TranspositionTable::Probe(PositionHash key,
                               TranspositionTable::Entry *entry) const {
  const Cluster& cluster = table_[key & mask_];
  for (int i = 0; i < kClusterSize; ++i) {
    // std::lock_guard<std::mutex> lock(mutex_);
    if (cluster.entries[i].bound != BOUND_NONE &&
        cluster.entries[i].key == key) {
      *entry = cluster.entries[i];
      return true;
    }
  }
  return false;
}

void TranspositionTable::Store(PositionHash key, Move best_move,
                               int score, int depth, Bound bound) {
  Cluster& cluster = table_[key & mask_];
  Entry *entry = nullptr;

  for (int i = 0; i < kClusterSize; ++i) {
    if (cluster.entries[i].key == key ||
        cluster.entries[i].bound == BOUND_NONE) {
      entry = &cluster.entries[i];
      break;
    }
  }

  if (entry == nullptr) {
    entry = &cluster.entries[0];
    for (int i = 0; i < kClusterSize; ++i) {
      if (cluster.entries[i].depth < entry->depth) {
        entry = &cluster.entries[i];
      } else if (cluster.entries[i].depth == entry->depth) {
        if (cluster.entries[i].generation < entry->generation) {
          entry = &cluster.entries[i];
        }
      }
    }
  }

  assert(entry != nullptr);
  assert(bound != BOUND_NONE);
  // std::lock_guard<std::mutex> lock(mutex_);
  entry->generation = generation_;
  entry->key = key;
  entry->best_move = best_move;
  entry->score = score;
  entry->depth = depth;
  entry->bound = bound;
}
