#include "script_runner.hpp"

#include <map>

const char* scriptActionName(ScriptAction action) {
  switch (action) {
    case ScriptAction::SAVED: return "saved";
    case ScriptAction::OUT_OF_TOLERANCE: return "out_of_tolerance";
    case ScriptAction::SKIPPED: return "skipped";
    case ScriptAction::BACK: return "back";
  }
  return "unknown";
}

void ScriptRunner::saved(int n, bool out_of_tolerance) {
  if (finished()) return;
  events_.push_back({current().id, out_of_tolerance ? ScriptAction::OUT_OF_TOLERANCE
                                                    : ScriptAction::SAVED, n});
  ++index_;
}

void ScriptRunner::skip() {
  if (finished()) return;
  events_.push_back({current().id, ScriptAction::SKIPPED, -1});
  ++index_;
}

bool ScriptRunner::back() {
  if (index_ == 0) return false;
  --index_;
  events_.push_back({current().id, ScriptAction::BACK, -1});
  return true;
}

ScriptRunner::Summary ScriptRunner::summary() const {
  std::map<std::string, ScriptAction> last;
  for (const auto& e : events_)
    if (e.action != ScriptAction::BACK) last[e.pose_id] = e.action;
  Summary s;
  for (const auto& p : script_.poses) {
    const auto it = last.find(p.id);
    if (it == last.end()) s.pending.push_back(p.id);
    else if (it->second == ScriptAction::SAVED) s.saved.push_back(p.id);
    else if (it->second == ScriptAction::OUT_OF_TOLERANCE) s.out_of_tolerance.push_back(p.id);
    else s.skipped.push_back(p.id);
  }
  return s;
}

SaveDecision decideScriptSave(char key, const SaveConditions& c) {
  SaveDecision d;
  if (key != 's' && key != 'o') return d;
  if (!c.enough_tags) d.reasons.push_back("not enough tags");
  if (!c.still) d.reasons.push_back("wait until the target is still");
  if (!c.sharp) d.reasons.push_back("target not sharp");
  if (key == 's') {
    if (!c.pose_known) {
      d.reasons.push_back("pose not available");
    } else {
      const std::pair<bool, const char*> checks[] = {
          {c.pose.fraction, "distance (width fraction)"}, {c.pose.position, "position"},
          {c.pose.tilt_h, "tilt about the horizontal axis"},
          {c.pose.tilt_v, "tilt about the vertical axis"}, {c.pose.rotation, "rotation"}};
      for (const auto& [ok, name] : checks)
        if (!ok) d.reasons.push_back(std::string("out of tolerance: ") + name);
    }
  }
  d.save = d.reasons.empty();
  d.out_of_tolerance = d.save && key == 'o';
  return d;
}
