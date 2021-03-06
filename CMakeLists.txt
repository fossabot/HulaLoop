######## Config ########

cmake_minimum_required (VERSION 3.5)

set (CMAKE_CXX_STANDARD 14)

# Make sure we can find our Find<package>.cmake files
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

include (${CMAKE_CURRENT_LIST_DIR}/cmake/Qt5.cmake)
include (${CMAKE_CURRENT_LIST_DIR}/cmake/Utility.cmake)

# Version
set (HL_VERSION_MAJOR 0)
set (HL_VERSION_MINOR 0)
set (HL_VERSION_REV 0)
set (HL_VERSION_BUILD 0)
set (HL_VERSION_STR "${HL_VERSION_MAJOR}.${HL_VERSION_MINOR}.${HL_VERSION_REV}.${HL_VERSION_BUILD}")

# Declare project
project (HulaLoop VERSION ${HL_VERSION_STR} LANGUAGES CXX C)

message (STATUS "")
message (STATUS "Building ${PROJECT_NAME} v${HL_VERSION_STR}")
message (STATUS "")

# Set build type if not provided
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Debug CACHE STRING "Configuration of build." FORCE)
endif ()

set (OSX FALSE)
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set (OSX TRUE)
endif ()

set (64BIT FALSE)
if (WIN32 AND CMAKE_GENERATOR_PLATFORM MATCHES "x64")
    set (64BIT TRUE)
endif ()

# Configuration code for installers
include (${CMAKE_CURRENT_LIST_DIR}/cmake/HulaInstall.cmake)

# Global build options
option (HL_BUILD_ONLY_AUDIO "Build only the audio library." OFF)
option (HL_BUILD_CLI "Build the command line application." ON)
option (HL_BUILD_GUI "Build the graphical application." ON)
option (HL_BUILD_EXAMPLES "Build example programs." OFF)

# Turn on coverage flags if needed
set (HL_ADD_COVERAGE_FLAGS OFF)
if (CMAKE_BUILD_TYPE MATCHES "Debug" AND (CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
    set (HL_ADD_COVERAGE_FLAGS ON)
endif ()

# Control debug output
if (NOT DEFINED HL_NO_DEBUG_OUTPUT)
    set (HL_NO_DEBUG_OUTPUT 1)
    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        set (HL_NO_DEBUG_OUTPUT 0)
    endif ()
endif ()

# Set output for binaries
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/lib)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/lib)
foreach (OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string (TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin)
    set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib/lib)
    set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib/lib)
endforeach ()

message (STATUS "System Platform: ${CMAKE_SYSTEM_NAME} ${CMAKE_GENERATOR_PLATFORM}")
message (STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}\n")

# Initialize lists
set (INCLUDE_DIRS)
set (HL_LIBRARIES)

# Link Windows specific libraries
if (WIN32)
    list (APPEND HL_LIBRARIES winmm.lib ) # TODO: Add required windows libraries
endif ()




######## Generated Headers ########
configure_file (
    "${PROJECT_SOURCE_DIR}/cmake/HulaVersion.h.in"
    "${CMAKE_BINARY_DIR}/HulaVersion.h"
)
include_directories (${CMAKE_BINARY_DIR})




######## Qt ########

if (NOT HL_BUILD_ONLY_AUDIO)
    # Prime for Qt
    set (CMAKE_INCLUDE_CURRENT_DIR ON)
    set (CMAKE_AUTOUIC ON)
    set (CMAKE_AUTOMOC ON)
    set (CMAKE_AUTORCC ON)

    if (WIN32 AND NOT DEFINED QT_ROOT)
        if (DEFINED ENV{QT_ROOT})
            set (QT_ROOT $ENV{QT_ROOT} CACHE STRING "Path to Qt Installation")
        else ()
            message (FATAL_ERROR "QT_ROOT environment variable not set. Add the environment variable in Windows Environment Variables or cmake with -DQT_ROOT=C:\\path\\to\\qt\\5.XX.X")
        endif ()
    elseif (WIN32)
        set (QT_ROOT $ENV{QT_ROOT} CACHE STRING "Path to Qt Installation")
    endif ()

    message (STATUS "QT_ROOT: " ${QT_ROOT} "\n")

    if (WIN32 AND NOT DEFINED Qt5_DIR)
        if(64BIT)
            set (Qt5_DIR "${QT_ROOT}\\msvc2017_64\\lib\\cmake\\Qt5")
        else ()
            set (Qt5_DIR "${QT_ROOT}\\msvc2015\\lib\\cmake\\Qt5")
        endif ()
    endif ()

    message (STATUS "Qt5_DIR: ${Qt5_DIR}")

    # Required Qt packages
    find_package (Qt5 REQUIRED
        Core
    )

    # Minimum set of libs required
    # for translation
    set (QT_TRANSLATION_LIBS
        Qt5::Core
    )

    if (HL_BUILD_GUI)
        find_package (Qt5 REQUIRED
            Gui
            Qml
            Quick
            QuickControls2
            Widgets
        )

        list (APPEND HL_LIBRARIES
            Qt5::Core
            Qt5::Gui
            Qt5::Qml
            Qt5::Quick
            Qt5::QuickControls2
            Qt5::Widgets
        )
    endif ()

    # Required Sndfile packages
    find_package (SndFile REQUIRED)

    add_include_dir (${LIBSNDFILE_INCLUDE})
    list (APPEND HL_LIBRARIES ${LIBSNDFILE_LIB})

    # Copy DLL to application bin folder
    if (WIN32)
        MESSAGE (STATUS "Found SndFile DLL: ${LIBSNDFILE_DLL}")
        file (COPY ${LIBSNDFILE_DLL} DESTINATION ${CMAKE_BINARY_DIR}/bin)
    endif ()
endif ()

######## Libraries ########

# Setup flags for all external library builds
if (WIN32 AND 64BIT)
    set (HL_EXT_LIB_FLAGS -A x64)
endif ()

# Build and install portaudio to our build/lib folder
add_subdirectory (src/libs/portaudio)

# Add coverage flags for GCC after 3rd party libs are built
set (CXX_FLAGS_NOCOV "${CMAKE_CXX_FLAGS}")
if (HL_ADD_COVERAGE_FLAGS)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
endif ()

# Add subdirectories and include any headers from lower level CMakeLists
add_subdirectory (src/audio)
include_directories (${INCLUDE_DIRS})

# Build UI and control modules
if (NOT HL_BUILD_ONLY_AUDIO)
    add_subdirectory (src/control)
    include_directories (${INCLUDE_DIRS})

    if (HL_BUILD_CLI)
        add_subdirectory (src/ui/cli)
        include_directories (${INCLUDE_DIRS})
    endif ()

    if (HL_BUILD_GUI)
        add_subdirectory (src/ui/gui)
        add_subdirectory (src/launcher)
        include_directories (${INCLUDE_DIRS})
    endif ()

    add_subdirectory (${PROJECT_SOURCE_DIR}/res/translations)
    include (${CMAKE_CURRENT_LIST_DIR}/cmake/Translation.cmake)

    if (HL_BUILD_EXAMPLES)
        add_subdirectory (examples)
    endif ()
endif ()

install (
    DIRECTORY ${CMAKE_BINARY_DIR}/bin
    DESTINATION .
    USE_SOURCE_PERMISSIONS
    PATTERN "*.ilk" EXCLUDE
    PATTERN "*.pdb" EXCLUDE
)




######## Testing ########
# GUI tests are in Testing.cmake
# Updater tests are in Testing.cmake

# Build settings for DEBUG builds only
if (CMAKE_BUILD_TYPE MATCHES "Debug")

    include (${CMAKE_CURRENT_LIST_DIR}/cmake/Testing.cmake)

    # Test that only rely on the audio library
    create_test ("src/test/TestOSAudio.cpp" "" 3 FALSE FALSE)
    create_test ("src/test/TestHulaRingBuffer.cpp" "" 1 TRUE FALSE)
    create_test ("src/test/TestController.cpp" "" -1 TRUE FALSE)

    if (OSX)
        create_test ("src/test/TestOSXAudio.cpp" "" -1 FALSE FALSE)
    elseif (WIN32)
        create_test ("src/test/TestWindowsAudio.cpp" "" -1 FALSE FALSE)
    endif ()

    if (NOT HL_BUILD_ONLY_AUDIO)
        create_test ("src/test/TestTransport.cpp" "" -1 FALSE FALSE)
        create_test ("src/test/TestRecord.cpp" "" -1 FALSE FALSE)

        if (HL_BUILD_CLI)
            create_test ("src/test/TestCLIArgs.cpp" "" -1 TRUE FALSE)
            create_test("src/test/TestInteractiveCLI.cpp" "src/ui/cli/InteractiveCLI.cpp" -1 TRUE FALSE)
        endif ()
    endif ()

endif ()

message (STATUS "")
