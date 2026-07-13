cmake_minimum_required(VERSION 3.15)

set(FPFMT_SOURCE "cpp" CACHE STRING "Which implementation to build: cpp (fpfmt.cpp) or c (fpfmt_c.c)")
set_property(CACHE FPFMT_SOURCE PROPERTY STRINGS cpp c)
option(FPFMT_WASM "Build for WebAssembly via Emscripten instead of the local machine -- fpfmt_c.c only" OFF)

if(FPFMT_WASM AND NOT FPFMT_SOURCE STREQUAL "c")
	message(FATAL_ERROR "FPFMT_WASM=ON requires FPFMT_SOURCE=c -- WASM is only supported for the portable C99 implementation (fpfmt_c.c), not fpfmt.cpp. Pass -DFPFMT_SOURCE=c.")
endif()

if(FPFMT_WASM AND NOT CMAKE_TOOLCHAIN_FILE)
	if(DEFINED ENV{EMSDK})
		set(CMAKE_TOOLCHAIN_FILE "$ENV{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake"
			CACHE FILEPATH "Emscripten toolchain file")
	else()
		message(FATAL_ERROR "FPFMT_WASM=ON needs the Emscripten toolchain. Either:\n"
			"  - source emsdk_env.sh so $EMSDK is set, then reconfigure, or\n"
			"  - configure with 'emcmake cmake ...', or\n"
			"  - pass -DCMAKE_TOOLCHAIN_FILE=/path/to/Emscripten.cmake directly")
	endif()
endif()

if(FPFMT_SOURCE STREQUAL "cpp")
	project(fpfmt LANGUAGES CXX)
elseif(FPFMT_SOURCE STREQUAL "c")
	project(fpfmt LANGUAGES C)
else()
	message(FATAL_ERROR "FPFMT_SOURCE must be 'cpp' or 'c' (got '${FPFMT_SOURCE}')")
endif()

message(STATUS "fpfmt: system: ${CMAKE_SYSTEM_NAME} (${CMAKE_SYSTEM_PROCESSOR})")

set(FPFMT_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(FPFMT_SOURCE STREQUAL "cpp")
	message(STATUS "fpfmt: compiler: ${CMAKE_CXX_COMPILER_ID} (${CMAKE_CXX_COMPILER})")

	add_library(fpfmt STATIC "${FPFMT_SRC_DIR}/fpfmt.cpp")
	set_target_properties(fpfmt PROPERTIES
		CXX_STANDARD 20
		CXX_STANDARD_REQUIRED ON)
else()
	message(STATUS "fpfmt: compiler: ${CMAKE_C_COMPILER_ID} (${CMAKE_C_COMPILER})")

	if(FPFMT_WASM)
		add_executable(fpfmt "${FPFMT_SRC_DIR}/fpfmt_c.c")
		target_link_options(fpfmt PRIVATE
			"SHELL:-s MODULARIZE=1"
			"SHELL:-s EXPORT_NAME=FpfmtModule"
			"SHELL:-s EXPORTED_FUNCTIONS=['_format','_parseText']"
			"SHELL:-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
			"SHELL:-s ENVIRONMENT=web")
	else()
		add_library(fpfmt STATIC "${FPFMT_SRC_DIR}/fpfmt_c.c")
	endif()

	set_target_properties(fpfmt PROPERTIES
		C_STANDARD 99
		C_STANDARD_REQUIRED ON
		C_EXTENSIONS OFF)
endif()

target_include_directories(fpfmt PUBLIC "${FPFMT_SRC_DIR}")

message(STATUS "fpfmt: building ${FPFMT_SOURCE}, WASM=${FPFMT_WASM}")
