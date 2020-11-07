cmake_minimum_required(VERSION 3.5.1)
project(StreamingInferenceExamples)
add_library(StreamingInferenceExamples INTERFACE)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(util_example
  ${CMAKE_CURRENT_LIST_DIR}/Util.cpp
)

add_dependencies(util_example cereal)
# add_dependencies(util_example grpc)
add_dependencies(util_example streaming_inference_common)

find_package(Threads REQUIRED)

target_include_directories(
  util_example
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${cereal_INCLUDE_DIRS}
    ${grpc_INCLUDE_DIRS}
    ${wav2letter-inference_SOURCE_DIR}
    ${wav2letter++_SOURCE_DIR}/src
)

target_link_libraries(
  util_example
  PUBLIC
    streaming_inference_common
)

# audio_to_words_example library is used by the example executables
add_library(audio_to_words_example
  ${CMAKE_CURRENT_LIST_DIR}/AudioToWords.cpp
)

target_include_directories(
  audio_to_words_example
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${wav2letter-inference_SOURCE_DIR}
)

target_link_libraries(
  audio_to_words_example
  PUBLIC
    util_example
    streaming_inference_modules_nn_backend
    streaming_inference_decoder
    decoder-library
    Threads::Threads
)

function(build_example TARGET SRCFILE)
  message(STATUS "Examples: add executable ${TARGET}")
  add_executable(${TARGET}
    ${SRCFILE}
  )
  add_dependencies(StreamingInferenceExamples ${TARGET})
  target_include_directories(
    ${TARGET}
    PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}
      ${GFLAGS_INCLUDE_DIRS}
  )
  target_link_libraries(
    ${TARGET}
    PRIVATE
      streaming_inference_modules_nn_backend
      streaming_inference_modules_feature
      audio_to_words_example
      ${GFLAGS_LIBRARIES}
  )
endfunction(build_example)

function(build_server TARGET SRCFILE)
  if(NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  else()
    add_definitions(-D_WIN32_WINNT=0x600)
  endif()

  find_package(Threads REQUIRED)

  if(GRPC_AS_SUBMODULE)
    add_subdirectory(../.. ${CMAKE_CURRENT_BINARY_DIR}/grpc EXCLUDE_FROM_ALL)
    message(STATUS "Using gRPC via add_subdirectory.")

    set(_PROTOBUF_LIBPROTOBUF libprotobuf)
    set(_REFLECTION grpc++_reflection)
    if(CMAKE_CROSSCOMPILING)
      find_program(_PROTOBUF_PROTOC protoc)
    else()
      set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
    endif()
    set(_GRPC_GRPCPP grpc++)
    if(CMAKE_CROSSCOMPILING)
      find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
    else()
      set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)
    endif()
  elseif(GRPC_FETCHCONTENT)
    message(STATUS "Using gRPC via add_subdirectory (FetchContent).")
    include(FetchContent)
    FetchContent_Declare(
      grpc
      GIT_REPOSITORY https://github.com/grpc/grpc.git
      GIT_TAG        vGRPC_TAG_VERSION_OF_YOUR_CHOICE)
    FetchContent_MakeAvailable(grpc)

    set(_PROTOBUF_LIBPROTOBUF libprotobuf)
    set(_REFLECTION grpc++_reflection)
    set(_PROTOBUF_PROTOC $<TARGET_FILE:protoc>)
    set(_GRPC_GRPCPP grpc++)
    if(CMAKE_CROSSCOMPILING)
      find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
    else()
      set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)
    endif()
  else()
    message(STATUS "Skipping GRPC")
    find_package(protobuf CONFIG REQUIRED)
    message(STATUS "Using protobuf ${protobuf_VERSION}")
    set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
    find_program(_PROTOBUF_PROTOC protoc)
    message(STATUS "Using protc ${_PROTOBUF_PROTOC}")
    find_package(gRPC CONFIG REQUIRED)
    message(STATUS "Using gRPC ${gRPC_VERSION}")
    set(_REFLECTION gRPC::grpc++_reflection)
    set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
    set(_GRPC_GRPCPP gRPC::grpc++)
    message(STATUS "Using gRPC ${_GRPC_GRPCPP}")
  endif()

  get_filename_component(w2l_proto "wav2letter.proto" ABSOLUTE)
  get_filename_component(w2l_proto_path "${w2l_proto}" PATH)

  set(w2l_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/wav2letter.pb.cc")
  set(w2l_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/wav2letter.pb.h")
  set(w2l_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/wav2letter.grpc.pb.cc")
  set(w2l_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/wav2letter.grpc.pb.h")
  add_custom_command(
        OUTPUT "${w2l_proto_srcs}" "${w2l_proto_hdrs}" "${w2l_grpc_srcs}" "${w2l_grpc_hdrs}"
        COMMAND ${_PROTOBUF_PROTOC}
        ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
          --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
          -I "${w2l_proto_path}"
          --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
          "${w2l_proto}"
        DEPENDS "${w2l_proto}")

  include_directories("${CMAKE_CURRENT_BINARY_DIR}")

  add_executable(${TARGET} ${SRCFILE}
    ${w2l_proto_srcs}
    ${w2l_grpc_srcs})
  target_link_libraries(${TARGET}
    PRIVATE
      ${_REFLECTION}
      ${_GRPC_GRPCPP}
      ${_PROTOBUF_LIBPROTOBUF}
      streaming_inference_modules_nn_backend
      streaming_inference_modules_feature
      audio_to_words_example
      ${GFLAGS_LIBRARIES}
  )
  add_dependencies(StreamingInferenceExamples ${TARGET})
  target_include_directories(
    ${TARGET}
    PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}
      ${GFLAGS_INCLUDE_DIRS}
  )
endfunction(build_server)

build_example(interactive_streaming_asr_example
  ${CMAKE_CURRENT_LIST_DIR}/InteractiveStreamingASRExample.cpp)
build_example(simple_streaming_asr_example
  ${CMAKE_CURRENT_LIST_DIR}/SimpleStreamingASRExample.cpp)
build_example(multithreaded_streaming_asr_example
  ${CMAKE_CURRENT_LIST_DIR}/MultithreadedStreamingASRExample.cpp)
build_server(server
  ${CMAKE_CURRENT_LIST_DIR}/server.cc)

