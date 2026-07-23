# iOS support for SpaceCadetPinball.
#
# Builds SDL2 and SDL2_mixer from source (static libs) for iOS via FetchContent,
# then packages the game as an iOS .app with the game data bundled as resources.
#
# This file is included from the top-level CMakeLists.txt when
# CMAKE_SYSTEM_NAME is "iOS". Configure with the helper script
# Platform/iOS/build-ios.sh, or manually:
#
#   cmake -S . -B build-ios -G Xcode \
#         -DCMAKE_SYSTEM_NAME=iOS \
#         -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
#         -DCMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=NO
#
# NOTE: The SDL / SDL_mixer FetchContent wiring below targets known-good
# releases; the very first Xcode build may still need a minor tweak
# (dependency versions move). See Platform/iOS/README-iOS.md.

include(FetchContent)

# Build every dependency (SDL2 AND SDL2_mixer) static and consistent. Without
# this, SDL2_mixer defaults to a shared build and records a different SDL2
# linkage than the static app, failing SDL's INTERFACE_SDL2_SHARED check.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# --- SDL2 (static) ---------------------------------------------------------
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
set(SDL_TEST   OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.30.9
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL2)

# NOTE: do NOT alias SDL2::SDL2 -> SDL2-static here. SDL2_mixer detects the
# static SDL2 target on its own; a shared-named alias makes it record
# INTERFACE_SDL2_SHARED=ON and then fails the static/shared consistency check.

# --- SDL2_mixer (static, WAV + MIDI only) ----------------------------------
# The game only uses .WAV sound effects and .MID music, so disable every
# external codec to keep the iOS build free of extra submodules.
set(SDL2MIXER_INSTALL   OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_SAMPLES   OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_VENDORED  OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_DEPS_SHARED OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_WAVE      ON  CACHE BOOL "" FORCE)
set(SDL2MIXER_MIDI      ON  CACHE BOOL "" FORCE)   # keep MIDI code path
# Only the built-in Timidity backend (no external libs). FluidSynth would
# require find_package(FluidSynth) which fails the iOS configure.
set(SDL2MIXER_MIDI_TIMIDITY   ON  CACHE BOOL "" FORCE)
set(SDL2MIXER_MIDI_FLUIDSYNTH OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MIDI_NATIVE     OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MOD       OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MP3       OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_FLAC      OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_OPUS      OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_VORBIS    OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_WAVPACK   OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_GME       OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_CMD       OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL2_mixer
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
    GIT_TAG        release-2.8.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL2_mixer)

# Expose the found libraries under the names the main CMakeLists expects, so
# the shared target_link_libraries() call works unchanged.
# This module is include()'d into the top-level listfile (same scope), so these
# plain set()s are visible to the rest of CMakeLists.txt.
if(TARGET SDL2::SDL2-static)
    set(SDL2_LIBRARY SDL2::SDL2-static)
else()
    set(SDL2_LIBRARY SDL2::SDL2)
endif()

if(TARGET SDL2_mixer::SDL2_mixer-static)
    set(SDL2_MIXER_LIBRARY SDL2_mixer::SDL2_mixer-static)
else()
    set(SDL2_MIXER_LIBRARY SDL2_mixer::SDL2_mixer)
endif()

set(SDL2_INCLUDE_DIR "")        # provided transitively by the targets
set(SDL2_MIXER_INCLUDE_DIR "")

# --- Bundle configuration helper -------------------------------------------
# Call after add_executable() to turn the target into an iOS app bundle and
# embed the game_resources/ folder so SDL_GetBasePath() can find PINBALL.DAT.
function(scp_configure_ios_bundle target)
    set(RES_DIR "${CMAKE_CURRENT_LIST_DIR}/game_resources")
    file(GLOB GAME_RESOURCE_FILES "${RES_DIR}/*")
    # Drop the resources into the bundle root (iOS bundles are flat, so the
    # app's Resources path == the .app directory that SDL reports as BasePath).
    set_source_files_properties(${GAME_RESOURCE_FILES} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(${target} PRIVATE ${GAME_RESOURCE_FILES})

    set(LAUNCH_SCREEN "${CMAKE_CURRENT_LIST_DIR}/Platform/iOS/LaunchScreen.storyboard")
    set_source_files_properties(${LAUNCH_SCREEN} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(${target} PRIVATE ${LAUNCH_SCREEN})

    # App icon. The icon is generated from the original game's artwork, so it is
    # not committed; run Platform/iOS/tools/make_icon.sh <image> to create it.
    # Without it the app simply builds with no custom icon.
    set(ASSET_CATALOG "${CMAKE_CURRENT_LIST_DIR}/Platform/iOS/Assets.xcassets")
    set(APP_ICON_PNG "${ASSET_CATALOG}/AppIcon.appiconset/icon_1024.png")
    if(EXISTS "${APP_ICON_PNG}")
        set_source_files_properties(${ASSET_CATALOG} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
        target_sources(${target} PRIVATE ${ASSET_CATALOG})
        set_target_properties(${target} PROPERTIES
            XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon")
    endif()

    # Bundle id can be overridden: -DSCP_BUNDLE_ID=com.yourname.spacecadetpinball
    if(NOT SCP_BUNDLE_ID)
        set(SCP_BUNDLE_ID "com.example.spacecadetpinball")
    endif()

    set_target_properties(${target} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_LIST_DIR}/Platform/iOS/Info.plist.in"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${SCP_BUNDLE_ID}"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"          # iPhone + iPad
    )

    if(CMAKE_OSX_SYSROOT MATCHES "simulator")
        # Simulator: unsigned is fine and avoids needing an Apple account.
        set_target_properties(${target} PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
            XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
            XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
        )
    else()
        # Physical device: must be signed. Xcode manages the provisioning
        # profile automatically once a DEVELOPMENT_TEAM is set.
        set_target_properties(${target} PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development"
            XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        )
    endif()

    # SDL provides main() on iOS via its UIKit entry point (libSDL2main).
    if(TARGET SDL2::SDL2main)
        target_link_libraries(${target} SDL2::SDL2main)
    endif()

    # iOS system frameworks SDL relies on (most come transitively, but be explicit).
    target_link_libraries(${target}
        "-framework AVFoundation"
        "-framework CoreAudio"
        "-framework CoreGraphics"
        "-framework CoreMotion"
        "-framework Foundation"
        "-framework GameController"
        "-framework Metal"
        "-framework OpenGLES"
        "-framework QuartzCore"
        "-framework UIKit"
    )
endfunction()
