if (CMAKE_VERSION VERSION_LESS 3.11)
  message(FATAL_ERROR "Atracsys SDK requires at least CMake version 3.11")
endif ()

get_filename_component(_atracsysAdvancedAPI_install_prefix "${CMAKE_CURRENT_LIST_DIR}/../../" ABSOLUTE)

include(GNUInstallDirs)

if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
  set(ARCH 64)
else ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
  set(ARCH 32)
endif ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif ()

set(Atracsys_DIR "${_atracsysAdvancedAPI_install_prefix}/cmake/Atracsys")
message(STATUS "Atracsys_DIR is ${Atracsys_DIR}")

find_package(Atracsys REQUIRED COMPONENTS SDK)

include(ProcessorCount)
ProcessorCount(N)
if (N EQUAL 0)
  set(N 1)
  message(STATUS "Hardcoding ${N} cores")
endif ()

set(AtracsysAdvancedAPI_LIBRARIES Atracsys::AdvancedAPI)

file(GLOB advanced_SRCS "${_atracsysAdvancedAPI_install_prefix}/advancedAPI/*")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/AdvancedApi-src")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/AdvancedApi-build/${CMAKE_BUILD_TYPE}")
foreach (FILE IN LISTS advanced_SRCS)
  configure_file(${FILE} "${CMAKE_BINARY_DIR}/AdvancedApi-src" COPYONLY)
endforeach ()
message(STATUS "Creating C++ API makefiles")
if (DEFINED CMAKE_GENERATOR_PLATFORM)
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" "-DAtracsys_DIR:PATH=${Atracsys_DIR}"
      "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}" "-DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
      "${CMAKE_BINARY_DIR}/AdvancedApi-src"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/AdvancedApi-build/${CMAKE_BUILD_TYPE}"
    RESULT_VARIABLE CPPWRAPPER_MAKEFILES)
else (DEFINED CMAKE_GENERATOR_PLATFORM)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" "-DAtracsys_DIR:PATH=${Atracsys_DIR}"
            "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}" "${CMAKE_BINARY_DIR}/AdvancedApi-src"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/AdvancedApi-build/${CMAKE_BUILD_TYPE}"
    RESULT_VARIABLE CPPWRAPPER_MAKEFILES)
endif (DEFINED CMAKE_GENERATOR_PLATFORM)
if (NOT ${CPPWRAPPER_MAKEFILES} EQUAL 0)
  message(FATAL_ERROR "Could not create C++ API makefiles (error is ${CPPWRAPPER_MAKEFILES})")
endif ()
message(STATUS "Makefiles creation OK, building C++ API")
if (NOT WIN32 OR (WIN32 AND CMAKE_GENERATOR MATCHES "Ninja.*"))
  # Linux build or Ninja on windows, we can directly build and install
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build . -- install -j${N}
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/AdvancedApi-build/${CMAKE_BUILD_TYPE}"
    RESULT_VARIABLE CPPWRAPPER_BUILD)
  if (NOT ${CPPWRAPPER_BUILD} EQUAL 0)
    message(FATAL_ERROR "Could not compile and/or install C++ API (error is ${CPPWRAPPER_BUILD})")
  endif ()
else ()
  # Windows with Visual Studio generator We need to start by just building...
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build . --config ${CMAKE_BUILD_TYPE}
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/AdvancedApi-build/${CMAKE_BUILD_TYPE}"
    RESULT_VARIABLE CPPWRAPPER_BUILD)
  if (NOT ${CPPWRAPPER_BUILD} EQUAL 0)
    message(FATAL_ERROR "Could not compile C++ API (error is ${CPPWRAPPER_BUILD})")
  endif ()

  # ... then install
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build . --config ${CMAKE_BUILD_TYPE} --target install
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/AdvancedApi-build/${CMAKE_BUILD_TYPE}"
    RESULT_VARIABLE CPPWRAPPER_INSTALL)
  if (NOT ${CPPWRAPPER_INSTALL} EQUAL 0)
    message(FATAL_ERROR "Could not install C++ API (error is ${CPPWRAPPER_BUILD})")
  endif ()
endif ()

macro (_atracsys_AdvancedAPI_check_file_exists file)
  if (NOT EXISTS "${file}")
    message(
      FATAL_ERROR
        "The imported target \"Atracsys::AdvancedAPI\" references the file
   \"${file}\"
but this file does not exist.  Possible reasons include:
* The file was deleted, renamed, or moved to another location.
* An install or uninstall procedure did not complete successfully.
* The installation package was faulty and contained
   \"${CMAKE_CURRENT_LIST_FILE}\"
but not all the files it references.
")
  endif ()
endmacro ()

macro (_populate_AdvancedAPI_target_properties Configuration LIB_LOCATION IMPLIB_LOCATION)
  set_property(
    TARGET Atracsys::AdvancedAPI
    APPEND
    PROPERTY IMPORTED_CONFIGURATIONS ${Configuration})

  set(imported_location
      "${CMAKE_BINARY_DIR}/AdvancedApi-build/${CMAKE_BUILD_TYPE}/advancedAPI/lib/${LIB_LOCATION}")
  _atracsys_sdk_check_file_exists(${imported_location})
  set_target_properties(
    Atracsys::AdvancedAPI PROPERTIES "INTERFACE_LINK_LIBRARIES" "${_AtracsysAdvancedAPI_LIB_DEPENDENCIES}"
                                     "IMPORTED_LOCATION_${Configuration}" ${imported_location})

endmacro ()

if (NOT TARGET Atracsys::AdvancedAPI)

  set(_AtracsysAdvancedAPI_OWN_INCLUDE_DIRS "${_atracsysAdvancedAPI_install_prefix}/advancedAPI/")

  foreach (_dir ${_AtracsysAdvancedAPI_OWN_INCLUDE_DIRS})
    _atracsys_sdk_check_file_exists(${_dir})
  endforeach ()

  set(AtracsysAdvancedAPI_INCLUDE_DIRS ${_AtracsysAdvancedAPI_OWN_INCLUDE_DIRS})

  set(_AtracsysAdvancedAPI_MODULE_DEPENDENCIES "SDK")

  set(_AtracsysAdvancedAPI_FIND_DEPENDENCIES_REQUIRED)
  if (AtracsysAdvancedAPI_FIND_REQUIRED)
    set(_AtracsysAdvancedAPI_FIND_DEPENDENCIES_REQUIRED REQUIRED)
  endif ()
  set(_AtracsysAdvancedAPI_FIND_DEPENDENCIES_QUIET)
  if (AtracsysAdvancedAPI_FIND_QUIETLY)
    set(_AtracsysAdvancedAPI_DEPENDENCIES_FIND_QUIET QUIET)
  endif ()
  set(_AtracsysAdvancedAPI_FIND_VERSION_EXACT)
  if (AtracsysAdvancedAPI_FIND_VERSION_EXACT)
    set(_AtracsysAdvancedAPI_FIND_VERSION_EXACT EXACT)
  endif ()

  set(AtracsysAdvancedAPI_EXECUTABLE_COMPILE_FLAGS "")

  foreach (_module_dep ${_AtracsysAdvancedAPI_MODULE_DEPENDENCIES})
    if (NOT Atracsys${_module_dep}_FOUND)
      find_package(
        Atracsys${_module_dep}
        ${_AtracsysAdvancedAPI_FIND_VERSION_EXACT}
        ${_AtracsysAdvancedAPI_DEPENDENCIES_FIND_QUIET}
        ${_AtracsysAdvancedAPI_FIND_DEPENDENCIES_REQUIRED}
        PATHS
        "${CMAKE_CURRENT_LIST_DIR}/.."
        NO_DEFAULT_PATH)
    endif ()

    if (NOT Atracsys${_module_dep}_FOUND)
      set(AtracsysAdvancedAPI_FOUND False)
      message(STATUS "Missing module ${_module_dep}")
      return()
    endif ()

    list(APPEND AtracsysAdvancedAPI_INCLUDE_DIRS "${Atracsys${_module_dep}_INCLUDE_DIRS}")
    list(APPEND AtracsysAdvancedAPI_PRIVATE_INCLUDE_DIRS "${Atracsys${_module_dep}_PRIVATE_INCLUDE_DIRS}")
    list(APPEND AtracsysAdvancedAPI_DEFINITIONS ${Atracsys${_module_dep}_DEFINITIONS})
    list(APPEND AtracsysAdvancedAPI_COMPILE_DEFINITIONS ${Atracsys${_module_dep}_COMPILE_DEFINITIONS})
    list(APPEND AtracsysAdvancedAPI_EXECUTABLE_COMPILE_FLAGS
         ${Atracsys${_module_dep}_EXECUTABLE_COMPILE_FLAGS})
  endforeach ()
  list(REMOVE_DUPLICATES AtracsysAdvancedAPI_INCLUDE_DIRS)
  list(REMOVE_DUPLICATES AtracsysAdvancedAPI_PRIVATE_INCLUDE_DIRS)
  list(REMOVE_DUPLICATES AtracsysAdvancedAPI_DEFINITIONS)
  list(REMOVE_DUPLICATES AtracsysAdvancedAPI_COMPILE_DEFINITIONS)
  list(REMOVE_DUPLICATES AtracsysAdvancedAPI_EXECUTABLE_COMPILE_FLAGS)

  if (NOT ${AdvancedAPI_FOUND})
    message(STATUS "Missing AtracsysAdvancedAPI module")
  else ()
    message(STATUS "Found (really?) AtracsysAdvancedAPI module")
  endif ()
  set(_AtracsysAdvancedAPI_LIB_DEPENDENCIES "Atracsys::SDK")
  message(STATUS "CMAKE_BINARY_DIR is ${CMAKE_BINARY_DIR}")
  include("${CMAKE_BINARY_DIR}/AdvancedApi-build/${CMAKE_BUILD_TYPE}/advancedAPI/advancedAPI-target.cmake")
  add_library(Atracsys::AdvancedAPI STATIC IMPORTED)

  set_property(
    TARGET Atracsys::AdvancedAPI
    PROPERTY
      INTERFACE_INCLUDE_DIRECTORIES
      "${CMAKE_BINARY_DIR}/AdvancedApi-build/${CMAKE_BUILD_TYPE}/advancedAPI/lib"
      "${AtracsysAdvancedAPI_INCLUDE_DIRS}")
  set_property(TARGET Atracsys::AdvancedAPI PROPERTY INTERFACE_COMPILE_DEFINITIONS
                                                     ${AtracsysAdvancedAPI_COMPILE_DEFINITIONS})
  _populate_advancedapi_target_properties(
    RELEASE ${CMAKE_STATIC_LIBRARY_PREFIX}advancedAPI${ARCH}${CMAKE_STATIC_LIBRARY_SUFFIX} "")
  _populate_advancedapi_target_properties(
    RELWITHDEBINFO ${CMAKE_STATIC_LIBRARY_PREFIX}advancedAPI${ARCH}${CMAKE_STATIC_LIBRARY_SUFFIX} "")
  _populate_advancedapi_target_properties(
    DEBUG ${CMAKE_STATIC_LIBRARY_PREFIX}advancedAPI${ARCH}${CMAKE_STATIC_LIBRARY_SUFFIX} "")

  _atracsys_advancedapi_check_file_exists("${CMAKE_CURRENT_LIST_DIR}/AtracsysAdvancedAPIConfigVersion.cmake")
endif ()
