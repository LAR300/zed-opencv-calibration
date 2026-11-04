#pragma once

// Reprojection error conventions (auditoria §3.4). OpenCV's calibrateCamera
// and stereoCalibrate return sqrt(sum |e|^2 / corners): a per-corner 2D RMS.
// The Bias Ratio needs the mean squared error per image coordinate, which is
// RMS^2 / 2. Every report names the convention it uses.
// Spec: .ai/specs/saida-calibracao/spec.md (R3, R4)

#include <cmath>
#include <limits>
#include <vector>

#include <opencv2/core.hpp>

// Per-corner RMS of a set of views, each weighted by its number of corners.
// NaN when there is nothing to aggregate or the sizes differ.
inline double aggregateRms(const std::vector<double>& rms_per_view,
                           const std::vector<int>& corners_per_view) {
  if (rms_per_view.empty() || rms_per_view.size() != corners_per_view.size())
    return std::numeric_limits<double>::quiet_NaN();
  double sum = 0.0;
  long long corners = 0;
  for (size_t i = 0; i < rms_per_view.size(); i++) {
    sum += corners_per_view[i] * rms_per_view[i] * rms_per_view[i];
    corners += corners_per_view[i];
  }
  if (corners <= 0) return std::numeric_limits<double>::quiet_NaN();
  return std::sqrt(sum / static_cast<double>(corners));
}

inline double msePerCoordinate(double rms_per_corner) {
  return rms_per_corner * rms_per_corner / 2.0;
}

// Per-corner RMS of 2D residuals (projected - detected), as OpenCV computes it
inline double rmsPerCorner(const std::vector<cv::Point2d>& residuals) {
  if (residuals.empty()) return std::numeric_limits<double>::quiet_NaN();
  double sum = 0.0;
  for (const auto& e : residuals) sum += e.x * e.x + e.y * e.y;
  return std::sqrt(sum / static_cast<double>(residuals.size()));
}
