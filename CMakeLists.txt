cmake_minimum_required(VERSION 3.0.0)
project(cppy2)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# find Qt
find_package(Qt5Core REQUIRED)
if (Qt5Core_FOUND)
 message(STATUS "Found Qt5 ${QT_VERSION_MAJOR}.${QT_VERSION_MINOR} at ${Qt5Core_INCLUDE_DIRS}")
 set(CMAKE_AUTOMOC ON)
endif()

# find Python 2.x
set(REQUIRED_PYTHON_VERSION "2.7") # tested on 2.7
message(STATUS "Recomended python version is ${REQUIRED_PYTHON_VERSION}")
find_package(PythonInterp ${REQUIRED_PYTHON_VERSION} REQUIRED)
find_package(PythonLibs ${PYTHON_VERSION_STRING} REQUIRED)
include_directories(${PYTHON_INCLUDE_DIRS})

# find numpy 
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(NumPy REQUIRED)
include_directories(${NUMPY_INCLUDE_DIR})


# make lib
add_subdirectory(cppy2)

# make tests
add_executable(tests tests/tests.cpp)
target_link_libraries(tests cppy2)
