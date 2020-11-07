cmake_minimum_required(VERSION 3.5.1)

include(ExternalProject)

set(grpc_URL https://github.com/grpc/grpc)
set(grpc_BUILD ${CMAKE_CURRENT_BINARY_DIR}/grpc)
set(grpc_TAG 414bb8322de2411eee1f4e841ff29d887bec7884) # `develop` branch, commit

ExternalProject_Add(
  grpc
  PREFIX grpc
  GIT_REPOSITORY ${grpc_URL}
  GIT_TAG ${grpc_TAG}
  BUILD_IN_SOURCE 1
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  CMAKE_CACHE_ARGS
    -DCMAKE_BUILD_TYPE:STRING=Release

)
ExternalProject_Get_Property(grpc source_dir)
set(GRPC_SOURCE_DIR ${source_dir})
ExternalProject_Get_Property(grpc binary_dir)
set(GRPC_BINARY_DIR ${binary_dir})

# Include dir. dependent on build or install
set(GRPC_INCLUDE_DIR
  $<BUILD_INTERFACE:${GRPC_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${GRPC_INSTALL_PATH}> # see root CMakeLists.txt
  )

  set(GRPC_INCLUDE_DIRS ${GRPC_INCLUDE_DIR})
