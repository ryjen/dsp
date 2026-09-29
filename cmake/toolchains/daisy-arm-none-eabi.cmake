include_guard(GLOBAL)

if(NOT DEFINED LIBDAISY_DIR)
    message(FATAL_ERROR "LIBDAISY_DIR must point to the pinned libDaisy source")
endif()
if(NOT DEFINED DAISY_ARM_GCC)
    message(FATAL_ERROR "DAISY_ARM_GCC must point to arm-none-eabi-gcc")
endif()

set(LIBDAISY_DIR "${LIBDAISY_DIR}" CACHE PATH "Pinned libDaisy source")
set(DAISY_ARM_GCC "${DAISY_ARM_GCC}" CACHE FILEPATH "ARM GCC compiler")
set(CMAKE_C_COMPILER "${DAISY_ARM_GCC}" CACHE FILEPATH "ARM C compiler")

list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
    LIBDAISY_DIR
    DAISY_ARM_GCC
)

include("${LIBDAISY_DIR}/cmake/toolchains/ArmGNUToolchain.cmake")
