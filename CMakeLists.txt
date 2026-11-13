cmake_minimum_required (VERSION 3.8)

project("RocketSim")

# Add all headers and code files
file(GLOB_RECURSE FILES_ROCKETSIM "${PROJECT_SOURCE_DIR}/src/*.cpp" "${PROJECT_SOURCE_DIR}/src/*.h")

# Only include bullet headers when using MSVC, otherwise just code files
file(GLOB_RECURSE FILES_BULLET "${PROJECT_SOURCE_DIR}/libsrc/bullet3-3.24/*.cpp" "${PROJECT_SOURCE_DIR}/libsrc/bullet3-3.24/*.h")

add_library(RocketSim STATIC ${FILES_ROCKETSIM} ${FILES_BULLET})

target_include_directories(RocketSim PUBLIC "${PROJECT_SOURCE_DIR}/src")

target_compile_definitions(RocketSim PUBLIC RS_DONT_LOG=1)

# FMA contraction: approved Tier B numerics change (single-rounding a*b+c). Pin the
# contraction mode explicitly so behavior doesn't depend on compiler defaults.
target_compile_options(RocketSim PRIVATE -mfma -ffp-contract=fast)

set_target_properties(RocketSim PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(RocketSim PROPERTIES CXX_STANDARD 20)
