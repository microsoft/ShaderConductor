# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

if(CMAKE_C_COMPILER_ID MATCHES MSVC)
	set(CMAKE_CXX_FLAGS "/W4 /WX /EHsc /MP /bigobj /Zc:throwingNew /Zc:strictStrings /Zc:rvalueCast /Gw /std:c++14 /permissive-")
	if(MSVC_VERSION GREATER 1912)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:externConstexpr")
	endif()
	set(CMAKE_C_FLAGS ${CMAKE_CXX_FLAGS})
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /fp:fast /Ob2 /GL /Qpar")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /fp:fast /Ob2 /GL /Qpar")
	set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /fp:fast /Ob1 /GL /Qpar")
	foreach(flagVar
		CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_MINSIZEREL)
		set(${flagVar} "${${flagVar}} /GS-")
	endforeach()

	foreach(flagVar
		CMAKE_EXE_LINKER_FLAGS CMAKE_SHARED_LINKER_FLAGS CMAKE_MODULE_LINKER_FLAGS)
		set(${flagVar} "/pdbcompress")
	endforeach()
	foreach(flagVar
		CMAKE_EXE_LINKER_FLAGS_DEBUG CMAKE_SHARED_LINKER_FLAGS_DEBUG)
		set(${flagVar} "/DEBUG:FASTLINK")
	endforeach()
	foreach(flagVar
		CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO)
		set(${flagVar} "/DEBUG:FASTLINK /INCREMENTAL:NO /LTCG:incremental /OPT:REF /OPT:ICF")
	endforeach()
	foreach(flagVar
		CMAKE_EXE_LINKER_FLAGS_MINSIZEREL CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL CMAKE_EXE_LINKER_FLAGS_RELEASE CMAKE_SHARED_LINKER_FLAGS_RELEASE)
		set(${flagVar} "/INCREMENTAL:NO /LTCG /OPT:REF /OPT:ICF")
	endforeach()
	foreach(flagVar
		CMAKE_MODULE_LINKER_FLAGS_RELEASE CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL)
		set(${flagVar} "/INCREMENTAL:NO /LTCG")
	endforeach()
	foreach(flagVar
		CMAKE_STATIC_LINKER_FLAGS_RELEASE CMAKE_STATIC_LINKER_FLAGS_MINSIZEREL)
		set(${flagVar} "${${flagVar}} /LTCG")
	endforeach()
	set(CMAKE_STATIC_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_STATIC_LINKER_FLAGS_RELWITHDEBINFO} /LTCG:incremental")

	add_definitions(-DWIN32 -D_WINDOWS)
else()
	foreach(flagVar
		CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
		set(${flagVar} "${${flagVar}} -W -Wall -Werror")
		if(NOT (ANDROID OR IOS))
			set(${flagVar} "${${flagVar}} -march=core2 -msse2")
		endif()
	endforeach()
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z -Wno-missing-field-initializers")
	if(CMAKE_C_COMPILER_ID MATCHES Clang)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-inconsistent-missing-override -Wno-missing-braces")
		if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
		endif()
	else()
		if(MINGW)
			foreach(flagVar
				CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
				set(${flagVar} "${${flagVar}} -Wa,-mbig-obj")
			endforeach()
		endif()
	endif()
	set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG -g -O0")
	set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O2")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-DNDEBUG -g -O2")
	set(CMAKE_CXX_FLAGS_MINSIZEREL "-DNDEBUG -Os")

	if(NOT SC_ARCH_NAME)
		if((CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64") OR (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64"))
			set(SC_ARCH_NAME "x64")
		else()
			set(SC_ARCH_NAME "x86")
		endif()
	endif()

	if(SC_ARCH_NAME STREQUAL "x64")
		foreach(flagVar
			CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
			set(${flagVar} "${${flagVar}} -m64")
		endforeach()
		if(NOT MSVC)
			foreach(flagVar
				CMAKE_SHARED_LINKER_FLAGS CMAKE_MODULE_LINKER_FLAGS CMAKE_EXE_LINKER_FLAGS)
				set(${flagVar} "${${flagVar}} -m64")
			endforeach()
			if(WIN32)
				set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} --target=pe-x86-64")
			else()
				set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} --target=elf64-x86-64")
			endif()
		endif()
	else()
		foreach(flagVar
			CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
			set(${flagVar} "${${flagVar}} -m32")
		endforeach()
		if(NOT MSVC)
			foreach(flagVar
				CMAKE_SHARED_LINKER_FLAGS CMAKE_MODULE_LINKER_FLAGS CMAKE_EXE_LINKER_FLAGS)
				set(${flagVar} "${${flagVar}} -m32")
				if(WIN32)
					set(${flagVar} "${${flagVar}} -Wl,--large-address-aware")
				endif()
			endforeach()
			if(WIN32)
				set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} --target=pe-i386")
			else()
				set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} --target=elf32-i386")
			endif()
		endif()
	endif()
	if(NOT (${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
		foreach(flagVar
			CMAKE_SHARED_LINKER_FLAGS_RELEASE CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL
			CMAKE_MODULE_LINKER_FLAGS_RELEASE CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL
			CMAKE_EXE_LINKER_FLAGS_RELEASE CMAKE_EXE_LINKER_FLAGS_MINSIZEREL)
			set(${flagVar} "-s")
		endforeach()
	endif()
endif()

set(CMAKE_C_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
set(CMAKE_C_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
set(CMAKE_C_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})
set(CMAKE_C_FLAGS_MINSIZEREL ${CMAKE_CXX_FLAGS_MINSIZEREL})

if(CMAKE_C_COMPILER_ID MATCHES MSVC)
	# Required by DirectXShaderCompiler
	foreach(flagVar
		CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
		set(${flagVar} "${${flagVar}} /UNDEBUG")
	endforeach()
endif()
add_definitions(-DDBG -DSUPPORT_QUERY_GIT_COMMIT_INFO -DENABLE_SPIRV_CODEGEN
	-D_CRT_SECURE_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_NONSTDC_NO_WARNINGS
	-D_SCL_SECURE_NO_DEPRECATE -D_SCL_SECURE_NO_WARNINGS
	-D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
	-D_GNU_SOURCE)

add_subdirectory(Core)
add_subdirectory(Tests)
add_subdirectory(Tools)
