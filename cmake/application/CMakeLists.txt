cmake_minimum_required(VERSION 3.20.0)

foreach(required BLITZMAX_SDK BLITZMAX_APPLICATION_ROOT BLITZMAX_GENERATED_C BLITZMAX_OUTPUT_NAME)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "${required} must be supplied by bmk")
  endif()
endforeach()

if(DEFINED ENV{BMX_NRF52_EXTRA_CONF_FILE})
  set(EXTRA_CONF_FILE "$ENV{BMX_NRF52_EXTRA_CONF_FILE}")
endif()
if(DEFINED ENV{BMX_NRF52_EXTRA_OVERLAY_FILE})
  set(EXTRA_DTC_OVERLAY_FILE "$ENV{BMX_NRF52_EXTRA_OVERLAY_FILE}")
endif()
if("__bb_nrf52_network_ble_ble" IN_LIST BLITZMAX_MODULE_INITIALIZERS)
  if(DEFINED EXTRA_CONF_FILE)
    list(APPEND EXTRA_CONF_FILE "${CMAKE_CURRENT_LIST_DIR}/ble.conf")
  else()
    set(EXTRA_CONF_FILE "${CMAKE_CURRENT_LIST_DIR}/ble.conf")
  endif()
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(blitzmax_nrf52_application C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(EMBEDDED_ROOT "${BLITZMAX_SDK}/mod/embedded.mod")
set(NRF52_ROOT "${BLITZMAX_SDK}/mod/nrf52.mod")
set(BRL_BLITZ_ROOT "${BLITZMAX_SDK}/mod/brl.mod/blitz.mod")
set(BOOTSTRAP_C "${CMAKE_CURRENT_BINARY_DIR}/blitzmax_nrf52_modules.c")

file(WRITE "${BOOTSTRAP_C}" "/* Generated module initialization order. */\n")
foreach(initializer IN LISTS BLITZMAX_MODULE_INITIALIZERS)
  file(APPEND "${BOOTSTRAP_C}" "extern int ${initializer}(void);\n")
endforeach()
file(APPEND "${BOOTSTRAP_C}" "extern void bmx_nrf52_managed_init(void);\nvoid bmx_embedded_modules_init(void) {\n    bmx_nrf52_managed_init();\n")
foreach(initializer IN LISTS BLITZMAX_MODULE_INITIALIZERS)
  file(APPEND "${BOOTSTRAP_C}" "    (void)${initializer}();\n")
endforeach()
file(APPEND "${BOOTSTRAP_C}" "}\n")

if(DEFINED BLITZMAX_NATIVE_CMAKE)
  include("${BLITZMAX_NATIVE_CMAKE}")
endif()

target_sources(app PRIVATE
  ${BLITZMAX_GENERATED_C}
  ${BLITZMAX_MODULE_SOURCES}
  ${BLITZMAX_PICO_NATIVE_SOURCES}
  "${BOOTSTRAP_C}"
  "${EMBEDDED_ROOT}/runtime.mod/native/embedded_runtime.c"
  "${EMBEDDED_ROOT}/runtime.mod/native/embedded_string_integer.c"
  "${EMBEDDED_ROOT}/runtime.mod/native/embedded_string_float.cpp"
  "${EMBEDDED_ROOT}/runtime.mod/native/embedded_unicode.c"
  "${NRF52_ROOT}/runtime.mod/native/nrf52_runtime.c"
  "${NRF52_ROOT}/runtime.mod/native/nrf52_device.c"
  "${NRF52_ROOT}/runtime.mod/native/nrf52_watchdog.c"
  "${NRF52_ROOT}/runtime.mod/native/nrf52_random.c"
  "${NRF52_ROOT}/runtime.mod/native/nrf52_gpio.c"
  "${NRF52_ROOT}/runtime.mod/native/nrf52_events.c"
  "${NRF52_ROOT}/runtime.mod/native/nrf52_adc.c"
  "${NRF52_ROOT}/runtime.mod/native/nrf52_pwm.c"
  "${NRF52_ROOT}/runtime.mod/native/nrf52_spi.c"
  "${NRF52_ROOT}/runtime.mod/native/nrf52_uart.c"
  "${NRF52_ROOT}/runtime.mod/native/nrf52_uart_async.c"
  "${NRF52_ROOT}/runtime.mod/native/nrf52_i2c.c"
  "${NRF52_ROOT}/runtime.mod/native/nrf52_power.c"
  "${BRL_BLITZ_ROOT}/ryu/ryu/f2s.c"
  "${BRL_BLITZ_ROOT}/ryu/ryu/d2s.c"
)
target_compile_definitions(app PRIVATE RYU_OPTIMIZE_SIZE=1 BMX_EMBEDDED_ARENA_SIZE=65536u)
target_include_directories(app PRIVATE
  "${EMBEDDED_ROOT}/runtime.mod/native/include"
  "${NRF52_ROOT}/runtime.mod/native/include"
  "${BLITZMAX_SDK}/mod"
  "${BLITZMAX_APPLICATION_ROOT}"
  "${BRL_BLITZ_ROOT}"
  "${BRL_BLITZ_ROOT}/ryu"
)
if(BLITZMAX_PICO_NATIVE_LINK_OPTIONS)
  target_link_options(app PRIVATE ${BLITZMAX_PICO_NATIVE_LINK_OPTIONS})
endif()
