option(ENABLE_VIRTUALCAM "Build OBS Virtualcam" ON)

if(NOT ENABLE_VIRTUALCAM)
  message(STATUS "OBS:  DISABLED   mac-virtualcam")
  return()
endif()

add_subdirectory(src/obs-plugin)
add_subdirectory(src/dal-plugin)
