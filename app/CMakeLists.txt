find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Multimedia)

set(CMAKE_AUTOMOC ON)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_library(pacman_view STATIC
    AndroidBridge.cpp
    AndroidBridge.hpp
    AppSettings.cpp
    AppSettings.hpp
    GameController.cpp
    GameController.hpp
    ScreenInsets.cpp
    ScreenInsets.hpp
    SfxPlayer.cpp
    SfxPlayer.hpp
)

qt_add_qml_module(pacman_view
    URI PacMan
    VERSION 1.0
    NO_PLUGIN
    # The generated files have to end in the target path, otherwise the
    # tooling (qmllint) does not find the module.
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/PacMan"
    QML_FILES
        Main.qml
)

qt_add_resources(pacman_view "pacman_assets"
    PREFIX "/"
    FILES
        assets/sprites/tiles.svg
        assets/sprites/pacman.svg
        assets/icons/pacman.svg
        assets/sprites/ghost_red.svg
        assets/sprites/ghost_pink.svg
        assets/sprites/ghost_cyan.svg
        assets/sprites/ghost_orange.svg
        assets/sprites/eyes.svg
        assets/sprites/frightened.svg
        assets/sprites/life.svg
        assets/audio/munch.wav
        assets/audio/power.wav
        assets/audio/ghost_eaten.wav
        assets/audio/death.wav
)

# The campaign travels inside the binary as well: PACMAN_LEVELS_DIR only points
# at the source tree of the build host, which does not exist on a phone (or any
# installed copy). GameController materialises these files into the app data
# directory on startup and loads them from there. Mirrors the source directory's
# "*.txt" rule, so a new level file needs no CMake edit.
file(GLOB PACMAN_LEVEL_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/levels/*.txt")
qt_add_resources(pacman_view "pacman_levels"
    PREFIX "/levels"
    BASE "${CMAKE_SOURCE_DIR}/levels"
    FILES ${PACMAN_LEVEL_FILES}
)

target_link_libraries(pacman_view PUBLIC
    pacman_core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
)

target_compile_definitions(pacman_view PRIVATE
    PACMAN_LEVELS_DIR="${CMAKE_SOURCE_DIR}/levels"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pacman_view PRIVATE -Wall -Wextra -Wpedantic)
endif()

qt_add_executable(pacman_app MANUAL_FINALIZATION main.cpp)

target_link_libraries(pacman_app PRIVATE pacman_view)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pacman_app PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ANDROID)
    # The Gradle template bundles androidx.core:core, which requires a
    # compileSdk of at least 36. Keep ANDROID_PLATFORM (the NDK API level)
    # at 34 and only raise the Gradle-side compile SDK.
    set_target_properties(pacman_app PROPERTIES
        QT_ANDROID_COMPILE_SDK_VERSION "android-36"
    )

    # Launcher icon and label.
    #
    # QT_ANDROID_PACKAGE_SOURCE_DIR contributes the res/ tree (adaptive icon
    # for API 26+ plus the legacy bitmaps); QT_ANDROID_APP_ICON puts
    # android:icon="@mipmap/ic_launcher" into the generated manifest, and
    # QT_ANDROID_APP_NAME replaces the default "pacman_app" label.
    set_target_properties(pacman_app PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../android/package"
        QT_ANDROID_APP_ICON "@mipmap/ic_launcher"
        QT_ANDROID_APP_NAME "Pac-Man"
    )
endif()

qt_finalize_executable(pacman_app)
