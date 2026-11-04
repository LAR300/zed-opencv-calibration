#include "timelapse.hpp"

#include <cmath>

TimelapseSchedule::TimelapseSchedule(double interval_s, double duration_min, double start_s)
    : interval_s_(interval_s),
      start_s_(start_s),
      slots_(static_cast<int>(std::floor(duration_min * 60.0 / interval_s + 1e-9))) {}

double TimelapseSchedule::nextDue() const { return start_s_ + next_ * interval_s_; }

bool TimelapseSchedule::due(double now_s) const {
  return next_ < slots_ && now_s >= nextDue();
}

void TimelapseSchedule::taken(double now_s) {
  ++taken_;
  ++next_;
  // slots whose time has already passed entirely are skipped, not taken late
  while (next_ < slots_ && now_s >= nextDue() + interval_s_) {
    ++next_;
    ++skipped_;
  }
}

bool TimelapseSchedule::finished(double now_s) const {
  return next_ >= slots_ && now_s >= start_s_ + (slots_ - 1) * interval_s_;
}
