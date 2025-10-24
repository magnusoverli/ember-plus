################################### Metadata ###################################
cmake_minimum_required(VERSION 3.9 FATAL_ERROR)

# Enable sane rpath handling on macOS
cmake_policy(SET CMP0042 NEW)
# Allow version in project definition
cmake_policy(SET CMP0048 NEW)
# Allow visibility definitions
cmake_policy(SET CMP0063 NEW)
# Allow interprocedural optimization
cmake_policy(SET CMP0069 NEW)

project(EmberViewer VERSION 1.0.0 LANGUAGES CXX)

# Use GNUInstallDirs to make sure libraries are installed into correct locations
# on all platforms.
include(GNUInstallDirs)

################################### Options ####################################

# Cross-platform considerations
if(WIN32)
    # Windows-specific settings
    set(CMAKE_WIN32_EXECUTABLE ON)
endif()

################################# Main Project #################################

# If libs101 is not already defined (e.g. because this is the toplevel invocation),
# look it up via find_package.
if(NOT TARGET libs101::s101)
    find_package(libs101 REQUIRED)
endif()

# If libformula is not already defined (e.g. because this is the toplevel invocation),
# look it up via find_package.
if(NOT TARGET libformula::formula)
    find_package(libformula REQUIRED)
endif()

# Choose whether to link against the static or the shared variant of libember
# depending on the preference specified by the invocation.
if (NOT BUILD_SHARED_LIBS)
    set(LIBEMBER_TARGET libember::ember-static)
else()
    set(LIBEMBER_TARGET libember::ember-shared)
endif()

# If libember is not already defined (e.g. because this is the toplevel invocation),
# look it up via find_package.
if(NOT TARGET ${LIBEMBER_TARGET})
    find_package(libember REQUIRED)
endif()

# Find Qt5 - works on Linux, Windows, and macOS
find_package(Qt5 COMPONENTS Core Gui Widgets Network REQUIRED)

# Collect source files
file(GLOB_RECURSE SOURCE_FILES
    src/*.cpp
    src/*.h
    include/*.h
)

# Collect UI files
file(GLOB UI_FILES
    ui/*.ui
)

# Collect resource files
file(GLOB RESOURCE_FILES
    resources/*.qrc
)

# Create a library for shared code (for testing)
add_library(EmberViewerLib STATIC
    src/MatrixWidget.cpp
    src/ParameterDelegate.cpp
    include/MatrixWidget.h
    include/ParameterDelegate.h
)

target_compile_features(EmberViewerLib
    PRIVATE
        cxx_std_11
)

set_target_properties(EmberViewerLib
    PROPERTIES
        AUTOMOC ON
        POSITION_INDEPENDENT_CODE ON
)

target_include_directories(EmberViewerLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(EmberViewerLib
    PUBLIC
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        Qt5::Network
        libs101::s101
        libformula::formula
        ${LIBEMBER_TARGET}
)

add_executable(${PROJECT_NAME}
    ${SOURCE_FILES}
    ${UI_FILES}
    ${RESOURCE_FILES}
)

target_compile_features(${PROJECT_NAME}
    PRIVATE
        cxx_std_11
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        AUTOMOC                      ON
        AUTOUIC                      ON
        AUTORCC                      ON
        POSITION_INDEPENDENT_CODE    ON
        C_EXTENSIONS                 OFF
        CXX_EXTENSIONS               OFF
)

# Include directories
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        Qt5::Network
        libs101::s101
        libformula::formula
        ${LIBEMBER_TARGET}
)

# Platform-specific settings
if(WIN32)
    # Windows: Add application icon
    # target_sources(${PROJECT_NAME} PRIVATE resources/app.rc)
endif()

if(APPLE)
    # macOS: Add application bundle settings
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.magnusoverli.emberviewer"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    )
endif()

# Add compiler optimizations for Release builds
if(NOT CMAKE_BUILD_TYPE MATCHES "Debug")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${PROJECT_NAME} PRIVATE
            -O3              # Maximum optimization
            -march=native    # Optimize for current CPU
            -DNDEBUG         # Disable assertions
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${PROJECT_NAME} PRIVATE
            /O2              # Maximum optimization
            /GL              # Whole program optimization
            /DNDEBUG         # Disable assertions
        )
    endif()
endif()

# Add the IPO property for all relevant targets, if we are building in the
# release configuration and the platform supports it.
if (NOT CMAKE_BUILD_TYPE MATCHES "Debug")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported)

    if(ipo_supported)
        set_target_properties(${PROJECT_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
    endif()
endif()

# <<<  Install  >>>

install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Windows: Install Qt DLLs alongside executable
if(WIN32)
    # Add windeployqt for automatic Qt DLL deployment
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt5_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${WINDEPLOYQT_EXECUTABLE} $<TARGET_FILE:${PROJECT_NAME}>
        )
    endif()
endif()


# <<<  Tests  >>>
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

