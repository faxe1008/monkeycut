include(FindPackageHandleStandardArgs)

set(MONKEYCUT_FFMPEG_LIBS avcodec avformat avutil swscale swresample)

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
  # BtbN / vcpkg style layout: <root>/include/libavcodec/avcodec.h, ...
  # Our sources include <libavcodec/...>, so the include root is the
  # directory that contains the libXXX subdirectories.
  find_path(FFMPEG_AVCODEC_HDR_DIR avcodec.h
    HINTS ${FFmpeg_ROOT} PATH_SUFFIXES include/libavcodec)
  if(FFMPEG_AVCODEC_HDR_DIR)
    get_filename_component(FFMPEG_INCLUDE_DIR "${FFMPEG_AVCODEC_HDR_DIR}" DIRECTORY)
  endif()
  foreach(_lib IN LISTS MONKEYCUT_FFMPEG_LIBS)
    find_library(FFMPEG_${_lib}_LIBRARY
      NAMES ${_lib} lib${_lib}
      HINTS ${FFmpeg_ROOT} PATH_SUFFIXES lib64 lib bin)
    if(NOT TARGET FFmpeg::${_lib})
      if(FFMPEG_${_lib}_LIBRARY)
        add_library(FFmpeg::${_lib} UNKNOWN IMPORTED)
        set_target_properties(FFmpeg::${_lib} PROPERTIES
          IMPORTED_LOCATION "${FFMPEG_${_lib}_LIBRARY}"
          INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIR}")
      endif()
    endif()
  endforeach()
  foreach(_lib IN LISTS MONKEYCUT_FFMPEG_LIBS)
    if(NOT FFMPEG_${_lib}_LIBRARY)
      set(FFMPEG_${_lib}_FOUND FALSE)
    endif()
  endforeach()
endif()

find_package_handle_standard_args(FFmpeg
  REQUIRED_VARS FFMPEG_INCLUDE_DIR
  REASON_FAILURE_MESSAGE
    "FFmpeg not found. Linux: apt install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev. Windows: set FFmpeg_ROOT to an ffmpeg build (e.g. gyan.dev essentials) or install via vcpkg.")