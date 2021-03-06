# IF(APPLE)
# MESSAGE(WARNING
# “Mac is not supported with MKLDNN in MAGMADNN yet.”
# “Force WITH_MKLDNN=OFF”)
# SET(WITH_MKLDNN OFF CACHE STRING “Disable MKLDNN in MacOS” FORCE)
# return()
# ENDIF()

SET(MKLDNN_SOURCES_DIR "third_party/mkldnn")
set(MKLDNN_INSTALL_DIR "${CMAKE_BINARY_DIR}/third_party/mkldnn/build")
set(MKLDNN_INCLUDE_DIRS "${MKLDNN_INSTALL_DIR}/include" CACHE PATH "MKLDNN include directory." FORCE)

include(GNUInstallDirs)
set(LIBDIR "lib")
if(CMAKE_INSTALL_LIBDIR MATCHES ".*lib64$")
  set(LIBDIR "lib64")
endif()

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_RPATH}" "${MKLDNN_INSTALL_DIR}/${LIBDIR}")

IF(NOT WIN32)
  set(MKLDNN_FLAG "-Wno-error=strict-overflow -Wno-error=unused-result -Wno-error=array-bounds")
  set(MKLDNN_FLAG "${MKLDNN_FLAG} -Wno-unused-result -Wno-unused-value")
  set(MKLDNN_CFLAG "${CMAKE_C_FLAGS} ${MKLDNN_FLAG}")
  set(MKLDNN_CXXFLAG "${CMAKE_CXX_FLAGS} ${MKLDNN_FLAG}")
ELSE()
  set(MKLDNN_CXXFLAG "${CMAKE_CXX_FLAGS} /EHsc")
ENDIF(NOT WIN32)

ExternalProject_Add(
  mkldnn
  GIT_REPOSITORY "https://github.com/intel/mkl-dnn.git"
  GIT_TAG "v1.2.1"  
  PREFIX ${MKLDNN_SOURCES_DIR}
  BINARY_DIR ${MKLDNN_INSTALL_DIR}
  UPDATE_COMMAND ""
  # INSTALL_COMMAND ""
  CMAKE_ARGS -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
  CMAKE_ARGS -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
  CMAKE_ARGS -DCMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}
  CMAKE_ARGS -DCMAKE_CXX_FLAGS_DEBUG=${CMAKE_CXX_FLAGS_DEBUG}
  CMAKE_ARGS -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
  CMAKE_ARGS -DCMAKE_C_FLAGS_DEBUG=${CMAKE_C_FLAGS_DEBUG}
  CMAKE_ARGS -DCMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${MKLDNN_INSTALL_DIR}
  CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  CMAKE_ARGS -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  # CMAKE_ARGS -DMKLROOT=${MKL_ROOT}
  CMAKE_ARGS -DCMAKE_C_FLAGS=${MKLDNN_CFLAG}
  CMAKE_ARGS -DCMAKE_CXX_FLAGS=${MKLDNN_CXXFLAG}
  CMAKE_ARGS -DWITH_TEST=OFF -DWITH_EXAMPLE=OFF
  CMAKE_CACHE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${MKLDNN_INSTALL_DIR}
  # -DMKLROOT:PATH=${MKL_ROOT}
  CMAKE_ARGS -DDNNL_CPU_RUNTIME=SEQ
  CMAKE_ARGS -DDNNL_ENABLE_CONCURRENT_EXEC=ON
  )

if(WIN32)
  set(MKLDNN_LIB "${MKLDNN_INSTALL_DIR}/${LIBDIR}/mkldnn.lib" CACHE FILEPATH "MKLDNN library." FORCE)
else()
  set(MKLDNN_LIB "${MKLDNN_INSTALL_DIR}/${LIBDIR}/libmkldnn.so" CACHE FILEPATH "MKLDNN library." FORCE)
endif()

set(LIBS ${LIBS} ${MKLDNN_LIB})
