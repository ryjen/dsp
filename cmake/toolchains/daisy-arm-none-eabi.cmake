include_guard(GLOBAL)

if(NOT DEFINED LIBDAISY_DIR)
    message(FATAL_ERROR "LIBDAISY_DIR must point to the pinned libDaisy source")
endif()

set(LIBDAISY_DIR "${LIBDAISY_DIR}" CACHE PATH "Pinned libDaisy source")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES LIBDAISY_DIR)

include("${LIBDAISY_DIR}/cmake/toolchains/ArmGNUToolchain.cmake")
