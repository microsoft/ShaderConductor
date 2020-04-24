# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set(SPIRV_Headers_REV "f8bf11a0253a32375c32cad92c841237b96696c0")

UpdateExternalLib("SPIRV-Headers" "https://github.com/KhronosGroup/SPIRV-Headers.git" ${SPIRV_Headers_REV})

add_subdirectory(SPIRV-Headers EXCLUDE_FROM_ALL)
foreach(target
    "install-headers" "SPIRV-Headers-example" "SPIRV-Headers-example-1.1")
    set_target_properties(${target} PROPERTIES FOLDER "External/SPIRV-Headers")
endforeach()
