# SPDX-License-Identifier: MPL-2.0
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# FIZZ's core for Apple platforms (iOS, the simulator, macOS). ios/engine/build_core.sh copies this file to the root of
# its mirror of FM1VST (ios/engine/.build/src: cmake/, core-api/, core-glue/) and configures it once per slice. It runs
# FM1VST's own cmake/FM1Cores.cmake and core-glue/fizz/core.cmake, so the sources, defines and include paths are
# exactly the plugin's. Only these differ:
#   - no coroutine (core-glue/common/coro.c: FIZZ's core never yields) and no audio adapter (fifo.c: it renders the
#     ABI's 128-frame blocks itself): of FM1VST's common units only nor.c is compiled in;
#   - the units' -O2 becomes FIZZ_IOS_OPT (-Os);
#   - FM1_PUBLIC_BUILD is OFF: the fizz core is gated in FM1VST (a prototype); the owner builds it for his own app.
# Only the static library core_fizz is built; build_core.sh links it with fizzhost.c into one object per slice.
cmake_minimum_required(VERSION 3.20)
project(fizz_ios C)

set(FIZZ_DIR "" CACHE PATH "the FIZZ (FMToneSeq) checkout")
set(FIZZ_IOS_OPT "-Os" CACHE STRING "the optimisation level of the units")
set(FM1_PUBLIC_BUILD OFF CACHE BOOL "keep the gated fizz core" FORCE)
set(CR_PYTHON "" CACHE FILEPATH "(unused: the fizz core has no generate step)")
if(NOT FIZZ_DIR)
    message(FATAL_ERROR "FIZZ_DIR is required")
endif()
set(MATH_LIB "")                                  # (libSystem has libm)

include(cmake/FM1Cores.cmake)
set(FM1_COMMON_SOURCES ${FM1_COMMON_DIR}/nor.c)
set_source_files_properties(${FM1_COMMON_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-fno-common")

add_library(fm1testlib INTERFACE)

include(${CMAKE_SOURCE_DIR}/core-glue/fizz/core.cmake)
if(NOT TARGET core_fizz_obj)
    message(FATAL_ERROR "core-glue/fizz/core.cmake made no core (FIZZ_DIR=${FIZZ_DIR})")
endif()
get_target_property(_o core_fizz_obj COMPILE_OPTIONS)
list(TRANSFORM _o REPLACE "^-O2$" "${FIZZ_IOS_OPT}")
set_target_properties(core_fizz_obj PROPERTIES COMPILE_OPTIONS "${_o}")
