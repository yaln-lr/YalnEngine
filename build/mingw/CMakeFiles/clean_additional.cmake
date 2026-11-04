# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "viewer\\CMakeFiles\\yalnviewer_autogen.dir\\AutogenUsed.txt"
  "viewer\\CMakeFiles\\yalnviewer_autogen.dir\\ParseCache.txt"
  "viewer\\yalnviewer_autogen"
  )
endif()
