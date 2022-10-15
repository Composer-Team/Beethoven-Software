project(composer)
cmake_minimum_required(VERSION 3.10)
set(version 1.0.0)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 17)
include(CMakePackageConfigHelpers)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)
include(GNUInstallDirs)

if(NOT DEFINED ${CMAKE_BUILD_TYPE})
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
    STRINGS "Release" "Debug" "MinSizeRel" "RelWithDebInfo")
endif()

add_library(composer SHARED src/fpga_handle.cc src/rocc.cc src/composer_util.cc src/composer_alloc.cc)
# If we can find the Amazon AWS FPGA, enable it a
if(NOT "$ENV{COMPOSER_AWS_SDK_DIR}" STREQUAL "")
  set(AWS_INCLUDE $ENV{COMPOSER_AWS_SDK_DIR}/sdk/userspace/include)
  target_include_directories(composer PUBLIC
    $<BUILD_INTERFACE:include> # for headers when building
    $<BUILD_INTERFACE:${AWS_INCLUDE}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/composer>
    $<INSTALL_INTERFACE:${AWS_INCLUDE}> # for client in install mode
    )
  target_include_directories(composer PUBLIC include ${AWS_INCLUDE})
  set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -DUSE_AWS )
  message("Amazon AWS FPGA SDK detected! This installation will support the F1")
else()
  message("install interface is ${CMAKE_INSTALL_INCLUDEDIR}")
  target_include_directories(composer
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )
  message(WARNING "The $COMPOSER_AWS_SDK_DIR environment variable was not found. This installation will NOT support the F1")
endif()

set(CONFIG_EXPORT_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/composer")
set(EXPORT_TARGETS composer)
configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/Config.cmake.in"
  "${PROJECT_BINARY_DIR}/cmake/composerConfig.cmake"
  INSTALL_DESTINATION "${CONFIG_EXPORT_DIR}"
  PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR CONFIG_EXPORT_DIR
  )

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/cmake/composerConfigVersion.cmake"
  VERSION "${version}"
  COMPATIBILITY AnyNewerVersion
  )

install(TARGETS composer
  EXPORT composerTargets
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  FILES_MATCHING PATTERN "*.h")

install(EXPORT composerTargets
  DESTINATION "${CONFIG_EXPORT_DIR}"
  NAMESPACE APEX::)


install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/cmake/composerConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake/composerConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/composer
  )
