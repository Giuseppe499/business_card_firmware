# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
set(PICO_SDK_PATH ${CMAKE_SOURCE_DIR}/pico-sdk)
include(pico_sdk_import.cmake)
# We also need PICO EXTRAS
set(PICO_EXTRAS_PATH ${CMAKE_SOURCE_DIR}/pico-extras)
include(${PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)

project(main C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add all .cpp files in the root of the project as sources
file(GLOB SOURCES "*.cpp")

# Add executable. Default name is the project name, version 0.1
add_executable(main
    ${SOURCES}
    )

# Add fpm include directory
target_include_directories(main PRIVATE ${CMAKE_SOURCE_DIR}/fpm/include)

# pull in common dependencies
target_link_libraries(main
    pico_stdlib
    pico_bootsel_via_double_reset
    pico_audio_i2s
)

if (PICO_CYW43_SUPPORTED)
    target_link_libraries(main pico_cyw43_arch_none)
endif()

pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 0)

target_compile_definitions(main PRIVATE
            # compile time configuration of I2S
            PICO_AUDIO_I2S_MONO_INPUT=1
            #define for our example code
            USE_AUDIO_I2S=1
            PICO_AUDIO_I2S_DATA_PIN=6
            PICO_AUDIO_I2S_CLOCK_PIN_BASE=14
#            PICO_DEFAULT_UART=0
#            PICO_DEFAULT_UART_TX_PIN=28
#            PICO_DEFAULT_UART_RX_PIN=29
            )

# Enable C++20 features
target_compile_features(main PRIVATE cxx_std_20)

# create map/bin/hex file etc.
pico_add_extra_outputs(main)
