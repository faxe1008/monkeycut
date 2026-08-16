add_library(monkeycut STATIC
  core/Time.cpp
  core/MediaInfo.h
  core/GopMap.h
  core/Cut.h
  core/Cutlist.cpp
  core/Cutlist.h
  core/CutPlanner.cpp
  core/CutPlanner.h
  cut/CulFile.cpp
  cut/CulFile.h
  cut/Project.cpp
  cut/Project.h
  av/AvProbe.cpp
  av/GopScanner.cpp
  av/GopScanner.h
  av/GopCache.cpp
  av/GopCache.h
  av/Player.cpp
  av/Player.h
  av/ffmpeg_c.h
  av/AvProbe.h
  av/VfrDetect.cpp
  av/VfrDetect.h
  core/Time.h
  core/TimeCode.cpp
  core/TimeCode.h
  core/Fps.h
  ui/MainWindow.cpp
  ui/MainWindow.h
  ui/VideoView.cpp
  ui/VideoView.h
  ui/TimelineBar.cpp
  ui/TimelineBar.h
)
target_include_directories(monkeycut PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(monkeycut PUBLIC
  Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network
  FFmpeg::avcodec FFmpeg::avformat FFmpeg::avutil FFmpeg::swscale FFmpeg::swresample
)

add_executable(monkeycut_app main.cpp)
set_target_properties(monkeycut_app PROPERTIES OUTPUT_NAME "monkeycut")
target_link_libraries(monkeycut_app PRIVATE monkeycut)