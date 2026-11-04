#pragma once

// Progress through a pose script during a capture, and the save rule of the
// script mode (.ai/specs/roteiro-poses R8, R10, R11). OpenCV only, no GUI: the
// capture tool calls it and shows what it returns.

#include <string>
#include <vector>

#include "pose_script.hpp"

enum class ScriptAction { SAVED, OUT_OF_TOLERANCE, SKIPPED, BACK };
const char* scriptActionName(ScriptAction action);  // "saved", "out_of_tolerance", ...

struct ScriptEvent {
  std::string pose_id;
  ScriptAction action = ScriptAction::SAVED;
  int n = -1;  // the pair number, for the two saving actions
};

class ScriptRunner {
 public:
  explicit ScriptRunner(PoseScript script) : script_(std::move(script)) {}

  bool finished() const { return index_ >= static_cast<int>(script_.poses.size()); }
  int index() const { return index_; }
  int size() const { return static_cast<int>(script_.poses.size()); }
  const ScriptPose& current() const { return script_.poses.at(index_); }
  const PoseScript& script() const { return script_; }

  // A pair 'n' was saved for the current pose: record it and go to the next
  void saved(int n, bool out_of_tolerance);
  // 'k': the current pose is skipped
  void skip();
  // 'b': back one pose, nothing deleted; false at the first pose
  bool back();

  const std::vector<ScriptEvent>& events() const { return events_; }

  // Per pose, its last saving or skipping action; poses never reached are pending
  struct Summary {
    std::vector<std::string> saved, out_of_tolerance, skipped, pending;
  };
  Summary summary() const;

 private:
  PoseScript script_;
  int index_ = 0;
  std::vector<ScriptEvent> events_;
};

// R8 / R10: what a key press does in the script mode. 's' saves only when every
// condition holds; 'o' saves out of tolerance when the pose is not feasible,
// but still needs the tags and a still, sharp target.
struct SaveConditions {
  PoseCheck pose;            // the measured quantities against the expected pose
  bool pose_known = false;   // false: no PnP pose (too few tags, ill-conditioned)
  bool enough_tags = false;
  bool still = false;        // captura-estatica: stable for the window
  bool sharp = false;        // captura-estatica: target sharpness above the threshold
};
struct SaveDecision {
  bool save = false;
  bool out_of_tolerance = false;
  std::vector<std::string> reasons;  // why not, for the panel
};
SaveDecision decideScriptSave(char key, const SaveConditions& c);
