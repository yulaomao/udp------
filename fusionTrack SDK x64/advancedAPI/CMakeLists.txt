#
# Atracsys fusionTrack Samples          # Create a Windows or Unix C++ makefile
# using  # CMake (http://www.cmake.org/)          #
#

cmake_minimum_required(VERSION 3.11)

set(CMAKE_CXX_STANDARD 11)

project(advancedAPI)

if (NOT Atracsys_DIR)
  set(Atracsys_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/Atracsys")
endif ()

find_package(Atracsys REQUIRED COMPONENTS SDK)

set(advancedAPI_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/AdditionalDataStructures.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EventData.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EvtActiveMarkersBatteryStateV1Data.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EvtActiveMarkersButtonStatusesV1Data.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EvtActiveMarkersMaskV1Data.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EvtEioTaggingV1Data.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EvtEioTriggerInfoV1Data.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EvtFansV1Data.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EvtSynchronisationPTPV1Data.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EvtSyntheticTemperatureV1Data.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EvtTemperatureV4Data.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/FrameData.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/FiducialData.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/HeaderData.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/MarkerData.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PictureData.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RawData.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TrackingSystemAbstract.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EventData.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EvtActiveMarkersBatteryStateV1Data.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EvtActiveMarkersButtonStatusesV1Data.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EvtActiveMarkersMaskV1Data.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EvtEioTaggingV1Data.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EvtEioTriggerInfoV1Data.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EvtFansV1Data.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EvtSynchronisationPTPV1Data.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EvtSyntheticTemperatureV1Data.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EvtTemperatureV4Data.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/FrameData.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/FiducialData.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/HeaderData.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/MarkerData.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PictureData.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RawData.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TrackingSystem.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TrackingSystemAbstract.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/version_advancedApi.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/version_advancedApi.hpp")

if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
  set(ARCH 64)
else ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
  set(ARCH 32)
endif ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")

add_library(advancedAPI${ARCH} STATIC ${advancedAPI_SRC})
target_link_libraries(advancedAPI${ARCH} PUBLIC Atracsys::SDK)
target_include_directories(advancedAPI${ARCH}
                           PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/")

if (WIN32)

else ()
  get_target_property(TEMP advancedAPI${ARCH} COMPILE_FLAGS)
  if (TEMP STREQUAL "TEMP-NOTFOUND")
    set(TEMP "") # Set to empty string
  else ()
    set(TEMP "${TEMP} ") # A space to cleanly separate from existing content
  endif ()
  # Append our values
  set(TEMP "${TEMP}-fPIC")
  set_target_properties(advancedAPI${ARCH} PROPERTIES COMPILE_FLAGS ${TEMP})
endif ()

include(GNUInstallDirs)

# install(TARGETS advancedAPI${ARCH} ARCHIVE DESTINATION
# "${CMAKE_BINARY_DIR}/${BUILD_TYPE_FOLDER}/install" PUBLIC_HEADER DESTINATION
# "${CMAKE_BINARY_DIR}/include" )

install(
    TARGETS advancedAPI${ARCH}
    DESTINATION "${CMAKE_BINARY_DIR}/advancedAPI/lib"
    EXPORT advancedAPI-target)
install(EXPORT advancedAPI-target
        DESTINATION "${CMAKE_BINARY_DIR}/advancedAPI")
