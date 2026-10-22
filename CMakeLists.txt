cmake_minimum_required(VERSION 3.20)
set(PICO_BOARD adafruit_fruit_jam CACHE STRING "Adafruit Fruit Jam" FORCE)
set(PICO_PSRAM_CS_PIN 47 CACHE STRING "Fruit Jam PSRAM CS" FORCE)
set(PICO_PSRAM_SIZE_BYTES 0x800000 CACHE STRING "Fruit Jam PSRAM size" FORCE)
set(PICO_DEFAULT_UART 0 CACHE STRING "Diagnostic UART" FORCE)
set(PICO_DEFAULT_UART_TX_PIN 44 CACHE STRING "Diagnostic UART TX" FORCE)
set(PICO_DEFAULT_UART_RX_PIN 45 CACHE STRING "Diagnostic UART RX" FORCE)
if(NOT DEFINED PICO_SDK_PATH OR NOT DEFINED PICO_PIO_USB_PATH)
  message(FATAL_ERROR "Set PICO_SDK_PATH and PICO_PIO_USB_PATH")
endif()
set(ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(UMAC "${ROOT}/deps/umac")
set(CPU "${ROOT}/deps/Musashi")
set(SHARED "${ROOT}/deps/pico_shared")
set(HDMI "${SHARED}/drivers/pico_hdmi")
set(FATFS "${SHARED}/drivers/pico_fatfs")
include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
project(Macintosh68K C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()
set(SAVED_TINYUSB_TOP "${TOP}")
set(TOP "${PICO_SDK_PATH}/lib/tinyusb")
check_and_add_pico_pio_usb_support()
set(TOP "${SAVED_TINYUSB_TOP}")
add_subdirectory("${FATFS}" "${CMAKE_BINARY_DIR}/pico_fatfs")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${ROOT}/scripts/materialize-mac-runtime.py"
  --umac "${UMAC}" --out "${CMAKE_CURRENT_BINARY_DIR}/runtime"
  RESULT_VARIABLE patch_result)
if(NOT patch_result EQUAL 0)
  message(FATAL_ERROR "umac integration patches failed")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${ROOT}/scripts/materialize-mac-runtime.py")

add_library(mac_system_core STATIC
  "${ROOT}/runtime/jack_mac_audio.c"
  "${CMAKE_CURRENT_BINARY_DIR}/runtime/main.c"
  "${CMAKE_CURRENT_BINARY_DIR}/runtime/disc.c"
  "${UMAC}/src/via.c" "${UMAC}/src/scc.c" "${UMAC}/src/rom.c"
  "${CPU}/m68kcpu.c" "${CPU}/m68kops.c" "${CPU}/m68kdasm.c")
target_include_directories(mac_system_core PUBLIC "${UMAC}/include" "${CPU}" "${ROOT}/runtime")
target_compile_definitions(mac_system_core PUBLIC UMAC_MEMSIZE=2048 DISP_WIDTH=512 DISP_HEIGHT=342)
target_compile_definitions(mac_system_core PRIVATE PICO=1 BYTE_ORDER=1234 LITTLE_ENDIAN=1234
  MUSASHI_CNF="${UMAC}/include/m68kconf.h")
target_compile_options(mac_system_core PRIVATE -O3 -ffunction-sections -fdata-sections -Wno-unused-function)
target_link_libraries(mac_system_core PUBLIC pico_stdlib)

add_library(mac_system_hal STATIC
  "${ROOT}/platform/input/jack_input.c"
  "${ROOT}/platform/input/jack_input_devices.c"
  "${ROOT}/platform/text/jack_terminal.c"
  "${ROOT}/platform/psram/jack_memory.c")
target_include_directories(mac_system_hal PUBLIC "${ROOT}/platform/input" "${ROOT}/platform/psram" "${ROOT}/platform/text")
target_compile_options(mac_system_hal PRIVATE -O3 -Wall -Wextra -Werror -ffunction-sections -fdata-sections)
target_link_libraries(mac_system_hal PUBLIC pico_stdlib)

add_library(mac_hdmi STATIC "${HDMI}/hstx.c" "${HDMI}/video_output.c"
  "${HDMI}/hstx_data_island_queue.c" "${HDMI}/hstx_packet.c")
target_include_directories(mac_hdmi PUBLIC "${HDMI}")
target_compile_definitions(mac_hdmi PRIVATE GPIOHSTXD0=15 GPIOHSTXD1=17 GPIOHSTXD2=19
  GPIOHSTXCK=13 GPIOHSTXINVERTED=1 JACK_HSTX_126MHZ=1)
target_compile_options(mac_hdmi PRIVATE -O3 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter)
target_link_libraries(mac_hdmi PUBLIC pico_stdlib pico_multicore hardware_dma hardware_sync
  hardware_clocks hardware_vreg hardware_pio hardware_spi hardware_i2c hardware_pwm hardware_adc)

add_executable(picoMac68kSystem "${ROOT}/firmware/mac_system.c"
  "${ROOT}/platform/input/jack_tinyusb_input.c"
  "${ROOT}/platform/input/jack_nespad_input.c")
pico_set_program_name(picoMac68kSystem "picoMac68kSystem")
pico_set_program_version(picoMac68kSystem "0.3.0-fruitjam-src")
pico_enable_stdio_uart(picoMac68kSystem 1)
pico_enable_stdio_usb(picoMac68kSystem 0)
target_include_directories(picoMac68kSystem PRIVATE "${ROOT}/platform/input" "${ROOT}/platform/psram"
  "${FATFS}/fatfs" "${SHARED}" "${HDMI}")
target_compile_definitions(picoMac68kSystem PRIVATE HW_CONFIG=8 PICO_STACK_SIZE=4096
  JACK_TINYUSB_KEYBOARD_RECOVERY=1 PICO_PIO_USE_GPIO_BASE=1 CFG_TUH_RPI_PIO_USB=1
  PIO_USB_USE_PIO=2 PICO_DEFAULT_PIO_USB_DP_PIN=1 SDCARD_PIN_CS=39
  SDCARD_PIN_SCK=34 SDCARD_PIN_MOSI=35 SDCARD_PIN_MISO=36
  SDCARD_SPI=spi0 SDCARD_PIO=pio0)
target_compile_options(picoMac68kSystem PRIVATE -O3 -Wall -Wextra -Werror
  -Wno-unused-parameter -Wno-cast-function-type -Wno-maybe-uninitialized
  -Wno-attributes -Wno-sign-compare)
target_link_libraries(picoMac68kSystem PRIVATE mac_system_core mac_system_hal mac_hdmi pico_fatfs
  pico_stdlib pico_multicore hardware_dma hardware_pio hardware_spi hardware_clocks
  hardware_vreg hardware_psram hardware_xip_cache tinyusb_host tinyusb_board tinyusb_pico_pio_usb m)
target_link_options(picoMac68kSystem PRIVATE -Wl,--gc-sections -Wl,--print-memory-usage)
include("${ROOT}/cmake/LoaderPartition.cmake")
mac_link_for_loader(picoMac68kSystem)
pico_add_extra_outputs(picoMac68kSystem)
