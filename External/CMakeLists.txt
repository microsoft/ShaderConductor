# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

find_program(git_executable NAMES git git.exe git.cmd)
if(NOT git_executable)
    message(FATAL_ERROR "Failed to find git.")
endif()

set(cxxopts_REV "3f2d70530219e09fe7e563f86126b0d3b228a60d")
set(googletest_REV "440527a61e1c91188195f7de212c63c77e8f0a45")
set(SPIRV_Headers_REV "5dbc1c32182e17b8ab8e8158a802ecabaf35aad3")
set(SPIRV_Tools_REV "70f888131ede3d267cb19a413c9fef3fc51ccc22")
set(DirectXShaderCompiler_REV "b053bfbaae0d4d14e7d715ebe5592ade82388637")
set(SPIRV_Cross_REV "d19f30a90eff98fb4c33a2f24b05699c7de87ecb")

function(UpdateExternalLib name url rev)
    set(need_checkout FALSE)
    set(external_folder "${SC_ROOT_DIR}/External")
    set(external_lib_folder "${external_folder}/${name}")
    if(EXISTS "${external_lib_folder}/.git")
        message(STATUS "Updating ${name} to revision ${rev}...")
        execute_process(COMMAND "${git_executable}" "fetch" "origin" WORKING_DIRECTORY "${external_lib_folder}")
        execute_process(COMMAND "${git_executable}" "rev-parse" "HEAD" WORKING_DIRECTORY "${external_lib_folder}" OUTPUT_VARIABLE head_rev)
        string(STRIP ${head_rev} head_rev)
        if (${head_rev} STREQUAL ${rev})
            set(need_checkout FALSE)
        else()
            set(need_checkout TRUE)
        endif()
    else()
        message(STATUS "Cloning ${name} revision...")
        execute_process(COMMAND "${git_executable}" "clone" ${url} "-n" WORKING_DIRECTORY "${external_folder}")
        set(need_checkout TRUE)
    endif()
    if(need_checkout)
        message(STATUS "Checking out to revision ${rev}...")
        execute_process(COMMAND "${git_executable}" "checkout" "-q" ${rev} WORKING_DIRECTORY "${external_lib_folder}")
    endif()

    set(${ARGV3} ${need_checkout} PARENT_SCOPE)
endfunction()

function(ApplyPatch name patch)
    set(external_folder "${SC_ROOT_DIR}/External")
    set(external_lib_folder "${external_folder}/${name}")

    execute_process(COMMAND "${git_executable}" "apply" "--check" "--ignore-space-change" "${patch}" WORKING_DIRECTORY ${external_lib_folder} RESULT_VARIABLE checkout_err)
    if(NOT checkout_err)
        message(STATUS "Applying ${patch}...")
        execute_process(COMMAND "${git_executable}" "am" "--ignore-space-change" "${patch}" WORKING_DIRECTORY ${external_lib_folder})
    endif()
endfunction()

UpdateExternalLib("cxxopts" "https://github.com/jarro2783/cxxopts.git" ${cxxopts_REV})
UpdateExternalLib("googletest" "https://github.com/google/googletest.git" ${googletest_REV})
UpdateExternalLib("SPIRV-Headers" "https://github.com/KhronosGroup/SPIRV-Headers.git" ${SPIRV_Headers_REV})
UpdateExternalLib("SPIRV-Tools" "https://github.com/KhronosGroup/SPIRV-Tools.git" ${SPIRV_Tools_REV})
UpdateExternalLib("DirectXShaderCompiler" "https://github.com/Microsoft/DirectXShaderCompiler.git" ${DirectXShaderCompiler_REV} need_patch)
if(need_patch)
    foreach(patch "0001-Fix-a-couple-warnings-in-code")
        ApplyPatch("DirectXShaderCompiler" "${CMAKE_CURRENT_SOURCE_DIR}/Patches/${patch}.patch")
    endforeach()
endif()
UpdateExternalLib("SPIRV-Cross" "https://github.com/KhronosGroup/SPIRV-Cross.git" ${SPIRV_Cross_REV})

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(googletest EXCLUDE_FROM_ALL)
foreach(target
    "gtest" "gtest_main")
    set_target_properties(${target} PROPERTIES FOLDER "External/googletest")
endforeach()

add_subdirectory(SPIRV-Headers EXCLUDE_FROM_ALL)
foreach(target
    "install-headers" "SPIRV-Headers-example" "SPIRV-Headers-example-1.1")
    set_target_properties(${target} PROPERTIES FOLDER "External/SPIRV-Headers")
endforeach()

set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "" FORCE)
set(SPIRV_TOOLS_EXTRA_DEFINITIONS /D_ITERATOR_DEBUG_LEVEL=0)
add_subdirectory(SPIRV-Tools EXCLUDE_FROM_ALL)
foreach(target
    "core_tables" "enum_string_mapping" "extinst_tables"
    "spirv-tools-pkg-config" "spirv-tools-shared-pkg-config"
    "spirv-tools-build-version" "spirv-tools-header-DebugInfo"
    "SPIRV-Tools-link" "SPIRV-Tools-shared"
    "spirv-tools-header-OpenCLDebugInfo100" "spirv-tools-vimsyntax" "spv-tools-cldi100" "spv-tools-debuginfo" "spv-tools-spv-amd-gs"
    "spv-tools-spv-amd-sb" "spv-tools-spv-amd-sevp" "spv-tools-spv-amd-stm")
    get_target_property(vsFolder ${target} FOLDER)
    if(NOT vsFolder)
        set(vsFolder "")
    endif()
    set_target_properties(${target} PROPERTIES FOLDER "External/SPIRV-Tools/${vsFolder}")
endforeach()

set(ENABLE_SPIRV_CODEGEN ON CACHE BOOL "" FORCE)
set(CLANG_ENABLE_ARCMT OFF CACHE BOOL "" FORCE)
set(CLANG_ENABLE_STATIC_ANALYZER OFF CACHE BOOL "" FORCE)
set(CLANG_INCLUDE_TESTS OFF CACHE BOOL "" FORCE)
set(LLVM_INCLUDE_TESTS OFF CACHE BOOL "" FORCE)
set(HLSL_INCLUDE_TESTS OFF CACHE BOOL "" FORCE)
set(HLSL_BUILD_DXILCONV OFF CACHE BOOL "" FORCE)
set(LLVM_TARGETS_TO_BUILD "None" CACHE STRING "" FORCE)
set(LLVM_INCLUDE_DOCS OFF CACHE BOOL "" FORCE)
set(LLVM_INCLUDE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LIBCLANG_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(LLVM_OPTIMIZED_TABLEGEN OFF CACHE BOOL "" FORCE)
set(LLVM_REQUIRES_EH ON CACHE BOOL "" FORCE)
set(LLVM_APPEND_VC_REV ON CACHE BOOL "" FORCE)
set(LLVM_ENABLE_RTTI ON CACHE BOOL "" FORCE)
set(LLVM_ENABLE_EH ON CACHE BOOL "" FORCE)
set(LLVM_ENABLE_TERMINFO OFF CACHE BOOL "" FORCE)
set(LLVM_DEFAULT_TARGET_TRIPLE "dxil-ms-dx" CACHE STRING "" FORCE)
set(CLANG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLVM_REQUIRES_RTTI ON CACHE BOOL "" FORCE)
set(CLANG_CL OFF CACHE BOOL "" FORCE)
if(SC_ARCH_NAME STREQUAL "x86")
    set(DXC_BUILD_ARCH "Win32" CACHE STRING "" FORCE)
elseif(SC_ARCH_NAME STREQUAL "arm")
    set(DXC_BUILD_ARCH "ARM" CACHE STRING "" FORCE)
elseif(SC_ARCH_NAME STREQUAL "arm64")
    set(DXC_BUILD_ARCH "ARM64" CACHE STRING "" FORCE)
else()
    set(DXC_BUILD_ARCH "${SC_ARCH_NAME}" CACHE STRING "" FORCE)
endif()
set(SPIRV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "" FORCE)
set(SPIRV_SKIP_TESTS ON CACHE BOOL "" FORCE)
add_subdirectory(DirectXShaderCompiler EXCLUDE_FROM_ALL)
foreach(target
    "clang" "dxc"
    "clangAnalysis" "clangAST" "clangASTMatchers" "clangBasic" "clangCodeGen" "clangDriver" "clangEdit" "clangFormat" "clangFrontend"
    "clangFrontendTool" "clangIndex" "clangLex" "clangParse" "clangRewrite" "clangRewriteFrontend" "clangSema" "clangSPIRV" "clangTooling"
    "clangToolingCore" "dxcompiler" "libclang"
    "ClangAttrClasses" "ClangAttrDump" "ClangAttrHasAttributeImpl" "ClangAttrImpl" "ClangAttrList" "ClangAttrParsedAttrImpl"
    "ClangAttrParsedAttrKinds" "ClangAttrParsedAttrList" "ClangAttrParserStringSwitches" "ClangAttrPCHRead" "ClangAttrPCHWrite"
    "ClangAttrSpellingListIndex" "ClangAttrTemplateInstantiate" "ClangAttrVisitor" "ClangCommentCommandInfo" "ClangCommentCommandList"
    "ClangCommentHTMLNamedCharacterReferences" "ClangCommentHTMLTags" "ClangCommentHTMLTagsProperties" "ClangCommentNodes" "ClangDeclNodes"
    "ClangDiagnosticAnalysis" "ClangDiagnosticAST" "ClangDiagnosticComment" "ClangDiagnosticCommon" "ClangDiagnosticDriver"
    "ClangDiagnosticFrontend" "ClangDiagnosticGroups" "ClangDiagnosticIndexName" "ClangDiagnosticLex" "ClangDiagnosticParse"
    "ClangDiagnosticSema" "ClangDiagnosticSerialization" "ClangStmtNodes"
    "LLVMAnalysis" "LLVMAsmParser" "LLVMBitReader" "LLVMBitWriter" "LLVMCore" "LLVMDxcSupport" "LLVMDXIL" "LLVMDxilContainer"
    "LLVMDxilPIXPasses" "LLVMDxilRootSignature" "LLVMDxrFallback" "LLVMHLSL" "LLVMInstCombine" "LLVMipa" "LLVMipo" "LLVMIRReader"
    "LLVMLinker" "LLVMLTO" "LLVMMSSupport" "LLVMOption" "LLVMPasses" "LLVMPassPrinters" "LLVMProfileData" "LLVMScalarOpts" "LLVMSupport"
    "LLVMTableGen" "LLVMTarget" "LLVMTransformUtils" "LLVMVectorize"
    "ClangDriverOptions" "DxcEtw" "intrinsics_gen" "TablegenHLSLOptions"
    "clang-tblgen" "GIT_COMMIT_INFO_ALWAYS_REBUILD" "llvm-tblgen" "hlsl_dxcversion_autogen" "hlsl_version_autogen")
    get_target_property(vsFolder ${target} FOLDER)	
    if(NOT vsFolder)
        set(vsFolder "")
    endif()
    set_target_properties(${target} PROPERTIES FOLDER "External/DirectXShaderCompiler/${vsFolder}")
endforeach()
if(WIN32)
    foreach(target
        "dndxc" "dxa" "dxl" "dxopt" "dxr" "dxv"
        "d3dcompiler_dxc_bridge" "dxlib_sample" "dxrfallbackcompiler"
        "dxexp" "LLVMDxilDia")
        get_target_property(vsFolder ${target} FOLDER)
        if(NOT vsFolder)
            set(vsFolder "")
        endif()
        set_target_properties(${target} PROPERTIES FOLDER "External/DirectXShaderCompiler/${vsFolder}")
    endforeach()
endif()

add_subdirectory(SPIRV-Cross EXCLUDE_FROM_ALL)
foreach(target
    "spirv-cross" "spirv-cross-core" "spirv-cross-cpp" "spirv-cross-glsl" "spirv-cross-hlsl" "spirv-cross-msl" "spirv-cross-reflect"
    "spirv-cross-util")
    set_target_properties(${target} PROPERTIES FOLDER "External/SPIRV-Cross")
endforeach()

foreach(target
    "SPIRV-Tools" "SPIRV-Tools-opt")
    set_target_properties(${target} PROPERTIES FOLDER "External/SPIRV-Tools/SPIRV-Tools libraries")
endforeach()