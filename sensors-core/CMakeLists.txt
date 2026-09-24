cmake_minimum_required(VERSION 3.16)

project(Sensors VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Определение ОС
if(WIN32)
    set(TARGET_OS "windows")
    message(STATUS "Building for Windows")
elseif(ANDROID)
    set(TARGET_OS "android")
    message(STATUS "Building for Android")
elseif(IOS)
    set(TARGET_OS "ios")
    message(STATUS "Building for iOS")
elseif(APPLE)
    set(TARGET_OS "macos")
    message(STATUS "Building for macOS")
elseif(UNIX AND NOT APPLE)
    set(TARGET_OS "linux")
    message(STATUS "Building for Linux")
else()
    set(TARGET_OS "unknown")
    message(STATUS "Building for unknown platform")
endif()

# Обязательные компоненты Qt для всех платформ
find_package(Qt6 REQUIRED COMPONENTS Core HttpServer WebSockets Widgets WebEngineWidgets Concurrent Network Bluetooth)

qt_standard_project_setup(REQUIRES 6.5)

# Основные исходные файлы
set(COMMON_SOURCES
    main.cpp
    socketserver.cpp
    socketserver.h
    responsemanager.cpp
    responsemanager.h
    requestmanager.cpp
    requestmanager.h
    streammanager.cpp
    streammanager.h
    ireqhandler.h
    basereqhandler.cpp
    basereqhandler.h
    core.cpp
    core.h
    types.h
    device_generator.h
    simplewebserver.h
    simplewebserver.cpp
    demodeviceservice.h
    demodeviceservice.cpp
    iwcy_wrapper.h
    wcydeviceservice.h
    wcydeviceservice.cpp
    swr/deviceapihandler.h
    swr/deviceapihandler.cpp
    swrdeviceservice.h swrdeviceservice.cpp
    swr/QBluetoothBase.cpp
    swr/QBluetoothBase.h
    swr/qbluetoothdevicemanagement.cpp
    swr/qbluetoothdevicemanagement.h
    swr/QLowEnergyBluetooth.cpp
    swr/QLowEnergyBluetooth.h
    swr/QStandardBluetooth.cpp
    swr/QStandardBluetooth.h
    swr/dataloggerstructinfo.h
)

# Платформозависимые исходные файлы
if(WIN32)
    set(PLATFORM_SOURCES
        wcy_dislab_wrapper.h
        wcy_dislab_wrapper.cpp

    )
elseif(UNIX AND NOT APPLE AND NOT ANDROID)  # Linux
    set(PLATFORM_SOURCES
        wcy_wrapper_linux.h
        wcy_wrapper_linux.cpp
    )
endif()

list(APPEND COMMON_SOURCES ${PLATFORM_SOURCES})

qt_add_executable(appSensors ${COMMON_SOURCES})

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-DQT_NO_DEBUG_OUTPUT -DQT_NO_WARNING_OUTPUT)
endif()

# QML модуль
qt_add_qml_module(appSensors
    URI Sensors
    VERSION 1.0
    QML_FILES
        Main.qml
)

target_include_directories(appSensors PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/swr
)

# Настройки для разных платформ
if(APPLE)
    set_target_properties(appSensors PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appSensors
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
    )
elseif(WIN32)
    set_target_properties(appSensors PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# Настройка для Windows с WCY SDK
if(WIN32)
    # Путь к библиотеке (предполагаем, что она в папке lib)
    set(WCY_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib")

    # Проверяем наличие заголовочного файла
    if(EXISTS "${WCY_SDK_DIR}/WCY_DisLab_SDK.h")
        message(STATUS "Found WCY_DisLab_SDK.h at ${WCY_SDK_DIR}")

        # Добавляем include директорию
        target_include_directories(appSensors PRIVATE ${WCY_SDK_DIR})

        # Проверяем наличие библиотеки
        if(EXISTS "${WCY_SDK_DIR}/WCY_DisLab_SDK.lib" AND EXISTS "${WCY_SDK_DIR}/WCY_DisLab_SDK.dll")
            message(STATUS "Found WCY_DisLab_SDK library at ${WCY_SDK_DIR}")

            # Добавляем библиотеку
            add_library(WCY_DisLab_SDK SHARED IMPORTED)
            set_target_properties(WCY_DisLab_SDK PROPERTIES
                IMPORTED_LOCATION "${WCY_SDK_DIR}/WCY_DisLab_SDK.dll"
                IMPORTED_IMPLIB "${WCY_SDK_DIR}/WCY_DisLab_SDK.lib"
            )

            target_link_libraries(appSensors PRIVATE WCY_DisLab_SDK)

            # Копирование DLL при сборке
            add_custom_command(TARGET appSensors POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${WCY_SDK_DIR}/WCY_DisLab_SDK.dll"
                    "$<TARGET_FILE_DIR:appSensors>/WCY_DisLab_SDK.dll"
                COMMENT "Copying WCY_DisLab_SDK.dll to output directory"
            )

            target_compile_definitions(appSensors PRIVATE WITH_WCY_SDK=1)
            message(STATUS "WCY SDK configured for Windows")
        else()
            message(WARNING "WCY_DisLab_SDK library files not found in ${WCY_SDK_DIR}")
            target_compile_definitions(appSensors PRIVATE WITH_WCY_SDK=0)
        endif()
    else()
        message(WARNING "WCY_DisLab_SDK.h not found in ${WCY_SDK_DIR}")
        target_compile_definitions(appSensors PRIVATE WITH_WCY_SDK=0)
    endif()
endif()

# Настройка для Linux с WCY SDK
if(UNIX AND NOT APPLE AND NOT ANDROID)  # Linux
    # Путь к библиотеке (предполагаем, что она в папке lib)
    set(WCY_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib")

    # Проверяем наличие заголовочного файла
    if(EXISTS "${WCY_SDK_DIR}/WCY_DisLab_SDK.h")
        message(STATUS "Found WCY_DisLab_SDK.h at ${WCY_SDK_DIR}")

        # Добавляем include директорию
        target_include_directories(appSensors PRIVATE ${WCY_SDK_DIR})

        # Ищем библиотеку
        set(WCY_SDK_LIB_NAMES libwcy_dislab_sdk.so wcy_dislab_sdk.so)

        # Сначала проверяем в папке lib
        set(WCY_SDK_LIB_FOUND FALSE)
        foreach(LIB_NAME ${WCY_SDK_LIB_NAMES})
            if(EXISTS "${WCY_SDK_DIR}/${LIB_NAME}")
                set(WCY_SDK_LIB_PATH "${WCY_SDK_DIR}/${LIB_NAME}")
                set(WCY_SDK_LIB_FOUND TRUE)
                message(STATUS "Found WCY SDK library at ${WCY_SDK_LIB_PATH}")
                break()
            endif()
        endforeach()

        # Если не нашли в lib, ищем в системных путях
        if(NOT WCY_SDK_LIB_FOUND)
            find_library(WCY_SDK_LIB_PATH
                NAMES wcy_dislab_sdk libwcy_dislab_sdk
                PATHS /usr/local/lib /usr/lib
            )
            if(WCY_SDK_LIB_PATH)
                set(WCY_SDK_LIB_FOUND TRUE)
                message(STATUS "Found WCY SDK library in system: ${WCY_SDK_LIB_PATH}")
            endif()
        endif()

        if(WCY_SDK_LIB_FOUND)
            # Добавляем библиотеку как импортированную
            add_library(WCY_DisLab_Sdk_Linux SHARED IMPORTED)
            set_target_properties(WCY_DisLab_Sdk_Linux PROPERTIES
                IMPORTED_LOCATION "${WCY_SDK_LIB_PATH}"
            )

            target_link_libraries(appSensors PRIVATE WCY_DisLab_Sdk_Linux)
            target_compile_definitions(appSensors PRIVATE WITH_WCY_SDK=1)

            # Добавляем путь к библиотеке для runtime
            get_filename_component(WCY_SDK_LIB_DIR "${WCY_SDK_LIB_PATH}" DIRECTORY)
            if(WCY_SDK_LIB_DIR)
                target_link_options(appSensors PRIVATE
                    "-Wl,-rpath,${WCY_SDK_LIB_DIR}"
                )
            endif()

            message(STATUS "WCY SDK configured for Linux")
        else()
            message(WARNING "WCY SDK library not found")
            target_compile_definitions(appSensors PRIVATE WITH_WCY_SDK=0)
        endif()
    else()
        message(WARNING "WCY_DisLab_SDK.h not found in ${WCY_SDK_DIR}")
        target_compile_definitions(appSensors PRIVATE WITH_WCY_SDK=0)
    endif()
endif()

# Общие библиотеки для всех платформ
target_link_libraries(appSensors
    PRIVATE
        Qt6::HttpServer
        Qt6::Network
        Qt6::WebSockets
        Qt6::Widgets
        Qt6::WebEngineWidgets
        Qt6::Concurrent
        Qt6::Bluetooth
)

# Дополнительные библиотеки для Linux
if(UNIX AND NOT APPLE AND NOT ANDROID)
    target_link_libraries(appSensors PRIVATE dl)
endif()

# Инсталляция
include(GNUInstallDirs)
install(TARGETS appSensors
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Копирование конфигурационных файлов
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/config"
        DESTINATION .
    )
endif()

# Копирование библиотеки для Linux при установке
if(UNIX AND NOT APPLE AND NOT ANDROID AND WCY_SDK_LIB_FOUND)
    install(FILES "${WCY_SDK_LIB_PATH}"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()
