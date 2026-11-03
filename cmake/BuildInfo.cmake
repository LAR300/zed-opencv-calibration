# zedcal_add_build_info(<target>): gives <target> a build_info.hpp with the
# commit of this repository, regenerated at every build of <target>.

set(ZEDCAL_BUILD_INFO_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/write_build_info.cmake")
set(ZEDCAL_REPO_DIR "${CMAKE_CURRENT_LIST_DIR}/..")

function(zedcal_add_build_info target)
  set(out_dir "${CMAKE_CURRENT_BINARY_DIR}/build_info_${target}")
  add_custom_target(${target}_build_info
    COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${ZEDCAL_REPO_DIR}
            -DOUTPUT=${out_dir}/build_info.hpp -P ${ZEDCAL_BUILD_INFO_SCRIPT}
    BYPRODUCTS ${out_dir}/build_info.hpp
    COMMENT "Recording the git commit in build_info.hpp"
    VERBATIM)
  add_dependencies(${target} ${target}_build_info)
  target_include_directories(${target} PRIVATE ${out_dir})
endfunction()
