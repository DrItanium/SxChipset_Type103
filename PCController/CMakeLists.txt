cmake_minimum_required(VERSION 3.10)

include(CheckIncludeFiles)
include(CheckIncludeFileCXX)
project(controller960 VERSION 1)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
find_package(Boost REQUIRED COMPONENTS system)
add_executable(controller960 main.cc)

install(TARGETS controller960 DESTINATION bin)
target_link_libraries(controller960 PUBLIC
		boost_system
		boost_program_options
		)
