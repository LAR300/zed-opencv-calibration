# Writes build_info.hpp with the commit of the source tree and whether it had
# uncommitted changes. Run at every build (not only at configure time), so a
# binary never reports a stale commit. The file is rewritten only when its
# content changes, so an unchanged tree does not trigger a rebuild.
#
# Usage: cmake -DSOURCE_DIR=<repo> -DOUTPUT=<path/build_info.hpp> -P write_build_info.cmake
# Spec: .ai/specs/persistencia-captura/spec.md (condições de implementação)

find_package(Git QUIET)
set(commit "unknown")
set(dirty 1)
if(GIT_FOUND)
  execute_process(COMMAND ${GIT_EXECUTABLE} -C ${SOURCE_DIR} rev-parse HEAD
                  OUTPUT_VARIABLE git_commit OUTPUT_STRIP_TRAILING_WHITESPACE
                  RESULT_VARIABLE commit_result ERROR_QUIET)
  if(commit_result EQUAL 0)
    set(commit "${git_commit}")
    execute_process(COMMAND ${GIT_EXECUTABLE} -C ${SOURCE_DIR} status --porcelain
                            --untracked-files=no
                    OUTPUT_VARIABLE git_status RESULT_VARIABLE status_result ERROR_QUIET)
    if(status_result EQUAL 0 AND git_status STREQUAL "")
      set(dirty 0)
    endif()
  endif()
endif()

set(content "#pragma once
// Generated at build time by cmake/write_build_info.cmake. Do not edit.
#define ZEDCAL_GIT_COMMIT \"${commit}\"
#define ZEDCAL_GIT_DIRTY ${dirty}
")

if(EXISTS "${OUTPUT}")
  file(READ "${OUTPUT}" previous)
else()
  set(previous "")
endif()
if(NOT previous STREQUAL content)
  file(WRITE "${OUTPUT}" "${content}")
endif()
