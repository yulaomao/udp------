set(_atracsys_root_dir ${_atracsys_install_prefix})
set(_atracsys_module_paths ${_atracsys_install_prefix})

set(_atracsys_at @)
set(_atracsys_module_location_template
    ${_atracsys_install_prefix}/Atracsys${_atracsys_at}module${_atracsys_at}/Atracsys${_atracsys_at}module${_atracsys_at}Config.cmake
)
unset(_atracsys_at)
