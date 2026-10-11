cmake_minimum_required(VERSION 3.21)

project(pawnet C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "PawNet currently only builds on Linux.")
endif()

# The CEF binary distribution (https://cef-builds.spotifycdn.com), unpacked.
#   cmake -S . -B build -DCEF_ROOT=/path/to/cef_binary_...
if(NOT CEF_ROOT)
  message(FATAL_ERROR "Set CEF_ROOT to the unpacked CEF binary distribution.")
endif()

# The CEF sandbox needs a SUID helper, so it is off unless requested. This must
# be declared before find_package(CEF), which otherwise defaults it to ON.
option(USE_SANDBOX "Enable the CEF sandbox (needs chrome-sandbox set SUID)" OFF)

list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)

# Builds the C++ wrapper around the CEF C API.
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

set(PAWNET_SRCS
  src/main.cc
  src/pawnet_app.cc
  src/pawnet_app.h
  src/browser_window.cc
  src/browser_window.h
  src/pawnet_client.cc
  src/pawnet_client.h
  src/home_page.cc
  src/home_page.h
  src/settings.cc
  src/settings.h
  )

SET_CEF_TARGET_OUT_DIR()

add_executable(pawnet ${PAWNET_SRCS})
SET_EXECUTABLE_TARGET_PROPERTIES(pawnet)
target_include_directories(pawnet PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CEF_ROOT})
add_dependencies(pawnet libcef_dll_wrapper)
ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")
target_link_libraries(pawnet libcef_lib libcef_dll_wrapper ${CEF_STANDARD_LIBS})

# Keep CEF's shared libraries and resources next to the executable.
set_target_properties(pawnet PROPERTIES
  INSTALL_RPATH "$ORIGIN"
  BUILD_WITH_INSTALL_RPATH TRUE
  RUNTIME_OUTPUT_DIRECTORY ${CEF_TARGET_OUT_DIR}
  )
COPY_FILES("pawnet" "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${CEF_TARGET_OUT_DIR}")
COPY_FILES("pawnet" "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${CEF_TARGET_OUT_DIR}")
if(EXISTS "${CEF_BINARY_DIR}/libminigbm.so")
  COPY_FILES("pawnet" "libminigbm.so" "${CEF_BINARY_DIR}" "${CEF_TARGET_OUT_DIR}")
endif()
# The window icon is loaded from next to the executable at runtime.
add_custom_command(TARGET pawnet POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/assets/pawnet-256.png"
          "${CEF_TARGET_OUT_DIR}/pawnet-256.png"
  VERBATIM)
if(USE_SANDBOX)
  SET_LINUX_SUID_PERMISSIONS("pawnet" "${CEF_TARGET_OUT_DIR}/chrome-sandbox")
endif()
