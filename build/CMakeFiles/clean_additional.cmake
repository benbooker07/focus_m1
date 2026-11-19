# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "bootloader/bootloader.bin"
  "bootloader/bootloader.elf"
  "bootloader/bootloader.map"
  "config/sdkconfig.cmake"
  "config/sdkconfig.h"
  "esp-idf/esptool_py/flasher_args.json.in"
  "esp-idf/mbedtls/x509_crt_bundle"
  "flash_app_args"
  "flash_bootloader_args"
  "flash_project_args"
  "flasher_args.json"
  "focus_m1.bin"
  "focus_m1.map"
  "gray_1024.jpg.S"
  "gray_480.jpg.S"
  "gray_800.jpg.S"
  "ldgen_libraries"
  "ldgen_libraries.in"
  "project_elf_src_esp32s3.c"
  "test.jpg.S"
  "x509_crt_bundle.S"
  )
endif()
