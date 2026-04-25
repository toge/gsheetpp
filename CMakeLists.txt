cmake_minimum_required(VERSION 3.25)
project(gsheetpp
  VERSION 0.1.0
  LANGUAGES CXX
)

include(CheckCXXCompilerFlag)
include(CheckIPOSupported)

check_cxx_compiler_flag("-std=c++26" COMPILER_SUPPORTS_CXX26)
check_cxx_compiler_flag("-std=c++23" COMPILER_SUPPORTS_CXX23)
check_cxx_compiler_flag("-std=c++20" COMPILER_SUPPORTS_CXX20)

if (COMPILER_SUPPORTS_CXX26)
  set(STD_CPP cxx_std_26)
elseif (COMPILER_SUPPORTS_CXX23)
  set(STD_CPP cxx_std_23)
elseif (COMPILER_SUPPORTS_CXX20)
  set(STD_CPP cxx_std_20)
else ()
  message(FATAL_ERROR "A C++20 or newer compiler is required.")
endif ()

if (CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -g3 -O3 -march=native")
endif ()

if (MSVC)
  if (CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else ()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif ()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Ox")
endif ()

option(STATIC "static binary" OFF)
if (CMAKE_COMPILER_IS_GNUCXX AND STATIC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static -static-libstdc++ -static-libgcc")
endif ()

check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if (ipo_supported)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif ()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(glaze CONFIG REQUIRED)
find_package(jwt-cpp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(gsheetpp)
add_library(gsheetpp::gsheetpp ALIAS gsheetpp)
target_sources(gsheetpp
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/google_sheets_client.cpp
    ${CMAKE_SOURCE_DIR}/src/http_client.cpp
)
target_compile_features(gsheetpp PUBLIC ${STD_CPP})
target_include_directories(gsheetpp
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(gsheetpp
  PUBLIC
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    glaze::glaze
    jwt-cpp::jwt-cpp
    nlohmann_json::nlohmann_json
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS gsheetpp
  EXPORT gsheetppTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT gsheetppTargets
  FILE gsheetppTargets.cmake
  NAMESPACE gsheetpp::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gsheetpp
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gsheetppConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/gsheetppConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gsheetpp
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/gsheetppConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/gsheetppConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/gsheetppConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gsheetpp
)

add_executable(gsheetpp_example ${CMAKE_SOURCE_DIR}/examples/main.cpp)
target_link_libraries(gsheetpp_example PRIVATE gsheetpp)
target_compile_features(gsheetpp_example PRIVATE ${STD_CPP})

enable_testing()
add_subdirectory(test)
