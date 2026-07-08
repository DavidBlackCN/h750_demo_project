cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME adc_fft_demo)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# CMSIS-DSP from the local STM32Cube H7 firmware package.
# CubeMX records the DSP selection in the .ioc file, but the CMake project does
# not link CMSIS-DSP automatically, so wire the downloaded package in here.
set(STM32CUBE_REPOSITORY "$ENV{USERPROFILE}/STM32Cube/Repository" CACHE PATH "STM32Cube firmware repository")
set(STM32CUBE_FW_H7_PATH "${STM32CUBE_REPOSITORY}/STM32Cube_FW_H7_V1.12.1" CACHE PATH "STM32CubeH7 firmware package")
set(CMSIS_DSP_PATH "${STM32CUBE_FW_H7_PATH}/Drivers/CMSIS/DSP" CACHE PATH "CMSIS-DSP package path")
set(CMSIS_DSP_LIB "${CMSIS_DSP_PATH}/Lib/GCC/libarm_cortexM7lfsp_math.a" CACHE FILEPATH "CMSIS-DSP GCC library for Cortex-M7 hard-float single precision")

if(NOT EXISTS "${CMSIS_DSP_PATH}/Include/arm_math.h")
    message(FATAL_ERROR "CMSIS-DSP header not found: ${CMSIS_DSP_PATH}/Include/arm_math.h")
endif()

if(NOT EXISTS "${CMSIS_DSP_LIB}")
    message(FATAL_ERROR "CMSIS-DSP library not found: ${CMSIS_DSP_LIB}")
endif()

add_library(CMSIS_DSP STATIC IMPORTED)
set_target_properties(CMSIS_DSP PROPERTIES
    IMPORTED_LOCATION "${CMSIS_DSP_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${CMSIS_DSP_PATH}/Include"
    INTERFACE_COMPILE_DEFINITIONS "ARM_MATH_CM7;ARM_MATH_LOOPUNROLL"
)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    API/ADC_API.c
    API/FFT_API.c
    BLL/ADC_BLL.c
    BLL/FFT_BLL.c
    BLL/USART_BLL.c
    HDL/AD9833.c
    HDL/AD9910.c
    HDL/DAC8830.c
    HDL/DAC8830_SPI.c
    HDL/MySPI.c
    FML/ADC_FML.c
    FML/DAC_FML.c
    FML/FFT_FML.c
    FML/USART_FML.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    API
    BLL
    FML
    HDL
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -u _printf_float
    -Wl,-Map=${PROJECT_NAME}.map    # 生成.map文件
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    CMSIS_DSP

    # Add user defined libraries
)
