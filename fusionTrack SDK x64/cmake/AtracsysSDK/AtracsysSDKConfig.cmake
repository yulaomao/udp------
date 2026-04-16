if (CMAKE_VERSION VERSION_LESS 3.11)
  message(FATAL_ERROR "Atracsys SDK requires at least CMake version 3.11")
endif ()

get_filename_component(_atracsysSDK_install_prefix
                       "${CMAKE_CURRENT_LIST_DIR}/../../" ABSOLUTE)

if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
  set(ARCH 64)
else ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
  set(ARCH 32)
endif ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")

set(AtracsysSDK_LIBRARIES Atracsys::SDK)

macro (_atracsys_SDK_check_file_exists file)
  if (NOT EXISTS "${file}")
    message(
      FATAL_ERROR "The imported target \"Atracsys::SDK\" references the file
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

macro (_populate_SDK_target_properties
       Configuration
       LIB_LOCATION
       IMPLIB_LOCATION)
  set_property(TARGET Atracsys::SDK
               APPEND
               PROPERTY IMPORTED_CONFIGURATIONS ${Configuration})

  if (WIN32)
    set(imported_location "${_atracsysSDK_install_prefix}/bin/${LIB_LOCATION}")
  else ()
    set(imported_location "${_atracsysSDK_install_prefix}/lib/${LIB_LOCATION}")
  endif ()
  _atracsys_sdk_check_file_exists(${imported_location})
  set_target_properties(Atracsys::SDK
                        PROPERTIES "INTERFACE_LINK_LIBRARIES"
                                   "${_AtracsysSDK_LIB_DEPENDENCIES}"
                                   "IMPORTED_LOCATION_${Configuration}"
                                   ${imported_location})

  if(NOT "${IMPLIB_LOCATION}" STREQUAL "")
    set(imported_implib "${_atracsysSDK_install_prefix}/lib/${IMPLIB_LOCATION}")
      _atracsys_sdk_check_file_exists(${imported_implib})
    set_target_properties(Atracsys::SDK PROPERTIES
					"IMPORTED_IMPLIB_${Configuration}" ${imported_implib})
  endif()

endmacro ()

if (NOT TARGET Atracsys::SDK)
  set(_AtracsysSDK_OWN_INCLUDE_DIRS "${_atracsysSDK_install_prefix}/include/")

  foreach (_dir ${_AtracsysSDK_OWN_INCLUDE_DIRS})
    _atracsys_sdk_check_file_exists(${_dir})
  endforeach ()

  set(AtracsysSDK_INCLUDE_DIRS ${_AtracsysSDK_OWN_INCLUDE_DIRS})

  set(_AtracsysSDK_MODULE_DEPENDENCIES "")

  set(_AtracsysSDK_FIND_DEPENDENCIES_REQUIRED)
  if (AtracsysSDK_FIND_REQUIRED)
    set(_AtracsysSDK_FIND_DEPENDENCIES_REQUIRED REQUIRED)
  endif ()
  set(_AtracsysSDK_FIND_DEPENDENCIES_QUIET)
  if (AtracsysSDK_FIND_QUIETLY)
    set(_AtracsysSDK_DEPENDENCIES_FIND_QUIET QUIET)
  endif ()
  set(_AtracsysSDK_FIND_VERSION_EXACT)
  if (AtracsysSDK_FIND_VERSION_EXACT)
    set(_AtracsysSDK_FIND_VERSION_EXACT EXACT)
  endif ()

  set(AtracsysSDK_EXECUTABLE_COMPILE_FLAGS "")
  set(AtracsysSDK_EXECUTABLE_LINKER_FLAGS "")
  if (WIN32)
    list(APPEND AtracsysSDK_EXECUTABLE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DELAYLOAD:fusionTrack${ARCH}.dll")
  endif ()

  foreach (_module_dep ${_AtracsysSDK_MODULE_DEPENDENCIES})
    if (NOT Atracsys${_module_dep}_FOUND)
      find_package(Atracsys${_module_dep}
                   "${PACKAGE_VERSION}"
                   ${_AtracsysSDK_FIND_VERSION_EXACT}
                   ${_AtracsysSDK_DEPENDENCIES_FIND_QUIET}
                   ${_AtracsysSDK_FIND_DEPENDENCIES_REQUIRED}
                   PATHS
                   "${CMAKE_CURRENT_LIST_DIR}/.."
                   NO_DEFAULT_PATH)
    endif ()

    if (NOT Atracsys${_module_dep}_FOUND)
      set(AtracsysSDK_FOUND False)
      return()
    endif ()

    list(APPEND AtracsysSDK_INCLUDE_DIRS
                "${Atracsys${_module_dep}_INCLUDE_DIRS}")
    list(APPEND AtracsysSDK_PRIVATE_INCLUDE_DIRS
                "${Atracsys${_module_dep}_PRIVATE_INCLUDE_DIRS}")
    list(APPEND AtracsysSDK_DEFINITIONS ${Atracsys${_module_dep}_DEFINITIONS})
    list(APPEND AtracsysSDK_COMPILE_DEFINITIONS
                ${Atracsys${_module_dep}_COMPILE_DEFINITIONS})
    list(APPEND AtracsysSDK_EXECUTABLE_COMPILE_FLAGS
                ${Atracsys${_module_dep}_EXECUTABLE_COMPILE_FLAGS})
    list(APPEND AtracsysSDK_EXECUTABLE_LINKER_FLAGS
                $Atracsys${_module_dep}_EXECUTABLE_LINKER_FLAGS)
  endforeach ()
  list(REMOVE_DUPLICATES AtracsysSDK_INCLUDE_DIRS)
  list(REMOVE_DUPLICATES AtracsysSDK_PRIVATE_INCLUDE_DIRS)
  list(REMOVE_DUPLICATES AtracsysSDK_DEFINITIONS)
  list(REMOVE_DUPLICATES AtracsysSDK_COMPILE_DEFINITIONS)
  list(REMOVE_DUPLICATES AtracsysSDK_EXECUTABLE_COMPILE_FLAGS)
  list(REMOVE_DUPLICATES AtracsysSDK_EXECUTABLE_LINKER_FLAGS)

  if (WIN32)
    list(APPEND _AtracsysSDK_LIB_DEPENDENCIES delayimp)
    list(APPEND _AtracsysSDK_LIB_DEPENDENCIES Winmm)
  endif()

  find_package(Threads)
  list(APPEND _AtracsysSDK_LIB_DEPENDENCIES ${CMAKE_THREAD_LIBS_INIT})

  add_library(Atracsys::SDK SHARED IMPORTED)

  set_property(TARGET Atracsys::SDK
               PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                        ${_AtracsysSDK_OWN_INCLUDE_DIRS})
  if (WIN32)
    set_property(TARGET Atracsys::SDK
	             APPEND PROPERTY INTERFACE_LINK_OPTIONS "/DELAYLOAD:fusionTrack${ARCH}.dll")
    set_property(TARGET Atracsys::SDK
                 PROPERTY INTERFACE_COMPILE_DEFINITIONS ATR_FTK NOMINMAX FORCED_DEVICE_DLL_PATH="${_atracsysSDK_install_prefix}/bin")
  else ()
    set_property(TARGET Atracsys::SDK
                 PROPERTY INTERFACE_COMPILE_DEFINITIONS ATR_FTK)
  endif ()

  if (WIN32)
    _populate_sdk_target_properties(RELEASE "${CMAKE_IMPORT_LIBRARY_PREFIX}fusionTrack${ARCH}${CMAKE_SHARED_LIBRARY_SUFFIX}" "${CMAKE_SHARED_LIBRARY_PREFIX}fusionTrack${ARCH}${CMAKE_IMPORT_LIBRARY_SUFFIX}")
    _populate_sdk_target_properties(DEFAULT "${CMAKE_IMPORT_LIBRARY_PREFIX}fusionTrack${ARCH}${CMAKE_SHARED_LIBRARY_SUFFIX}" "${CMAKE_SHARED_LIBRARY_PREFIX}fusionTrack${ARCH}${CMAKE_IMPORT_LIBRARY_SUFFIX}")
    _populate_sdk_target_properties(DEBUG "${CMAKE_IMPORT_LIBRARY_PREFIX}fusionTrack${ARCH}${CMAKE_SHARED_LIBRARY_SUFFIX}" "${CMAKE_SHARED_LIBRARY_PREFIX}fusionTrack${ARCH}${CMAKE_IMPORT_LIBRARY_SUFFIX}")
  else ()
    _populate_sdk_target_properties(RELEASE "${CMAKE_SHARED_LIBRARY_PREFIX}fusionTrack${ARCH}${CMAKE_SHARED_LIBRARY_SUFFIX}" "")
  endif ()

  _atracsys_sdk_check_file_exists(
    "${CMAKE_CURRENT_LIST_DIR}/AtracsysSDKConfigVersion.cmake")
endif ()
