# SPDX-License-Identifier: MIT
# Audit-only consumer: compiles original GPL sources, no plugin or installation.
cmake_minimum_required(VERSION 3.28)
project(PlayingSharedVisualAudit LANGUAGES CXX)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Audit targets Windows x64")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/shared-visual.cmake")
add_executable(shared-visual-audit "${PLAYING_SHARED_ROOT}/tests/visual-tests.cpp")
target_compile_options(shared-visual-audit PRIVATE /W4 /WX /utf-8)
target_link_libraries(shared-visual-audit PRIVATE playing-shared-visual)
enable_testing()
add_test(NAME audited-visual-text-contract COMMAND shared-visual-audit)
get_target_property(qt_library Qt6::Core IMPORTED_LOCATION_RELEASE)
get_filename_component(qt_bin "${qt_library}" DIRECTORY)
set_tests_properties(audited-visual-text-contract PROPERTIES TIMEOUT 40
  ENVIRONMENT "QT_QPA_PLATFORM=windows;QT_QPA_PLATFORM_PLUGIN_PATH=${qt_bin}/../plugins/platforms"
  ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${qt_bin}")
