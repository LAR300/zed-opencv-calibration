#include "script_runner.hpp"
#include "test_utils.hpp"

namespace {

PoseScript threePoses() {
  PoseScript s;
  s.name = "t";
  for (const char* id : {"B1-P-TL", "B1-P-TC", "B1-P-TR"}) {
    ScriptPose p;
    p.id = id;
    p.block = 1;
    s.poses.push_back(p);
  }
  return s;
}

SaveConditions allGood() {
  SaveConditions c;
  c.pose.fraction = c.pose.position = c.pose.tilt_h = c.pose.tilt_v = c.pose.rotation = true;
  c.pose_known = c.enough_tags = c.still = c.sharp = true;
  return c;
}

}  // namespace

TEST(script_runner_follows_the_order_and_records_every_action) {
  ScriptRunner r(threePoses());
  CHECK(r.current().id == "B1-P-TL");
  r.saved(0, false);
  CHECK(r.current().id == "B1-P-TC");
  r.skip();
  CHECK(r.current().id == "B1-P-TR");
  CHECK(r.back());                  // back to TC, nothing deleted
  CHECK(r.current().id == "B1-P-TC");
  r.saved(1, true);                 // the new pair gets the next n
  r.saved(2, false);
  CHECK(r.finished());
  CHECK(r.events().size() == 5);
  CHECK(r.events()[2].action == ScriptAction::BACK);
  CHECK(r.events()[3].n == 1);
  const auto s = r.summary();
  CHECK(s.saved.size() == 2 && s.saved[0] == "B1-P-TL" && s.saved[1] == "B1-P-TR");
  CHECK(s.out_of_tolerance.size() == 1 && s.out_of_tolerance[0] == "B1-P-TC");  // last action
  CHECK(s.skipped.empty() && s.pending.empty());
  CHECK(std::string(scriptActionName(ScriptAction::OUT_OF_TOLERANCE)) == "out_of_tolerance");
}

TEST(script_runner_back_at_the_start_and_pending_poses) {
  ScriptRunner r(threePoses());
  CHECK(!r.back());
  r.skip();
  const auto s = r.summary();
  CHECK(s.skipped.size() == 1 && s.pending.size() == 2);
  r.saved(0, false);
  r.saved(1, false);
  r.saved(2, false);  // past the end: ignored
  CHECK(r.finished() && r.events().size() == 3);
}

TEST(script_save_needs_every_condition_for_s) {
  CHECK(decideScriptSave('s', allGood()).save);
  CHECK(!decideScriptSave('s', allGood()).out_of_tolerance);
  CHECK(!decideScriptSave('x', allGood()).save);

  SaveConditions c = allGood();
  c.pose.tilt_v = false;
  SaveDecision d = decideScriptSave('s', c);
  CHECK(!d.save && d.reasons.size() == 1);
  CHECK(d.reasons[0] == "out of tolerance: tilt about the vertical axis");

  d = decideScriptSave('o', c);  // 'o' saves the infeasible pose, flagged
  CHECK(d.save && d.out_of_tolerance);

  c = allGood();
  c.still = false;
  CHECK(!decideScriptSave('s', c).save);
  CHECK(!decideScriptSave('o', c).save);  // never a moving target
  c = allGood();
  c.pose_known = false;
  CHECK(!decideScriptSave('s', c).save);
  CHECK(decideScriptSave('o', c).save);
  c.enough_tags = false;
  CHECK(!decideScriptSave('o', c).save);
}
