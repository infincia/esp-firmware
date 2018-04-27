cmake_minimum_required(VERSION 3.10)
project(firmware)

set(IDF_PATH "/Volumes/SourceCache/Frameworks/esp-idf/")
set(CMAKE_SYSTEM_NAME xtensa-esp32-elf)
set(CMAKE_C_COMPILER /Volumes/SourceCache/Toolchains/xtensa-esp32-elf/bin/xtensa-esp32-elf-gcc)
set(CMAKE_FIND_ROOT_PATH /Volumes/SourceCache/Toolchains/xtensa-esp32-elf/)


set(CMAKE_CXX_STANDARD 11)

file(GLOB_RECURSE SOURCES "${IDF_PATH}/*.c")

set(COMPONENTS
        app_update
        aws_iot
        bootloader
        bootloader_support
        bt
        coap
        console
        cxx
        driver
        esp32
        esptool_py
        esp-tls
        ethernet
        expat
        fatfs
        freertos
        heap
        idf_test
        json
        log
        lwip
        mbedtls
        mdns
        micro-ecc
        newlib
        nghttp
        nvs_flash
        openssl
        partition_table
        sdmmc
        soc
        spi_flash
        spiffs
        tcpip_adapter
        ulp
        vfs
        wpa_supplicant
        xtensa-debug-module)


set (INCLUDE_DIRS "")
foreach (component ${COMPONENTS})
    list (APPEND INCLUDE_DIRS ${IDF_PATH}/components/${component}/include)
    list (APPEND INCLUDE_DIRS ${IDF_PATH}/components/${component}/src)

    list (APPEND INCLUDE_DIRS ${IDF_PATH}/components/${component}/src/include)
endforeach()

list (APPEND INCLUDE_DIRS ${IDF_PATH}/components/console)
list (APPEND INCLUDE_DIRS ${IDF_PATH}/components/soc/esp32/include)
list (APPEND INCLUDE_DIRS ${IDF_PATH}/components/lwip/include/lwip)
list (APPEND INCLUDE_DIRS ${IDF_PATH}/components/esp-tls)
list (APPEND INCLUDE_DIRS ${IDF_PATH}/components/nghttp/port/include)

list (APPEND INCLUDE_DIRS ${IDF_PATH}/components/aws_iot/aws-iot-device-sdk-embedded-C/include)

list(REMOVE_DUPLICATES INCLUDE_DIRS)

include_directories(${INCLUDE_DIRS})

include_directories(build/include)

include_directories(components/esp-si7021/include)
include_directories(components/esp32-snippets/c-utils)
include_directories(components/esp32-snippets/cpp_utils)
include_directories(components/https_client_cpp)
include_directories(components/MAX9744)
include_directories(components/sha256_cpp)
include_directories(components/tarablessd1306)
include_directories(main)
include_directories(main/aws)

add_executable(firmware
        components/esp-si7021/include/si7021.h
        components/esp-si7021/src/si7021.c

        components/esp32-snippets/cpp_utils/AWS.cpp
        components/esp32-snippets/cpp_utils/AWS.h
        components/esp32-snippets/cpp_utils/FATFS_VFS.cpp
        components/esp32-snippets/cpp_utils/FATFS_VFS.h
        components/esp32-snippets/cpp_utils/File.cpp
        components/esp32-snippets/cpp_utils/File.h
        components/esp32-snippets/cpp_utils/FileSystem.cpp
        components/esp32-snippets/cpp_utils/FileSystem.h
        components/esp32-snippets/cpp_utils/FreeRTOS.cpp
        components/esp32-snippets/cpp_utils/FreeRTOS.h
        components/esp32-snippets/cpp_utils/FreeRTOSTimer.cpp
        components/esp32-snippets/cpp_utils/FreeRTOSTimer.h
        components/esp32-snippets/cpp_utils/GeneralUtils.cpp
        components/esp32-snippets/cpp_utils/GeneralUtils.h
        components/esp32-snippets/cpp_utils/GPIO.cpp
        components/esp32-snippets/cpp_utils/GPIO.h
        components/esp32-snippets/cpp_utils/HttpParser.cpp
        components/esp32-snippets/cpp_utils/HttpParser.h
        components/esp32-snippets/cpp_utils/HttpRequest.cpp
        components/esp32-snippets/cpp_utils/HttpRequest.h
        components/esp32-snippets/cpp_utils/HttpResponse.cpp
        components/esp32-snippets/cpp_utils/HttpResponse.h
        components/esp32-snippets/cpp_utils/HttpServer.cpp
        components/esp32-snippets/cpp_utils/HttpServer.h
        components/esp32-snippets/cpp_utils/JSON.cpp
        components/esp32-snippets/cpp_utils/JSON.h
        components/esp32-snippets/cpp_utils/Memory.cpp
        components/esp32-snippets/cpp_utils/Memory.h
        components/esp32-snippets/cpp_utils/Socket.cpp
        components/esp32-snippets/cpp_utils/Socket.h
        components/esp32-snippets/cpp_utils/SockServ.cpp
        components/esp32-snippets/cpp_utils/SockServ.h
        components/esp32-snippets/cpp_utils/SSLUtils.cpp
        components/esp32-snippets/cpp_utils/SSLUtils.h
        components/esp32-snippets/cpp_utils/System.cpp
        components/esp32-snippets/cpp_utils/System.h
        components/esp32-snippets/cpp_utils/Task.cpp
        components/esp32-snippets/cpp_utils/Task.h
        components/esp32-snippets/cpp_utils/WebSocket.cpp
        components/esp32-snippets/cpp_utils/WebSocket.h
        components/esp32-snippets/cpp_utils/WebSocketFileTransfer.cpp
        components/esp32-snippets/cpp_utils/WebSocketFileTransfer.h
        components/esp32-snippets/posix/posix_shims.c
        components/https_client_cpp/HTTPSClient.cpp
        components/https_client_cpp/HTTPSClient.hpp
        components/MAX9744/MAX9744.cpp
        components/MAX9744/MAX9744.hpp
        components/sha256_cpp/SHA256.hpp
        components/tarablessd1306/fonts/font_droid_sans_fallback_11x12.c
        components/tarablessd1306/fonts/font_droid_sans_fallback_16x17.c
        components/tarablessd1306/fonts/font_droid_sans_fallback_25x25.c
        components/tarablessd1306/fonts/font_droid_sans_mono_13x21.c
        components/tarablessd1306/fonts/font_droid_sans_mono_17x30.c
        components/tarablessd1306/fonts/font_droid_sans_mono_7x12.c
        components/tarablessd1306/fonts/font_liberation_mono_13x20.c
        components/tarablessd1306/fonts/font_liberation_mono_17x27.c
        components/tarablessd1306/fonts/font_liberation_mono_8x11.c
        components/tarablessd1306/ifaces/default_if_i2c.c
        components/tarablessd1306/ifaces/default_if_spi.c
        components/tarablessd1306/ssd1306.c
        components/tarablessd1306/ssd1306.h
        components/tarablessd1306/ssd1306_default_if.h
        components/tarablessd1306/ssd1306_draw.c
        components/tarablessd1306/ssd1306_draw.h
        components/tarablessd1306/ssd1306_err.h
        components/tarablessd1306/ssd1306_font.c
        components/tarablessd1306/ssd1306_font.h
        main/aws/aws.cpp
        main/aws/aws.hpp
        main/amp.cpp
        main/amp.hpp
        main/Audiowide_47x60.c
        main/cmd_decl.hpp
        main/cmd_system.cpp
        main/cmd_volume.cpp
        main/console.cpp
        main/console.hpp
        main/constants.h
        main/CuteFont_47x80.c
        main/font.h
        main/globals.cpp
        main/globals.hpp
        main/ipc_command.h
        main/ir.cpp
        main/ir.hpp
        main/main.cpp
        main/oled.cpp
        main/oled.hpp
        main/pch.hpp
        main/Roboto_42x57.c
        main/Roboto_black_54x72.c
        main/sdkconfig.h
        main/temperature.cpp
        main/temperature.hpp
        main/update.cpp
        main/update.hpp
        main/util.cpp
        main/util.hpp
        main/web.cpp
        main/web.hpp
        main/websocket.cpp
        main/websocket.hpp)
