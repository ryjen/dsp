include_guard(GLOBAL)

if(NOT DEFINED LIBDAISY_DIR)
    message(FATAL_ERROR "LIBDAISY_DIR must point to the pinned libDaisy source")
endif()

include("${LIBDAISY_DIR}/cmake/toolchains/ArmGNUToolchain.cmake")
