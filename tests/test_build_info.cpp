#include <cstdio>
#include <memory>
#include <string>

#include "build_info.hpp"
#include "test_utils.hpp"

namespace {

std::string run(const std::string& command) {
  std::string out;
  std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen(command.c_str(), "r"), pclose);
  if (!pipe) return out;
  char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe.get())) out += buffer;
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
  return out;
}

}  // namespace

TEST(build_info_has_the_current_commit) {
  // The binary was just built: its commit is the HEAD of the source tree
  const std::string head = run("git -C " ZEDCAL_SOURCE_DIR " rev-parse HEAD 2>/dev/null");
  const std::string commit = ZEDCAL_GIT_COMMIT;
  CHECK(commit.size() == 40);
  CHECK(commit == head);
}

TEST(build_info_dirty_flag_matches_the_tree) {
  const std::string status = run(
      "git -C " ZEDCAL_SOURCE_DIR " status --porcelain --untracked-files=no 2>/dev/null");
  CHECK((ZEDCAL_GIT_DIRTY == 1) == !status.empty());
}
