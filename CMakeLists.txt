cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 COMPONENTS Core Widgets Network REQUIRED)

target_link_libraries(
  ${CMAKE_PROJECT_NAME}
  PRIVATE OBS::libobs OBS::obs-frontend-api Qt6::Core Qt6::Widgets Qt6::Network)

set_target_properties(
  ${CMAKE_PROJECT_NAME}
  PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)

target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_17)
target_compile_options(${CMAKE_PROJECT_NAME}
                       PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src)

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE src/plugin-main.cpp
          src/data_model.cpp
          src/data_model.h
          src/store.cpp
          src/store.h
          src/text_renderer.cpp
          src/text_renderer.h
          src/transition_math.h
          src/letras_state.h
          src/letras_source.cpp
          src/letras_source.h
          src/txt_import.cpp
          src/txt_import.h
          src/letter_size_dialog.cpp
          src/letter_size_dialog.h
          src/song_editor_dialog.cpp
          src/song_editor_dialog.h
          src/theme_editor_dialog.cpp
          src/theme_editor_dialog.h
          src/transitions_dialog.cpp
          src/transitions_dialog.h
          src/main_window.cpp
src/main_window.h
src/web_server.cpp
src/web_server.h)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME
                                                              ${_name})
