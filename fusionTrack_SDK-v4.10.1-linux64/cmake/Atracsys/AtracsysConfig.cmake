if (CMAKE_VERSION VERSION_LESS 3.14)
  message(FATAL_ERROR "Atracsys SDK requires at least CMake version 3.14")
endif ()

if (NOT Atracsys_FIND_COMPONENTS)
  set(Atracsys_NOT_FOUND_MESSAGE "The Atracsys package requires at least one component")
  set(Atracsys_FOUND False)
  return()
endif ()

set(_Atracsys_FIND_PARTS_REQUIRED)
if (Atracsys_FIND_REQUIRED)
  set(_Atracsys_FIND_PARTS_REQUIRED REQUIRED)
endif ()
set(_Atracsys_FIND_PARTS_QUIET)
if (Atracsys_FIND_QUIETLY)
  set(_Atracsys_FIND_PARTS_QUIET QUIET)
endif ()

get_filename_component(_atracsys_install_prefix "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(_Atracsys_NOTFOUND_MESSAGE)

include(${CMAKE_CURRENT_LIST_DIR}/AtracsysModuleLocation.cmake)

foreach (module ${Atracsys_FIND_COMPONENTS})
  find_package(Atracsys${module} ${_Atracsys_FIND_PARTS_QUIET} ${_Atracsys_FIND_PARTS_REQUIRED} PATHS
               ${_atracsys_module_paths} NO_DEFAULT_PATH)
  if (NOT Atracsys${module}_FOUND)
    string(CONFIGURE ${_atracsys_module_location_template} _expected_module_location @ONLY)

    if (Atracsys_FIND_REQUIRED_${module})
      set(_Atracsys_NOTFOUND_MESSAGE
          "${_Atracsys_NOTFOUND_MESSAGE}Failed to find Atracsys component \"${module}\" config file at \"${_expected_module_location}\"\n"
      )
    elseif (NOT Atracsys_FIND_QUIETLY)
      message(
        WARNING
          "Failed to find Atracsys component \"${module}\" config file at \"${_expected_module_location}\"")
    endif ()

    unset(_expected_module_location)
  endif ()
endforeach ()

if (_Atracsys_NOTFOUND_MESSAGE)
  set(Atracsys_NOT_FOUND_MESSAGE "${_Atracsys_NOTFOUND_MESSAGE}")
  set(Atracsys_FOUND False)
endif ()

_atracsys_sdk_check_file_exists("${CMAKE_CURRENT_LIST_DIR}/AtracsysConfigVersion.cmake")
