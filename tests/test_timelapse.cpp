#include <random>
#include <vector>

#include "test_utils.hpp"
#include "timelapse.hpp"

TEST(timelapse_30s_for_60min_gives_120_pairs_30s_apart) {
  TimelapseSchedule s(30.0, 60.0, 100.0);
  CHECK(s.expectedPairs() == 120);
  std::mt19937 rng(1);
  std::uniform_real_distribution<double> loop(0.01, 0.08);  // ~15 fps plus the work
  std::vector<double> shots;
  double now = 100.0;
  while (!s.finished(now)) {
    if (s.due(now)) {
      shots.push_back(now);
      s.taken(now);
    }
    now += loop(rng);
  }
  CHECK(shots.size() == 120);
  CHECK(s.skippedSlots() == 0);
  for (size_t i = 1; i < shots.size(); ++i) {
    const double gap = shots[i] - shots[i - 1];
    CHECK(gap > 29.0 && gap < 31.0);
  }
  CHECK(shots.front() - 100.0 < 0.1);  // the first pair at the start
}

TEST(timelapse_does_not_accumulate_delay_and_skips_missed_slots) {
  TimelapseSchedule s(10.0, 1.0, 0.0);  // 6 slots: 0, 10, ..., 50
  CHECK(s.due(0.0));
  s.taken(0.5);
  CHECK(!s.due(9.9));
  CHECK(s.due(10.3));
  s.taken(10.3);
  CHECK_NEAR(s.nextDue(), 20.0, 1e-12);  // anchored to the start, not to 10.3
  s.taken(41.0);  // the loop stalled from 20 to 41: slot 20 taken late, 30 skipped
  CHECK(s.skippedSlots() == 1);
  CHECK_NEAR(s.nextDue(), 40.0, 1e-12);
  CHECK(s.due(41.0));
  s.taken(41.0);
  CHECK(!s.finished(49.0));
  CHECK(s.due(50.0));
  s.taken(50.0);
  CHECK(s.finished(50.0));
  CHECK(s.takenPairs() == 5);
}
