# MonkeyCut

Frame-accurate, stream-copy video cutter for advertised TV recordings
(MPEG-TS/PS/AVI/MP4) with cutlist.at integration.
[Design brief](docs/DESIGN.md).

## Build

Requirements: CMake ≥ 3.21, C++20 compiler, Qt ≥ 6.5, FFmpeg ≥ 6
(libraries: avcodec, avformat, avutil, swscale, swresample).

### Linux (Debian/Ubuntu)

```sh
sudo apt install qt6-base-dev libavcodec-dev libavformat-dev libavutil-dev \
  libswscale-dev libswresample-dev ninja-build
cmake -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

### Windows

```bat
cmake -B build -G "Ninja Multi-Config" ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.9.2\msvc2022_64 ^
  -DFFmpeg_ROOT=C:\ffmpeg   (e.g. gyan.dev shared build)
cmake --build build --config Release
ctest --test-dir build -C Release
```

## Run

```sh
build/src/monkeycut
```