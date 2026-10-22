# SPDX-License-Identifier: GPL-3.0-only
# FIZZ's core for the browser (site/build_wasm.sh copies this file to the root of its mirror of FM1VST:
# site/.build/src/CMakeLists.txt, next to the mirrored cmake/, core-api/ and core-glue/).
#
# Adapted from RemoteFM1's web/engines/native/cmake/CMakeLists.txt (GPL-3.0-only): it runs FM1VST's own
# cmake/FM1Cores.cmake (fm1_add_core) and the fizz glue core-glue/fizz/core.cmake under Emscripten's toolchain
# (emcmake), so the defines, the include paths and the units are exactly FM1VST's. Only these differ:
#   - core-glue/common/coro.c (ucontext) is replaced by the native engine's Asyncify coroutine (FM1WEB_CORO);
#   - the units' -O2 becomes FM1WEB_OPT, and they are built without -fPIC (one static wasm, no dynamic linking);
#   - FM1_PUBLIC_BUILD is OFF: the fizz core is GATED in FM1VST (a prototype), and this preview is its owner's
#     choice to publish it.
# Only the static library core_fizz is built; build_wasm.sh links it with wasm_glue.c.
cmake_minimum_required(VERSION 3.20)
project(fizz_web C CXX)

set(FIZZ_DIR "" CACHE PATH "the FIZZ (FMToneSeq) checkout")
set(FM1WEB_CORO "" CACHE FILEPATH "the coroutine that replaces core-glue/common/coro.c")
set(FM1WEB_OPT "-Os" CACHE STRING "the optimisation level of the units")
set(FM1_PUBLIC_BUILD OFF CACHE BOOL "keep the gated fizz core" FORCE)
set(CR_PYTHON "" CACHE FILEPATH "(unused: the fizz core has no generate step)")
if(NOT FIZZ_DIR)
    message(FATAL_ERROR "FIZZ_DIR is required")
endif()
set(MATH_LIB "")                                  # (Emscripten's libc has libm)

include(cmake/FM1Cores.cmake)
set(FM1_COMMON_SOURCES ${FM1_COMMON_DIR}/nor.c ${FM1_COMMON_DIR}/fifo.c ${FM1WEB_CORO})
set_source_files_properties(${FM1_COMMON_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-fno-common")

add_library(fm1testlib INTERFACE)

include(${CMAKE_SOURCE_DIR}/core-glue/fizz/core.cmake)
if(NOT TARGET core_fizz_obj)
    message(FATAL_ERROR "core-glue/fizz/core.cmake made no core (FIZZ_DIR=${FIZZ_DIR})")
endif()
get_target_property(_o core_fizz_obj COMPILE_OPTIONS)
list(TRANSFORM _o REPLACE "^-O2$" "${FM1WEB_OPT}")
set_target_properties(core_fizz_obj PROPERTIES COMPILE_OPTIONS "${_o}" POSITION_INDEPENDENT_CODE OFF)
# plutovg's rasteriser (third_party/plutovg/source/plutovg-ft-raster.c, FreeType's) recovers with setjmp / longjmp:
# Emscripten's default for that is JavaScript invoke_* trampolines, which a standalone module has no runtime for. Wasm
# exception handling does it inside the module (every current browser; build_wasm.sh links with the same flag).
target_compile_options(core_fizz_obj PRIVATE -sSUPPORT_LONGJMP=wasm)
