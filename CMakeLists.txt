cmake_minimum_required(VERSION 3.10)
project(TachyonTick CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find MPI
find_package(MPI REQUIRED)

# Include directories
include_directories(include)

# Add executable
add_executable(tachyon_tick
    src/main.cpp
    src/BitStream.cpp
    src/CompressedBlock.cpp
    src/Coordinator.cpp
    src/Worker.cpp
    src/TickGenerator.cpp
)

# Link MPI libraries
target_link_libraries(tachyon_tick PRIVATE MPI::MPI_CXX)

# ... (cmake_minimum_required, project, etc.)

# We are building a library now
add_library(tachyon_lib SHARED
    src/BitStream.cpp
    src/CompressedBlock.cpp
    src/TachyonApi.cpp # <-- A new file for our C API
)
# We no longer need the MPI parts for the library itself
# You can keep the old executable target for testing/compression
# add_executable(tachyon_compressor src/main.cpp ...)

# Link to C++ standard library
target_link_libraries(tachyon_lib PRIVATE stdc++)
