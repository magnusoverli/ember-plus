cmake_minimum_required(VERSION 3.20 FATAL_ERROR)
cmake_policy(SET CMP0042 NEW)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0063 NEW)
cmake_policy(SET CMP0069 NEW)
if(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(CMAKE_C_COMPILER_ABI_CHECKED TRUE)
    set(CMAKE_CXX_COMPILER_ABI_CHECKED TRUE)
endif()
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY TRUE)
find_package(Git QUIET)
set(VERSION_MAJOR 0)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)
set(GIT_VERSION "unknown")
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --match "v*" --abbrev=7 --dirty
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE GIT_RESULT
    )
    if(GIT_RESULT EQUAL 0 AND GIT_VERSION MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)")
        set(VERSION_MAJOR ${CMAKE_MATCH_1})
        set(VERSION_MINOR ${CMAKE_MATCH_2})
        set(VERSION_PATCH ${CMAKE_MATCH_3})
        message(STATUS "Detected version from git: ${GIT_VERSION}")
    else()
        message(WARNING "Could not detect version from git tags, using fallback version 0.0.0")
        set(GIT_VERSION "0.0.0-unknown")
    endif()
else()
    message(WARNING "Git not found, using fallback version 0.0.0")
    set(GIT_VERSION "0.0.0-unknown")
endif()
set(PROJECT_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
project(EmberViewer VERSION ${PROJECT_VERSION} LANGUAGES CXX)
if(DEFINED ENV{EMBERVIEWER_UPDATE_SECRET})
    set(GITHUB_API_TOKEN $ENV{EMBERVIEWER_UPDATE_SECRET})
    message(STATUS "GitHub API token loaded from environment")
endif()
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/update_version.cmake "
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND \${GIT_EXECUTABLE} describe --tags --match \"v*\" --abbrev=7 --dirty
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE GIT_RESULT
    )
    if(GIT_RESULT EQUAL 0 AND GIT_VERSION MATCHES \"^v([0-9]+)\\\\.([0-9]+)\\\\.([0-9]+)\")
        set(VERSION_MAJOR \${CMAKE_MATCH_1})
        set(VERSION_MINOR \${CMAKE_MATCH_2})
        set(VERSION_PATCH \${CMAKE_MATCH_3})
    else()
        set(VERSION_MAJOR 0)
        set(VERSION_MINOR 0)
        set(VERSION_PATCH 0)
        set(GIT_VERSION \"0.0.0-unknown\")
    endif()
else()
    set(VERSION_MAJOR 0)
    set(VERSION_MINOR 0)
    set(VERSION_PATCH 0)
    set(GIT_VERSION \"0.0.0-unknown\")
endif()
set(PROJECT_VERSION \"\${VERSION_MAJOR}.\${VERSION_MINOR}.\${VERSION_PATCH}\")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/version.h
    @ONLY
)
")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/version.h
    @ONLY
)
message(STATUS "EmberViewer version: ${PROJECT_VERSION} (${GIT_VERSION})")
include(GNUInstallDirs)
option(BUILD_TESTING "Build the testing tree" ON)
if(WIN32)
    set(CMAKE_WIN32_EXECUTABLE ON)
endif()
if(NOT TARGET libs101::s101)
    find_package(libs101 REQUIRED)
endif()
if (NOT BUILD_SHARED_LIBS)
    set(LIBEMBER_TARGET libember::ember-static)
else()
    set(LIBEMBER_TARGET libember::ember-shared)
endif()
if(NOT TARGET ${LIBEMBER_TARGET})
    find_package(libember REQUIRED)
endif()
find_package(Qt6 6.8 COMPONENTS Core Gui Widgets Network REQUIRED)
file(GLOB UI_FILES
    ui/*.ui
)
file(GLOB RESOURCE_FILES
    resources/*.qrc
)
add_custom_target(update_version
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/update_version.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Updating version information from git"
    VERBATIM
)
if(WIN32)
    set(PLATFORM_UPDATE_SOURCES
        src/WindowsUpdateManager.cpp
        include/WindowsUpdateManager.h
    )
elseif(APPLE)
    set(PLATFORM_UPDATE_SOURCES
        src/MacUpdateManager.cpp
        include/MacUpdateManager.h
    )
else()
    set(PLATFORM_UPDATE_SOURCES
        src/LinuxUpdateManager.cpp
        include/LinuxUpdateManager.h
    )
endif()
add_library(EmberViewerLib STATIC
    src/MatrixModel.cpp
    src/VirtualizedMatrixWidget.cpp
    src/VirtualizedHeaderView.cpp
    src/VirtualizedSidebarView.cpp
    src/MeterWidget.cpp
    src/TriggerWidget.cpp
    src/SliderWidget.cpp
    src/GraphWidget.cpp
    src/ParameterDelegate.cpp
    src/DeviceSnapshot.cpp
    src/UpdateManager.cpp
    src/UpdateDialog.cpp
    src/EmberTreeWidget.cpp
    src/S101Protocol.cpp
    src/GlowParser.cpp
    src/StreamingDomReader.cpp
    src/TreeViewController.cpp
    src/SubscriptionManager.cpp
    src/MatrixManager.cpp
    src/CrosspointActivityTracker.cpp
    src/SnapshotManager.cpp
    src/FunctionInvoker.cpp
    src/TreeFetchService.cpp
    src/CacheManager.cpp
    src/BusySpinner.cpp
    src/EmberConnection.cpp
    src/EmberProvider.cpp
    ${PLATFORM_UPDATE_SOURCES}
    include/MatrixModel.h
    include/VirtualizedMatrixWidget.h
    include/VirtualizedHeaderView.h
    include/VirtualizedSidebarView.h
    include/MeterWidget.h
    include/TriggerWidget.h
    include/SliderWidget.h
    include/GraphWidget.h
    include/ParameterDelegate.h
    include/DeviceSnapshot.h
    include/UpdateManager.h
    include/UpdateDialog.h
    include/EmberTreeWidget.h
    include/S101Protocol.h
    include/GlowParser.h
    include/StreamingDomReader.h
    include/EmberDataTypes.h
    include/TreeViewController.h
    include/SubscriptionManager.h
    include/MatrixManager.h
    include/CrosspointActivityTracker.h
    include/SnapshotManager.h
    include/FunctionInvoker.h
    include/TreeFetchService.h
    include/CacheManager.h
    include/EmberConnection.h
    include/EmberProvider.h
)
add_dependencies(EmberViewerLib update_version)
file(GLOB_RECURSE SOURCE_FILES
    src/*.cpp
    src/*.h
    include/*.h
)
list(REMOVE_ITEM SOURCE_FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MatrixModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/VirtualizedMatrixWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/VirtualizedHeaderView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/VirtualizedSidebarView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MeterWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TriggerWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SliderWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GraphWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ParameterDelegate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DeviceSnapshot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/UpdateManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/UpdateDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EmberTreeWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/S101Protocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GlowParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StreamingDomReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TreeViewController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SubscriptionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MatrixManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CrosspointActivityTracker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SnapshotManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FunctionInvoker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TreeFetchService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CacheManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EmberConnection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EmberProvider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LinuxUpdateManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WindowsUpdateManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MacUpdateManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/MatrixModel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/VirtualizedMatrixWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/VirtualizedHeaderView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/VirtualizedSidebarView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/MeterWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/TriggerWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/SliderWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/GraphWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ParameterDelegate.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/DeviceSnapshot.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/UpdateManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/UpdateDialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EmberTreeWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/S101Protocol.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/GlowParser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/StreamingDomReader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EmberDataTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/TreeViewController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/SubscriptionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/MatrixManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/CrosspointActivityTracker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/SnapshotManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/FunctionInvoker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/TreeFetchService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/CacheManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EmberConnection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EmberProvider.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/LinuxUpdateManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/WindowsUpdateManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/MacUpdateManager.h
)
target_compile_features(EmberViewerLib
    PRIVATE
        cxx_std_17
)
set_target_properties(EmberViewerLib
    PROPERTIES
        AUTOMOC ON
        POSITION_INDEPENDENT_CODE ON
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(EmberViewerLib PRIVATE -fPIE)
    if(NOT CMAKE_BUILD_TYPE MATCHES "Debug")
        target_compile_options(EmberViewerLib PRIVATE -march=native)
    endif()
endif()
target_include_directories(EmberViewerLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(EmberViewerLib
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        libs101::s101
        ${LIBEMBER_TARGET}
)
target_precompile_headers(EmberViewerLib
    PRIVATE
        <QtCore/QtCore>
        <QtGui/QtGui>
        <QtWidgets/QtWidgets>
        <QtNetwork/QtNetwork>
        <memory>
        <vector>
        <string>
)
add_executable(${PROJECT_NAME}
    ${SOURCE_FILES}
    ${UI_FILES}
    ${RESOURCE_FILES}
)
add_dependencies(${PROJECT_NAME} update_version)
target_compile_features(${PROJECT_NAME}
    PRIVATE
        cxx_std_17
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
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        EmberViewerLib
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        libs101::s101
        ${LIBEMBER_TARGET}
)
target_precompile_headers(${PROJECT_NAME} REUSE_FROM EmberViewerLib)
if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE resources/app.rc)
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /I${CMAKE_CURRENT_SOURCE_DIR}/resources")
endif()
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.magnusoverli.emberviewer"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    )
endif()
if(NOT CMAKE_BUILD_TYPE MATCHES "Debug")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${PROJECT_NAME} PRIVATE
            -O3              
            -march=native    
            -DNDEBUG         
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${PROJECT_NAME} PRIVATE
            /O2              
            /GL              
            /DNDEBUG         
        )
    endif()
endif()
if (NOT CMAKE_BUILD_TYPE MATCHES "Debug")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported)
    if(ipo_supported)
        set_target_properties(EmberViewerLib PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
        set_target_properties(${PROJECT_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
        message(STATUS "Link-Time Optimization (LTO) enabled for EmberViewer")
    else()
        message(STATUS "Link-Time Optimization (LTO) not supported by compiler")
    endif()
endif()
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${WINDEPLOYQT_EXECUTABLE} $<TARGET_FILE:${PROJECT_NAME}>
        )
    endif()
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.ico
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
if(UNIX AND NOT APPLE)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/EmberViewer.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
    )
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/512x512/apps
        RENAME emberviewer.png
    )
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pixmaps
        RENAME emberviewer.png
    )
endif()
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS AND BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
    message(STATUS "EmberViewer tests configured")
endif()
