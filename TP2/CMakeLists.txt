cmake_minimum_required(VERSION 3.0.0)
project(GIF-4104-TP2 VERSION 0.1.0)

set(CMAKE_CXX_COMPILE_FEATURES "-03")

find_library(gmp gmp)
find_library(gmpxx gmpxx)
find_package(OpenMP REQUIRED)

include_directories(include)

include(CTest)
enable_testing()

add_executable(GIF-4104-TP2 
    src/miller-rabin-gmp.cpp
    src/main.cpp)

target_link_libraries(GIF-4104-TP1 PRIVATE OpenMP::OpenMP_CXX gmp gmpxx)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
