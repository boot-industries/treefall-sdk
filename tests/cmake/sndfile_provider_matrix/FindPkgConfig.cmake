set(PKG_CONFIG_FOUND TRUE)
set(PkgConfig_FOUND TRUE)

function(pkg_check_modules prefix)
  set(${prefix}_FOUND TRUE PARENT_SCOPE)
  set(${prefix}_VERSION "fake" PARENT_SCOPE)
  if(NOT TARGET PkgConfig::${prefix})
    add_library(PkgConfig::${prefix} STATIC IMPORTED GLOBAL)
    set_target_properties(PkgConfig::${prefix} PROPERTIES
      IMPORTED_LOCATION "${FAKE_SNDFILE_PREFIX}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}fake_sndfile${CMAKE_STATIC_LIBRARY_SUFFIX}"
      INTERFACE_INCLUDE_DIRECTORIES "${FAKE_SNDFILE_PREFIX}/include")
  endif()
endfunction()
