cmake_minimum_required(VERSION 3.21)
project(monkeycut VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network Test)
find_package(FFmpeg REQUIRED)

# --- i18n: German (default) / English at parity -----------------------------
set(MC_I18N_LANGUAGES de en)
set(MC_TS_FILES "")
set(MC_QM_FILES "")
foreach(lang IN LISTS MC_I18N_LANGUAGES)
  list(APPEND MC_TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/translations/monkeycut_${lang}.ts")
  list(APPEND MC_QM_FILES "${CMAKE_CURRENT_BINARY_DIR}/translations/monkeycut_${lang}.qm")
endforeach()

set(MC_I18N_SOURCES
  src/main.cpp
  src/ui/MainWindow.cpp  src/ui/MainWindow.h
  src/ui/VideoView.cpp   src/ui/VideoView.h
  src/ui/TimelineBar.cpp src/ui/TimelineBar.h
  src/ui/CutlistAtDialog.cpp src/ui/CutlistAtDialog.h
  src/ui/SettingsDialog.cpp  src/ui/SettingsDialog.h
)
find_program(LUPDATE_EXECUTABLE NAMES lupdate lupdate6 lupdate-qt6
             HINTS ${CMAKE_PREFIX_PATH} /usr/lib/qt6/bin /usr/bin /usr/lib/qt6/libexec)
find_program(LRELEASE_EXECUTABLE NAMES lrelease lrelease6 lrelease-qt6
             HINTS ${CMAKE_PREFIX_PATH} /usr/lib/qt6/bin /usr/bin /usr/lib/qt6/libexec)

# lupdate/lrelease must be usable (mixed Qt versions break them)
set(MC_I18N_TOOLS_OK ON)
if(LRELEASE_EXECUTABLE)
  execute_process(COMMAND ${LRELEASE_EXECUTABLE} -version
                  RESULT_VARIABLE _lrc OUTPUT_QUIET
                  ERROR_VARIABLE _lrc)
  if(NOT _lrc EQUAL 0)
    set(MC_I18N_TOOLS_OK OFF)
  endif()
endif()

if(MC_I18N_TOOLS_OK AND LUPDATE_EXECUTABLE)
  add_custom_target(monkeycut_lupdate
    COMMAND ${LUPDATE_EXECUTABLE} ${MC_I18N_SOURCES}
            -ts ${CMAKE_CURRENT_SOURCE_DIR}/translations/monkeycut_de.ts
            -ts ${CMAKE_CURRENT_SOURCE_DIR}/translations/monkeycut_en.ts
            -locations none
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Extracting translatable strings (lupdate)")
endif()

if(MC_I18N_TOOLS_OK)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/translations")
# Qt 6.11+: lrelease takes one -qm output per invocation
foreach(lang IN LISTS MC_I18N_LANGUAGES)
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/translations/monkeycut_${lang}.qm
    COMMAND ${LRELEASE_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/translations/monkeycut_${lang}.ts
            -qm ${CMAKE_CURRENT_BINARY_DIR}/translations/monkeycut_${lang}.qm
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/translations/monkeycut_${lang}.ts
    COMMENT "Compiling ${lang} translations (lrelease)")
endforeach()
add_custom_target(monkeycut_translations DEPENDS ${MC_QM_FILES})

# ship the .qm files in a Qt resource: :/i18n/monkeycut_<lang>.qm
set(MC_QRC_ENTRIES "")
foreach(qm IN LISTS MC_QM_FILES)
  get_filename_component(qm_base "${qm}" NAME)
  string(APPEND MC_QRC_ENTRIES "    <file alias=\"${qm_base}\">${qm}</file>\n")
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/monkeycut_i18n.qrc"
"<RCC>
  <resource prefix=\"/i18n\">
${MC_QRC_ENTRIES}  </resource>
</RCC>
")
endif()  # MC_I18N_TOOLS_OK


add_subdirectory(src)

if(MC_I18N_TOOLS_OK)
  qt6_add_resources(monkeycut monkeycut_i18n.qrc)
  add_dependencies(monkeycut monkeycut_translations)
  if(NOT LUPDATE_EXECUTABLE)
    message(WARNING "lupdate not found - monkeycut_lupdate target unavailable")
  endif()
else()
  message(WARNING "lrelease unusable (mixed Qt versions?) - building without "
                  "compiled translations")
endif()

enable_testing()
add_subdirectory(tests)