add_library(cppy2 cppy2.cpp cppy2numpy)
set(NUMPY_LIB "${NUMPY_INCLUDE_DIR}/../lib/libnpymath.a")

target_link_libraries(cppy2 ${PYTHON_LIBRARY} ${NUMPY_LIB})

if (Qt5Core_FOUND)
 target_link_libraries(cppy2 Qt5::Core)
endif()
