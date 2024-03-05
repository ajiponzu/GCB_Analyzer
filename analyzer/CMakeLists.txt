cmake_minimum_required(VERSION 3.0)

project("gcb-analyzer")   

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-std=c++17 -lpthread -Wall -Wextra -pedantic -Wcast-align -Wcast-qual -Wconversion")
set(CMAKE_CXX_FLAGS_DEBUG "-g3 -O0 -pg")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -s -DNDEBUG -march=native")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g3 -Og -pg")
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -s -DNDEBUG -march=native")

add_executable(${PROJECT_NAME}
  src/main.cpp
  src/ApiServer/Server.cpp
  src/GCB/Analyzer.cpp
  src/GCB/ImgFunc/ImgSize.cpp
  src/GCB/ImgFunc/ImgProc.cpp
)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fuse-ld=gold" COMPILER_SUPPORTS_GOLD)

if(COMPILER_SUPPORTS_GOLD)
  execute_process(COMMAND ${CMAKE_CXX_COMPILER} -fuse-ld=gold -Wl,--version OUTPUT_VARIABLE stdout ERROR_QUIET)
  if("${stdout}" MATCHES "GNU gold")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=gold")
  endif()
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS})
include_directories( . )


target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

find_package(Drogon CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon
  ${OpenCV_LIBS}
)