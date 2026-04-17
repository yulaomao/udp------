if (CMAKE_VERSION VERSION_LESS 3.11)
  message(FATAL_ERROR "Atracsys Atnet requires at least CMake version 3.11")
endif ()

get_filename_component(_atracsysAtnet_install_prefix "${CMAKE_CURRENT_LIST_DIR}/../../" ABSOLUTE)

if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
  set(ARCH 64)
else ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
  set(ARCH 32)
endif ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")

set(AtracsysAtnet_LIBRARIES Atracsys::Atnet)

macro (_atracsys_Atnet_check_file_exists file)
  if (NOT EXISTS "${file}")
    message(
      FATAL_ERROR
        "The imported target \"Atracsys::Atnet\" references the file
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

macro (_populate_Atnet_target_properties Configuration LIB_LOCATION IMPLIB_LOCATION)
  set_property(
    TARGET Atracsys::Atnet
    APPEND
    PROPERTY IMPORTED_CONFIGURATIONS ${Configuration})

  if (WIN32)
    set(imported_location "${_atracsysAtnet_install_prefix}/bin/${LIB_LOCATION}")
  else ()
    set(imported_location "${_atracsysAtnet_install_prefix}/lib/${LIB_LOCATION}")
  endif ()
  _atracsys_sdk_check_file_exists(${imported_location})
  set_target_properties(
    Atracsys::Atnet PROPERTIES "INTERFACE_LINK_LIBRARIES" "${_AtracsysAtnet_LIB_DEPENDENCIES}"
                               "IMPORTED_LOCATION_${Configuration}" ${imported_location})

  if (NOT "${IMPLIB_LOCATION}" STREQUAL "")
    set(imported_implib "${_atracsysAtnet_install_prefix}/lib/${IMPLIB_LOCATION}")
    _atracsys_sdk_check_file_exists(${imported_implib})
    set_target_properties(Atracsys::Atnet PROPERTIES "IMPORTED_IMPLIB_${Configuration}" ${imported_implib})
  endif ()

endmacro ()

if (NOT TARGET Atracsys::Atnet)
  set(_AtracsysAtnet_OWN_INCLUDE_DIRS "${_atracsysAtnet_install_prefix}/atnet/")

  foreach (_dir ${_AtracsysAtnet_OWN_INCLUDE_DIRS})
    _atracsys_sdk_check_file_exists(${_dir})
  endforeach ()

  set(AtracsysAtnet_INCLUDE_DIRS ${_AtracsysAtnet_OWN_INCLUDE_DIRS})

  set(_AtracsysAtnet_MODULE_DEPENDENCIES "")

  set(_AtracsysAtnet_FIND_DEPENDENCIES_REQUIRED)
  if (AtracsysAtnet_FIND_REQUIRED)
    set(_AtracsysAtnet_FIND_DEPENDENCIES_REQUIRED REQUIRED)
  endif ()
  set(_AtracsysAtnet_FIND_DEPENDENCIES_QUIET)
  if (AtracsysAtnet_FIND_QUIETLY)
    set(_AtracsysAtnet_DEPENDENCIES_FIND_QUIET QUIET)
  endif ()
  set(_AtracsysAtnet_FIND_VERSION_EXACT)
  if (AtracsysAtnet_FIND_VERSION_EXACT)
    set(_AtracsysAtnet_FIND_VERSION_EXACT EXACT)
  endif ()

  set(AtracsysAtnet_EXECUTABLE_COMPILE_FLAGS "")
  set(AtracsysAtnet_EXECUTABLE_LINKER_FLAGS "")
  if (WIN32)
    list(APPEND AtracsysAtnet_EXECUTABLE_LINKER_FLAGS
         "${CMAKE_EXE_LINKER_FLAGS} /DELAYLOAD:atnetLib${ARCH}.dll")
  endif ()

  foreach (_module_dep ${_AtracsysAtnet_MODULE_DEPENDENCIES})
    if (NOT Atracsys${_module_dep}_FOUND)
      find_package(
        Atracsys${_module_dep}
        "${PACKAGE_VERSION}"
        ${_AtracsysAtnet_FIND_VERSION_EXACT}
        ${_AtracsysAtnet_DEPENDENCIES_FIND_QUIET}
        ${_AtracsysAtnet_FIND_DEPENDENCIES_REQUIRED}
        PATHS
        "${CMAKE_CURRENT_LIST_DIR}/.."
        NO_DEFAULT_PATH)
    endif ()

    if (NOT Atracsys${_module_dep}_FOUND)
      set(AtracsysAtnet_FOUND False)
      return()
    endif ()

    list(APPEND AtracsysAtnet_INCLUDE_DIRS "${Atracsys${_module_dep}_INCLUDE_DIRS}")
    list(APPEND AtracsysAtnet_PRIVATE_INCLUDE_DIRS "${Atracsys${_module_dep}_PRIVATE_INCLUDE_DIRS}")
    list(APPEND AtracsysAtnet_DEFINITIONS ${Atracsys${_module_dep}_DEFINITIONS})
    list(APPEND AtracsysAtnet_COMPILE_DEFINITIONS ${Atracsys${_module_dep}_COMPILE_DEFINITIONS})
    list(APPEND AtracsysAtnet_EXECUTABLE_COMPILE_FLAGS ${Atracsys${_module_dep}_EXECUTABLE_COMPILE_FLAGS})
    list(APPEND AtracsysAtnet_EXECUTABLE_LINKER_FLAGS $Atracsys${_module_dep}_EXECUTABLE_LINKER_FLAGS)
  endforeach ()
  list(REMOVE_DUPLICATES AtracsysAtnet_INCLUDE_DIRS)
  list(REMOVE_DUPLICATES AtracsysAtnet_PRIVATE_INCLUDE_DIRS)
  list(REMOVE_DUPLICATES AtracsysAtnet_DEFINITIONS)
  list(REMOVE_DUPLICATES AtracsysAtnet_COMPILE_DEFINITIONS)
  list(REMOVE_DUPLICATES AtracsysAtnet_EXECUTABLE_COMPILE_FLAGS)
  list(REMOVE_DUPLICATES AtracsysAtnet_EXECUTABLE_LINKER_FLAGS)

  if (WIN32)
    list(APPEND _AtracsysAtnet_LIB_DEPENDENCIES delayimp)
    list(APPEND _AtracsysAtnet_LIB_DEPENDENCIES Winmm)
  endif ()

  find_package(Threads)
  list(APPEND _AtracsysAtnet_LIB_DEPENDENCIES ${CMAKE_THREAD_LIBS_INIT})

  add_library(Atracsys::Atnet SHARED IMPORTED)

  set_property(TARGET Atracsys::Atnet PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                                               ${_AtracsysAtnet_OWN_INCLUDE_DIRS})
  if (WIN32)
    set_property(
      TARGET Atracsys::Atnet
      APPEND
      PROPERTY INTERFACE_LINK_OPTIONS "/DELAYLOAD:atnetLib${ARCH}.dll")
    set_property(
      TARGET Atracsys::Atnet PROPERTY INTERFACE_COMPILE_DEFINITIONS ATR_FTK NOMINMAX
                                      FORCED_DEVICE_DLL_PATH="${_atracsysAtnet_install_prefix}/bin")
  else ()
    set_property(TARGET Atracsys::Atnet PROPERTY INTERFACE_COMPILE_DEFINITIONS ATR_FTK)
  endif ()

  if (WIN32)
    _populate_Atnet_target_properties(
      RELEASE "${CMAKE_IMPORT_LIBRARY_PREFIX}atnetLib${ARCH}${CMAKE_SHARED_LIBRARY_SUFFIX}"
      "${CMAKE_SHARED_LIBRARY_PREFIX}atnetLib${ARCH}${CMAKE_IMPORT_LIBRARY_SUFFIX}")
    _populate_Atnet_target_properties(
      DEFAULT "${CMAKE_IMPORT_LIBRARY_PREFIX}atnetLib${ARCH}${CMAKE_SHARED_LIBRARY_SUFFIX}"
      "${CMAKE_SHARED_LIBRARY_PREFIX}atnetLib${ARCH}${CMAKE_IMPORT_LIBRARY_SUFFIX}")
    _populate_Atnet_target_properties(
      DEBUG "${CMAKE_IMPORT_LIBRARY_PREFIX}atnetLib${ARCH}${CMAKE_SHARED_LIBRARY_SUFFIX}"
      "${CMAKE_SHARED_LIBRARY_PREFIX}atnetLib${ARCH}${CMAKE_IMPORT_LIBRARY_SUFFIX}")
  else ()
    _populate_Atnet_target_properties(
      RELEASE "${CMAKE_SHARED_LIBRARY_PREFIX}atnetLib${ARCH}${CMAKE_SHARED_LIBRARY_SUFFIX}" "")
  endif ()

  _atracsys_sdk_check_file_exists("${CMAKE_CURRENT_LIST_DIR}/AtracsysAtnetConfigVersion.cmake")
endif ()
