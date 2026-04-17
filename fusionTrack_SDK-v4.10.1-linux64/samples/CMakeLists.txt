# ##############################################################################
# Atracsys fusionTrack Samples          # Create a Windows or Unix C++ makefile
# using  # CMake (http://www.cmake.org/)          # # This script compile the
# samples using the    # 32 bits version of the driver                # #
# ##############################################################################

cmake_minimum_required(VERSION 3.4)

set(CMAKE_CXX_STANDARD 11)

project(samples C CXX)

if (WIN32)
  # On Windows, we look for the installation folder in the registry
  get_filename_component(ATRACSYS_SDK_INSTALL_PATH "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Atracsys\\fusionTrack;Root]"
                         ABSOLUTE CACHE)
else ()
  # On Linux, we assume that the installation folder is on directory below.
  set(ATRACSYS_SDK_INSTALL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif ()

set(Atracsys_DIR "${ATRACSYS_SDK_INSTALL_PATH}/cmake/Atracsys")

find_package(Atracsys REQUIRED COMPONENTS SDK AdvancedAPI Atnet)

# Atracsys SDK API This will be only used to display the headers in Visual Studio
set(api_SRCS
    "${ATRACSYS_SDK_INSTALL_PATH}/include/ftkErrors.h" "${ATRACSYS_SDK_INSTALL_PATH}/include/ftkTypes.h"
    "${ATRACSYS_SDK_INSTALL_PATH}/include/ftkErrors.h" "${ATRACSYS_SDK_INSTALL_PATH}/include/ftkInterface.h"
    "${ATRACSYS_SDK_INSTALL_PATH}/include/ftkPlatform.h" "${ATRACSYS_SDK_INSTALL_PATH}/include/ftkOptions.h")

# Helpers dedicated to the samples
set(additional_SRCS
    "${ATRACSYS_SDK_INSTALL_PATH}/samples/helpers.cpp" "${ATRACSYS_SDK_INSTALL_PATH}/samples/helpers.hpp"
    "${ATRACSYS_SDK_INSTALL_PATH}/samples/geometryHelper.hpp"
    "${ATRACSYS_SDK_INSTALL_PATH}/samples/geometryHelper.cpp")

if (WIN32)
  set(additional_SRCS ${additional_SRCS} "${ATRACSYS_SDK_INSTALL_PATH}/samples/helpers_windows.cpp")
else (WIN32)
  set(additional_SRCS ${additional_SRCS} "${ATRACSYS_SDK_INSTALL_PATH}/samples/helpers_linux.cpp")
endif (WIN32)

add_executable(ftk1_ListOptions "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo1_ListOptions.cpp" ${api_SRCS}
                                ${additional_SRCS})
add_executable(ftk2_AcquisitionBasic "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo2_AcquisitionBasic.cpp"
                                     ${api_SRCS} ${additional_SRCS})
add_executable(ftk3_AcquisitionAdvanced "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo3_AcquisitionAdvanced.cpp"
                                        ${api_SRCS} ${additional_SRCS})
add_executable(ftk4_AcquisitionRawData "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo4_AcquisitionRawData.cpp"
                                       ${api_SRCS} ${additional_SRCS})
add_executable(ftk5_AcquisitionRelative "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo5_AcquisitionRelative.cpp"
                                        ${api_SRCS} ${additional_SRCS})
add_executable(ftk6_ControlLED "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo6_ControlLED.cpp" ${api_SRCS}
                               ${additional_SRCS})
add_executable(ftk7_AcquisitionExtended "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo7_AcquisitionExtended.cpp"
                                        ${api_SRCS} ${additional_SRCS})
add_executable(ftk8_GetTemperatures "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo8_GetTemperatures.cpp"
                                    ${api_SRCS} ${additional_SRCS})
add_executable(ftk9_GetAcceleration "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo9_GetAcceleration.cpp"
                                    ${api_SRCS} ${additional_SRCS})
add_executable(ftk10_GetTimestamp "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo10_GetTimestamp.cpp"
                                  ${api_SRCS} ${additional_SRCS})
add_executable(ftk18_DumpDeviceInfo "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo18_DumpDeviceInfo.cpp"
                                    ${api_SRCS} ${additional_SRCS})
add_executable(
  ftk19_OpticalCommunication "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo19_OpticalCommunication.cpp"
                             ${api_SRCS} ${additional_SRCS})
add_executable(ftk20_DiffLatency "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo20_DiffLatency.cpp" ${api_SRCS}
                                 ${additional_SRCS})
if (NOT (MSVC AND MSVC_VERSION LESS 1900))
  add_executable(ftk11_WirelessMarker "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo11_WirelessMarker.cpp"
                                      ${api_SRCS} ${additional_SRCS})
  add_executable(
    ftk21_MultipleAcquisition "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo21_MultipleAcquisition.cpp"
                              ${api_SRCS} ${additional_SRCS})
  add_executable(
    ftk22_EnvironmentHandling "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo22_EnvironmentHandling.cpp"
                              ${api_SRCS} ${additional_SRCS})
  add_executable(
    ftk24_CheckDeviceAuthentication
    "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo24_CheckDeviceAuthentication.cpp" ${api_SRCS}
    ${additional_SRCS})
  add_executable(ftk26_StrobeMode "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo26_StrobeMode.cpp" ${api_SRCS}
                                  ${additional_SRCS})
  add_executable(
    ftk29_ExtractCalibrationParameters
    "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo29_ExtractCalibrationParameters.cpp" ${api_SRCS}
    ${additional_SRCS})
  add_executable(ftk32_InterpolateFrames "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo32_InterpolateFrames.cpp"
                                         ${api_SRCS} ${additional_SRCS})
  add_executable(ftk33_Buzzer "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo33_Buzzer.cpp" ${api_SRCS}
                              ${additional_SRCS})
  add_executable(ftk34_ReprocessFrame "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo34_ReprocessFrame.cpp"
                                      ${api_SRCS} ${additional_SRCS})
endif ()

add_executable(ftk35_PTPBasic "${ATRACSYS_SDK_INSTALL_PATH}/samples/ftk35_PTPBasic.cpp" ${api_SRCS}
                              ${additional_SRCS})
add_executable(ftk36_TaggerModes "${ATRACSYS_SDK_INSTALL_PATH}/samples/ftk36_TaggerModes.cpp" ${api_SRCS}
                                 ${additional_SRCS})
add_executable(
  ftk37_Acquisition64FiducialsMarkers
  "${ATRACSYS_SDK_INSTALL_PATH}/samples/stereo37_Acquisition64FiducialsMarkers.cpp" ${api_SRCS}
  ${additional_SRCS})

add_executable(atnetExecutable "${ATRACSYS_SDK_INSTALL_PATH}/samples/atnet.cpp")

if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
  set(ARCH 64)
else ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
  set(ARCH 32)
endif ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")

target_link_libraries(ftk1_ListOptions Atracsys::SDK)
target_link_libraries(ftk2_AcquisitionBasic Atracsys::SDK)
target_link_libraries(ftk3_AcquisitionAdvanced Atracsys::SDK)
target_link_libraries(ftk4_AcquisitionRawData Atracsys::SDK)
target_link_libraries(ftk5_AcquisitionRelative Atracsys::SDK)
target_link_libraries(ftk6_ControlLED Atracsys::SDK)
target_link_libraries(ftk7_AcquisitionExtended Atracsys::SDK)
target_link_libraries(ftk8_GetTemperatures Atracsys::SDK)
target_link_libraries(ftk9_GetAcceleration Atracsys::SDK)
target_link_libraries(ftk10_GetTimestamp Atracsys::SDK)
target_link_libraries(ftk18_DumpDeviceInfo Atracsys::SDK)
target_link_libraries(ftk19_OpticalCommunication Atracsys::SDK)
target_link_libraries(ftk20_DiffLatency Atracsys::SDK)
target_link_libraries(ftk29_ExtractCalibrationParameters Atracsys::SDK)
target_link_libraries(ftk32_InterpolateFrames Atracsys::SDK)
target_link_libraries(ftk34_ReprocessFrame Atracsys::SDK)
if (NOT (MSVC AND MSVC_VERSION LESS 1900))
  target_link_libraries(ftk11_WirelessMarker Atracsys::AdvancedAPI)
  target_link_libraries(ftk21_MultipleAcquisition Atracsys::AdvancedAPI)
  target_link_libraries(ftk22_EnvironmentHandling Atracsys::AdvancedAPI)
  target_link_libraries(ftk24_CheckDeviceAuthentication Atracsys::AdvancedAPI)
  target_link_libraries(ftk26_StrobeMode Atracsys::AdvancedAPI)
  target_link_libraries(ftk33_Buzzer Atracsys::AdvancedAPI)
endif ()
target_link_libraries(ftk35_PTPBasic Atracsys::SDK)
target_link_libraries(ftk36_TaggerModes Atracsys::SDK)
target_link_libraries(ftk37_Acquisition64FiducialsMarkers Atracsys::SDK)
target_link_libraries(atnetExecutable Atracsys::Atnet)
