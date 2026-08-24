cmake_minimum_required(VERSION 3.28)

project(blobcat VERSION 0.0.1)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")

option(BLOBCAT_BUILD_EXECUTABLE "Build the executable" ON)

add_library(blobcat STATIC src/blobcat.c)

if(BLOBCAT_BUILD_EXECUTABLE)
  add_executable(blobcat_exe src/cli.c)
  set_target_properties(blobcat_exe PROPERTIES OUTPUT_NAME blobcat)
  target_link_libraries(blobcat_exe PRIVATE blobcat)
endif()
