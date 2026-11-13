cmake_minimum_required(VERSION 3.26)

project(RocketSimScratch)

# Gather library sources (Bullet + RocketSim src), excluding the Python bindings.
file(GLOB_RECURSE ROCKETSIM_BULLET3_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../libsrc/bullet3-3.24/*.cpp)
file(GLOB_RECURSE ROCKETSIM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/*.cpp)

add_library(RocketSim STATIC ${ROCKETSIM_BULLET3_SOURCES} ${ROCKETSIM_SOURCES})

target_compile_features(RocketSim PRIVATE cxx_std_20)
target_include_directories(RocketSim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_include_directories(RocketSim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../libsrc)
target_compile_definitions(RocketSim PRIVATE
	RS_DONT_LOG=1
	XXH_INLINE_ALL=1
	XXH_STATIC_LINKING_ONLY=1
)

add_executable(scratch ${CMAKE_CURRENT_SOURCE_DIR}/../scratch.cpp)
set_target_properties(scratch PROPERTIES CXX_STANDARD 20)
target_link_libraries(scratch RocketSim)
