#pragma once

// Schedule of the --timelapse mode (.ai/specs/aquecimento-termico R1): one pair
// every interval, from the start, for a duration. The slots are anchored to the
// start (start + k * interval), so the loop jitter does not accumulate; a slot
// missed entirely (the loop stalled for longer than an interval) is skipped and
// counted. Times in seconds of any monotonic clock.

class TimelapseSchedule {
 public:
  TimelapseSchedule(double interval_s, double duration_min, double start_s);

  // Is a pair due at 'now_s'? Then call taken(now_s), saved or not.
  bool due(double now_s) const;
  void taken(double now_s);
  // The duration is over and the last slot has been handled
  bool finished(double now_s) const;

  int expectedPairs() const { return slots_; }
  int takenPairs() const { return taken_; }
  int skippedSlots() const { return skipped_; }
  double nextDue() const;
  double interval() const { return interval_s_; }

 private:
  double interval_s_;
  double start_s_;
  int slots_;      // slots in the duration: floor(duration / interval)
  int next_ = 0;   // index of the next slot
  int taken_ = 0;
  int skipped_ = 0;
};
