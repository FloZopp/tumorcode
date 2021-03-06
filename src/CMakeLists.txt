cmake_minimum_required(VERSION 2.8.8)
# we need quite a bit of python information for compiling the source
# declare that here, so that is it is recursive valid
# option(PAGMO_MPI "is pagmo mpi?" OFF)
if(USE_PAGMO_MPI)
message("we will use pagmo with mpi support!")
  ADD_DEFINITIONS(-DPAGMO_ENABLE_MPI)
  find_package(MPI)
  include_directories(${MPI_CXX_INCLUDE_PATH})
  set(myLibs "${myLibs};${MPI_CXX_LIBRARIES}")
endif()
set(CMAKE_DEBUG_POSTFIX "d")

if(USE_ADAPTION)
  add_subdirectory(adaption)
endif()

add_subdirectory(detailedO2)
add_subdirectory(common)
add_subdirectory(python_krebsutils)
if(BUILD_TESTS)
  add_subdirectory(tests)
endif()
add_subdirectory(mwlib)
add_subdirectory(mwlib_tests)



add_subdirectory(iff)
add_subdirectory(tumors)

# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
  "${PROJECT_SOURCE_DIR}/src/tumorcode_build_info.h.in"
  "${PROJECT_BINARY_DIR}/include/tumorcode_build_info.h"
  )
