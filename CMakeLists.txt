#-------------------------------------------------------------------------------------------
# CMakeLists
#-------------------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.20)
project(TinyPool CXX)
set(CMAKE_CXX_STANDARD 17)

#-------------------------------------------------------------------------------------------
# Compiler Flags
#-------------------------------------------------------------------------------------------
aux_source_directory(${PROJECT_SOURCE_DIR}/src/ SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/test/ SRC)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
#-------------------------------------------------------------------------------------------
# Executable file definition
#-------------------------------------------------------------------------------------------
add_executable(test ${SRC})

#-------------------------------------------------------------------------------------------
# Include dirs
#-------------------------------------------------------------------------------------------
target_include_directories(test PUBLIC ${PROJECT_SOURCE_DIR}/include)
#include_directories(${PROJECT_SOURCE_DIR}/include)
