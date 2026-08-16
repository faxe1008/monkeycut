include(FindPackageHandleStandardArgs)

set(MONKEYCUT_FFMPEG_LIBS avcodec avformat avutil swscale swresample)

function(_ffmpeg_define_targets)
  foreach(_lib IN LISTS MONKEYCUT_FFMPEG_LIBS)
    if(NOT FFMPEG_${_lib}_LIBRARY)
      set(FFMPEG_${_lib}_FOUND FALSE PARENT_SCOPE)
    else()
      if(NOT TARGET FFmpeg::${_lib})
        add_library(FFmpeg::${_lib} UNKNOWN IMPORTED)
        set_target_properties(FFmpeg::${_lib} PROPERTIES
          IMPORTED_LOCATION "${FFMPEG_${_lib}_LIBRARY}"
          INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIR}")
      endif()
    endif()
  endforeach()
endfunction()

# 1) An explicit FFmpeg_ROOT (BtbN / vcpkg style tree):
#      <root>/include/libavcodec/avcodec.h
#    Our sources include <libavcodec/...>, so the include root is
#    <root>/include. Libraries in <root>/{lib,lib64,bin}.
if(FFmpeg_ROOT)
  set(FFMPEG_INCLUDE_DIR "")
  if(EXISTS "${FFmpeg_ROOT}/include/libavcodec/avcodec.h")
    set(FFMPEG_INCLUDE_DIR "${FFmpeg_ROOT}/include")
  endif()

  # An explicit root is authoritative: search only inside it so a partial
  # tree fails loudly instead of silently mixing in system/mismatched libs.
  if(FFMPEG_INCLUDE_DIR)
    foreach(_lib IN LISTS MONKEYCUT_FFMPEG_LIBS)
      find_library(FFMPEG_${_lib}_LIBRARY
        NAMES ${_lib} lib${_lib}
        HINTS "${FFmpeg_ROOT}" PATH_SUFFIXES lib lib64 bin
        NO_DEFAULT_PATH)
    endforeach()
  endif()

  set(_missing_libs "")
  foreach(_lib IN LISTS MONKEYCUT_FFMPEG_LIBS)
    if(NOT FFMPEG_${_lib}_LIBRARY)
      string(APPEND _missing_libs " ${_lib}")
    endif()
  endforeach()
  message(STATUS "[FindFFmpeg] explicit root=${FFmpeg_ROOT} include=[${FFMPEG_INCLUDE_DIR}] missing=[${_missing_libs}]")

  set(_required FFMPEG_INCLUDE_DIR)
  foreach(_lib IN LISTS MONKEYCUT_FFMPEG_LIBS)
    list(APPEND _required FFMPEG_${_lib}_LIBRARY)
  endforeach()
  string(REPLACE ";" " " _libs_str "${MONKEYCUT_FFMPEG_LIBS}")
  find_package_handle_standard_args(FFmpeg
    REQUIRED_VARS ${_required}
    REASON_FAILURE_MESSAGE
      "FFmpeg_ROOT=${FFmpeg_ROOT} is not a complete FFmpeg build. Missing components:[${_missing_libs}]. Expected include/libavcodec/avcodec.h plus ${_libs_str} libraries under lib/.")

  _ffmpeg_define_targets()
  return()
endif()

# 2) No explicit root: pkg-config (distro dev packages).
set(FFMPEG_FOUND_PC TRUE)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  foreach(_lib IN LISTS MONKEYCUT_FFMPEG_LIBS)
    pkg_check_modules(PC_${_lib} QUIET IMPORTED_TARGET lib${_lib})
    if(NOT PC_${_lib}_FOUND)
      set(FFMPEG_FOUND_PC FALSE)
    endif()
  endforeach()
else()
  set(FFMPEG_FOUND_PC FALSE)
endif()

if(FFMPEG_FOUND_PC)
  set(_ffmpeg_include_dirs "")
  foreach(_lib IN LISTS MONKEYCUT_FFMPEG_LIBS)
    foreach(_dir IN LISTS PC_${_lib}_INCLUDE_DIRS)
      list(FIND _ffmpeg_include_dirs ${_dir} _idx)
      if(_idx EQUAL -1)
        list(APPEND _ffmpeg_include_dirs ${_dir})
      endif()
    endforeach()
  endforeach()
  set(FFMPEG_INCLUDE_DIR "${_ffmpeg_include_dirs}")
  foreach(_lib IN LISTS MONKEYCUT_FFMPEG_LIBS)
    if(NOT TARGET FFmpeg::${_lib})
      add_library(FFmpeg::${_lib} ALIAS PkgConfig::PC_${_lib})
    endif()
  endforeach()
else()
  # 3) System search (no pkg-config, no explicit root).
  find_path(FFMPEG_NESTED_HDR_DIR avcodec.h PATH_SUFFIXES libavcodec)
  if(FFMPEG_NESTED_HDR_DIR)
    get_filename_component(FFMPEG_INCLUDE_DIR "${FFMPEG_NESTED_HDR_DIR}" DIRECTORY)
  endif()
  foreach(_lib IN LISTS MONKEYCUT_FFMPEG_LIBS)
    find_library(FFMPEG_${_lib}_LIBRARY
      NAMES ${_lib} lib${_lib}
      PATH_SUFFIXES lib64 lib)
  endforeach()
  _ffmpeg_define_targets()
endif()

find_package_handle_standard_args(FFmpeg
  REQUIRED_VARS FFMPEG_INCLUDE_DIR
  REASON_FAILURE_MESSAGE
    "FFmpeg not found. Linux: apt install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev. Windows: set FFmpeg_ROOT to an ffmpeg build (e.g. BtbN shared zip) or install via vcpkg.")
