# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set(googletest_REV "440527a61e1c91188195f7de212c63c77e8f0a45")

UpdateExternalLib("googletest" "https://github.com/google/googletest.git" ${googletest_REV})

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(googletest EXCLUDE_FROM_ALL)
foreach(target
    "gtest" "gtest_main")
    set_target_properties(${target} PROPERTIES FOLDER "External/googletest")
endforeach()
