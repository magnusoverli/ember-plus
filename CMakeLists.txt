cmake_minimum_required(VERSION 3.20 FATAL_ERROR)
project(ember-plus VERSION 1.8.3)

add_subdirectory(libs101)
# add_subdirectory(libformula)      # Not used by EmberViewer - commented out to reduce build time and warnings

add_subdirectory(libember)
# add_subdirectory(libember_slim)   # Not used by EmberViewer - commented out to reduce build time and warnings

# add_subdirectory(tinyember)       # Example applications only - commented out to reduce build time and warnings
add_subdirectory(EmberViewer)
