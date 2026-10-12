cmake_minimum_required(VERSION 3.24)
project(TuxPaintIPad VERSION 0.9.31 LANGUAGES C CXX OBJC)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
  message(FATAL_ERROR "Configure with -DCMAKE_SYSTEM_NAME=iOS and the iPhone SDK.")
endif()
include(FetchContent)
set(CMAKE_C_STANDARD 11)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Static dependencies" FORCE)
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)
set(CMAKE_XCODE_ATTRIBUTE_ENABLE_BITCODE NO)
set(CMAKE_XCODE_ATTRIBUTE_SKIP_INSTALL YES)
set(TUXPAINT_BUILD_NUMBER "1" CACHE STRING "App Store Connect build number")
set(TUXPAINT_SIGNING_TEAM "" CACHE STRING "Apple developer team for signed builds")
set(TUXPAINT_SIGNING_PROFILE "" CACHE STRING "Distribution provisioning profile UUID")

# SDL2 remains the API used by this Tux Paint port. Pin every dependency.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
FetchContent_Declare(sdl GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-2.32.10 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(sdl)

set(PNG_SHARED OFF CACHE BOOL "" FORCE)
set(PNG_TESTS OFF CACHE BOOL "" FORCE)
set(PNG_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(png GIT_REPOSITORY https://github.com/pnggroup/libpng.git
  GIT_TAG v1.6.50 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(png)

set(SDL2IMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_TESTS OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_DEPS_SHARED OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_AVIF OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_TIF OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_WEBP OFF CACHE BOOL "" FORCE)
FetchContent_Declare(sdl_image GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
  GIT_TAG release-2.8.8 GIT_SHALLOW TRUE GIT_SUBMODULES "")
FetchContent_MakeAvailable(sdl_image)

set(SDL2TTF_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2TTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDL2TTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
FetchContent_Declare(sdl_ttf GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
  GIT_TAG release-2.24.0 GIT_SHALLOW TRUE GIT_SUBMODULES external/freetype)
FetchContent_MakeAvailable(sdl_ttf)

# Tux Paint assets use WAV, Ogg Vorbis and MP3. These decoders are bundled.
set(SDL2MIXER_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_DEPS_SHARED OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MOD OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MIDI OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_OPUS OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_WAVPACK OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_VORBIS STB CACHE STRING "" FORCE)
FetchContent_Declare(sdl_mixer GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
  GIT_TAG release-2.8.1 GIT_SHALLOW TRUE GIT_SUBMODULES "")
FetchContent_MakeAvailable(sdl_mixer)

FetchContent_Declare(gfx
  URL https://www.ferzkopp.net/Software/SDL2_gfx/SDL2_gfx-1.0.4.tar.gz
  URL_HASH SHA256=63e0e01addedc9df2f85b93a248f06e8a04affa014a835c2ea34bfe34e576262
  SOURCE_SUBDIR no-cmake)
FetchContent_Declare(intl GIT_REPOSITORY https://github.com/youknowone/libintl-alternative.git
  GIT_TAG 17f48714e6a77ec331585e65900ff399d1f60706 SOURCE_SUBDIR no-cmake)
# The last C release provides the quantizer API used by Tux Paint's GIF export.
FetchContent_Declare(quant GIT_REPOSITORY https://github.com/ImageOptim/libimagequant.git
  GIT_TAG 2.17.0 GIT_SHALLOW TRUE SOURCE_SUBDIR no-cmake)
FetchContent_MakeAvailable(gfx intl quant)
add_library(gfx STATIC ${gfx_SOURCE_DIR}/SDL2_rotozoom.c
  ${gfx_SOURCE_DIR}/SDL2_gfxPrimitives.c ${gfx_SOURCE_DIR}/SDL2_imageFilter.c
  ${gfx_SOURCE_DIR}/SDL2_framerate.c)
target_include_directories(gfx PUBLIC ${gfx_SOURCE_DIR})
target_link_libraries(gfx PUBLIC SDL2::SDL2-static)
add_library(intl STATIC ${intl_SOURCE_DIR}/libintl.m)
target_include_directories(intl PUBLIC ${intl_SOURCE_DIR})
target_link_libraries(intl PUBLIC "-framework Foundation")
add_library(imagequant STATIC ${quant_SOURCE_DIR}/libimagequant.c
  ${quant_SOURCE_DIR}/blur.c ${quant_SOURCE_DIR}/mediancut.c
  ${quant_SOURCE_DIR}/mempool.c ${quant_SOURCE_DIR}/nearest.c
  ${quant_SOURCE_DIR}/pam.c ${quant_SOURCE_DIR}/kmeans.c)
target_include_directories(imagequant PUBLIC ${quant_SOURCE_DIR})

set(TUX "${CMAKE_CURRENT_SOURCE_DIR}/../tuxpaint-tuxpaint")
set(core_sources cursor dirwalk fill fonts get_fname gifenc i18n im
  onscreen_keyboard pixels playsound progressbar rgblinear sounds tuxpaint)
set(app_sources IOSBridge.m ../IOS/TuxPaint/language.m
  "${TUX}/src/ios.m" "${TUX}/src/ios_print.m" "${TUX}/obj/parse.c")
foreach(source IN LISTS core_sources)
  list(APPEND app_sources "${TUX}/src/${source}.c")
endforeach()
file(GLOB magic_sources CONFIGURE_DEPENDS "${TUX}/magic/src/*.c")
add_executable(TuxPaint MACOSX_BUNDLE ${app_sources} ${magic_sources})
target_include_directories(TuxPaint PRIVATE "${TUX}/src" "${TUX}/src/mouse"
  "${png_SOURCE_DIR}" "${png_BINARY_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/include")
# The older port mixes SDL2/foo.h with foo.h includes.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/SDL2")
file(GLOB sdl_headers "${sdl_SOURCE_DIR}/include/*.h"
  "${sdl_image_SOURCE_DIR}/include/*.h" "${sdl_mixer_SOURCE_DIR}/include/*.h"
  "${sdl_ttf_SOURCE_DIR}/*.h")
file(COPY ${sdl_headers} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/include/SDL2")
target_compile_definitions(TuxPaint PRIVATE
  HAVE_STRCASESTR NOSVG
  DATA_PREFIX="data/" LOCALEDIR="locale/" MAGIC_PREFIX="data/"
  DOC_PREFIX="docs/" CONFDIR="" IMDIR="im/"
  VER_VERSION="0.9.31-ipad" VER_DATE="2026-10-11")
target_link_libraries(TuxPaint PRIVATE SDL2::SDL2main SDL2::SDL2-static
  SDL2_image::SDL2_image-static SDL2_mixer::SDL2_mixer-static
  SDL2_ttf::SDL2_ttf-static gfx png_static imagequant intl
  "-framework Photos" "-framework UIKit" "-framework Foundation"
  "-framework ImageIO" "-framework MobileCoreServices" "-lz" "-liconv")
set_target_properties(TuxPaint PROPERTIES
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
  MACOSX_BUNDLE_GUI_IDENTIFIER "org.ladyada.tuxpaint.ipad"
  MACOSX_BUNDLE_BUNDLE_VERSION "${TUXPAINT_BUILD_NUMBER}"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "0.9.31"
  XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "2"
  XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "org.ladyada.tuxpaint.ipad"
  XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon")
if(TUXPAINT_SIGNING_PROFILE)
  set_target_properties(TuxPaint PROPERTIES
    XCODE_GENERATE_SCHEME YES
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED YES
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE Manual
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Distribution"
    XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${TUXPAINT_SIGNING_TEAM}"
    XCODE_ATTRIBUTE_PROVISIONING_PROFILE_SPECIFIER "${TUXPAINT_SIGNING_PROFILE}"
    XCODE_ATTRIBUTE_SKIP_INSTALL NO
    XCODE_ATTRIBUTE_INSTALL_PATH "/Applications")
endif()
target_sources(TuxPaint PRIVATE "../IOS/TuxPaint/TuxPaint/Assets.xcassets")
set_source_files_properties("../IOS/TuxPaint/TuxPaint/Assets.xcassets"
  PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
add_custom_command(TARGET TuxPaint POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${TUX}/data" "$<TARGET_BUNDLE_DIR:TuxPaint>/data"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${TUX}/starters" "$<TARGET_BUNDLE_DIR:TuxPaint>/data/starters"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${TUX}/templates" "$<TARGET_BUNDLE_DIR:TuxPaint>/data/templates"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${TUX}/im" "$<TARGET_BUNDLE_DIR:TuxPaint>/im"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${TUX}/osk" "$<TARGET_BUNDLE_DIR:TuxPaint>/data/osk"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${TUX}/fonts" "$<TARGET_BUNDLE_DIR:TuxPaint>/data/fonts"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${TUX}/stamps" "$<TARGET_BUNDLE_DIR:TuxPaint>/data/stamps"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../tuxpaint-stamps-2023.07.20/stamps" "$<TARGET_BUNDLE_DIR:TuxPaint>/data/stamps"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/tuxpaint.cfg" "$<TARGET_BUNDLE_DIR:TuxPaint>/tuxpaint.cfg")
