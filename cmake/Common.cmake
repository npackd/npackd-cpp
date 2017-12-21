# compiler warnings
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX OR CMAKE_COMPILER_IS_GNUCC)
  set(NPACKD_WARNING_FLAGS "-Wall -Winit-self -Wwrite-strings -Wextra -Wno-long-long -Wno-overloaded-virtual -Wno-missing-field-initializers -Wno-unused-parameter -Wno-unknown-pragmas")
  if(EMSCRIPTEN)
    set(NPACKD_WARNING_FLAGS "${NPACKD_WARNING_FLAGS} -Wno-warn-absolute-paths")
  elseif(NOT APPLE)
    set(NPACKD_WARNING_FLAGS "${NPACKD_WARNING_FLAGS} -Wno-unused-but-set-parameter")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${NPACKD_WARNING_FLAGS}")
endif()

