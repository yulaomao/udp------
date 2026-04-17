cmake_minimum_required(VERSION 3.10)

project(sdkMatlabInterface)

set(CMAKE_CXX_STANDARD 11)

if (WIN32)
  # On Windows, we look for the installation folder in the registry
  get_filename_component(ATRACSYS_SDK_INSTALL_PATH
                         "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Atracsys\\fusionTrack;Root]" ABSOLUTE CACHE)
else ()
  # On Linux, we assume that the installation folder is on directory below.
  get_filename_component(TMP "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
  set(ATRACSYS_SDK_INSTALL_PATH
      "${TMP}" CACHE PATH "Root installation path of the SDK" FORCE)
endif ()

set(Atracsys_DIR "${ATRACSYS_SDK_INSTALL_PATH}/cmake/Atracsys")

find_package(Atracsys REQUIRED COMPONENTS SDK)

# Atracsys SDK API This will be only used to display the headers in Visual Studio
set(api_SRCS
    "${ATRACSYS_SDK_INSTALL_PATH}/include/ftkErrors.h" "${ATRACSYS_SDK_INSTALL_PATH}/include/ftkTypes.h"
    "${ATRACSYS_SDK_INSTALL_PATH}/include/ftkErrors.h" "${ATRACSYS_SDK_INSTALL_PATH}/include/ftkInterface.h"
    "${ATRACSYS_SDK_INSTALL_PATH}/include/ftkPlatform.h" "${ATRACSYS_SDK_INSTALL_PATH}/include/ftkOptions.h")

if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
  set(ARCH 64)
else ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
  set(ARCH 32)
endif ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")

find_package(Matlab REQUIRED MX_LIBRARY)

if (${Matlab_FOUND})

  set(matlab_interface_SRCS
      CPP_frame.cpp
      CPP_frame.hpp
      CPP_fusionTrack.cpp
      CPP_fusionTrack.hpp
      CPP_geometry.cpp
      CPP_geometry.hpp
      CPP_helpers.hpp
      CPP_option.cpp
      CPP_option.hpp)

  matlab_add_mex(
    NAME sdkMatlabInterface
    SRC ${matlab_interface_SRCS} ${api_SRCS}
    LINK_TO Atracsys::SDK)

  target_include_directories(sdkMatlabInterface PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

  file(GLOB M_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.m")
  foreach (M_FILE IN LISTS M_FILES)
    add_custom_command(
      TARGET sdkMatlabInterface
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${M_FILE}" ${CMAKE_CURRENT_BINARY_DIR})
  endforeach ()
  if (WIN32)
    add_custom_command(
      TARGET sdkMatlabInterface
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ATRACSYS_SDK_INSTALL_PATH}/bin/fusionTrack${ARCH}.dll"
              ${CMAKE_CURRENT_BINARY_DIR})
    add_custom_command(
      TARGET sdkMatlabInterface
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ATRACSYS_SDK_INSTALL_PATH}/bin/device${ARCH}.dll"
              ${CMAKE_CURRENT_BINARY_DIR})
    if ("fusionTrack" STREQUAL "spryTrack")
      add_custom_command(
        TARGET sdkMatlabInterface
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ATRACSYS_SDK_INSTALL_PATH}/bin/libusb-1.0.dll"
                ${CMAKE_CURRENT_BINARY_DIR})
    endif ()
  endif ()
endif ()
