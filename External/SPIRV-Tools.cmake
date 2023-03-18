# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set(SPIRV_Tools_REV "1589720e1065bd163fb8e812f268413b13755f7c")

UpdateExternalLib("SPIRV-Tools" "https://github.com/KhronosGroup/SPIRV-Tools.git" ${SPIRV_Tools_REV})

set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "" FORCE)
add_subdirectory(SPIRV-Tools EXCLUDE_FROM_ALL)

if(MSVC)
    target_compile_options(SPIRV-Tools-static
        PRIVATE
            /wd4819
    )
endif()

foreach(target
    "core_tables" "enum_string_mapping" "extinst_tables"
    "spirv-tools-pkg-config" "spirv-tools-shared-pkg-config"
    "spirv-tools-build-version" "spirv-tools-header-DebugInfo"
    "SPIRV-Tools-link" "SPIRV-Tools-shared"
    "spirv-tools-header-OpenCLDebugInfo100" "spirv-tools-header-NonSemanticShaderDebugInfo100"
    "spirv-tools-vimsyntax" "spv-tools-cldi100" "spv-tools-shdi100" "spv-tools-clspvreflection"
    "spv-tools-debuginfo" "spv-tools-spv-amd-gs" "spv-tools-spv-amd-sb" "spv-tools-spv-amd-sevp"
    "spv-tools-spv-amd-stm")
    get_target_property(vsFolder ${target} FOLDER)
    if(NOT vsFolder)
        set(vsFolder "")
    endif()
    set_target_properties(${target} PROPERTIES FOLDER "External/SPIRV-Tools/${vsFolder}")
endforeach()
