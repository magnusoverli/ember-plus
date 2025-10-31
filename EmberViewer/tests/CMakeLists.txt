# EmberViewer Unit Tests

find_package(Qt6 COMPONENTS Test REQUIRED)

# Enable testing
enable_testing()

# Helper function to create tests
function(add_emberviewer_test test_name)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})
    add_executable(${test_name} ${test_name}.cpp ${ARG_SOURCES})
    
    target_include_directories(${test_name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    
    target_link_libraries(${test_name}
        PRIVATE
            EmberViewerLib
            Qt6::Test
            libs101::s101
            # libformula::formula    # Not used by EmberViewer - commented out
            ${LIBEMBER_TARGET}
    )
    
    target_compile_features(${test_name}
        PRIVATE
            cxx_std_17
    )
    
    set_target_properties(${test_name}
        PROPERTIES
            AUTOMOC ON
    )
    
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

# Create tests
add_emberviewer_test(test_constants)
add_emberviewer_test(test_path_parsing)
add_emberviewer_test(test_matrix_widget)
add_emberviewer_test(test_parameter_delegate)
add_emberviewer_test(test_s101_protocol)

# Link widget tests against the library
target_link_libraries(test_matrix_widget PRIVATE EmberViewerLib)
target_link_libraries(test_parameter_delegate PRIVATE EmberViewerLib)

# Optional: Add coverage target
option(ENABLE_COVERAGE "Enable code coverage" OFF)
if(ENABLE_COVERAGE)
    target_compile_options(test_constants PRIVATE --coverage)
    target_link_libraries(test_constants PRIVATE --coverage)
endif()

message(STATUS "EmberViewer tests configured")
