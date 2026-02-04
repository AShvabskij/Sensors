cmake_minimum_required(VERSION 3.16)

project(Sensors VERSION 0.1 LANGUAGES CXX)

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
find_package(Qt6 REQUIRED COMPONENTS Core HttpServer WebSockets Widgets WebEngineWidgets Concurrent Network)

# Protobuf и Grpc только для Windows
if(WIN32)
    find_package(Qt6 REQUIRED COMPONENTS
        Protobuf
        Grpc
        ProtobufTools
    )
    message(STATUS "Enabling Protobuf and gRPC for Windows")
else()
    message(STATUS "Skipping Protobuf and gRPC for non-Windows platform")
endif()

qt_standard_project_setup(REQUIRES 6.5)

# Настройка пути к библиотеке WCY_DisLab_SDK (только для Windows)
if(WIN32)
    # Путь к библиотеке (предполагаем, что она в папке 3lib)
    set(WCY_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3lib")

    # Проверяем существование библиотеки
    if(EXISTS "${WCY_SDK_DIR}/WCY_DisLab_SDK.lib")
        message(STATUS "Found WCY_DisLab_SDK library at ${WCY_SDK_DIR}")

        # Добавляем библиотеку
        add_library(WCY_DisLab_SDK SHARED IMPORTED)
        set_target_properties(WCY_DisLab_SDK PROPERTIES
            IMPORTED_LOCATION "${WCY_SDK_DIR}/WCY_DisLab_SDK.dll"
            IMPORTED_IMPLIB "${WCY_SDK_DIR}/WCY_DisLab_SDK.lib"
        )

        set(WCY_SDK_AVAILABLE TRUE)
        message(STATUS "WCY_DisLab_SDK library configured")
    else()
        message(WARNING "WCY_DisLab_SDK library not found at ${WCY_SDK_DIR}")
        set(WCY_SDK_AVAILABLE FALSE)
    endif()
else()
    set(WCY_SDK_AVAILABLE FALSE)
    message(STATUS "WCY_DisLab_SDK only supported on Windows")
endif()

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
    wcy_dislab_wrapper.h
    wcy_dislab_wrapper.cpp
    wcydeviceservice.h
    wcydeviceservice.cpp
)

# Файлы только для Windows
#if(WIN32)
#    set(WINDOWS_SOURCES
#        wcygrpcdeviceservice.h
#        wcygrpcdeviceservice.cpp
#        wcygrpcwrapper.h
#        wcygrpcwrapper.cpp
#        grpc_client.h
#        grpc_client.cpp
#    )
#    list(APPEND COMMON_SOURCES ${WINDOWS_SOURCES})
#endif()

qt_add_executable(appSensors ${COMMON_SOURCES})
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-DQT_NO_DEBUG_OUTPUT -DQT_NO_WARNING_OUTPUT)
endif()

# Добавляем include директорию для WCY SDK после создания цели
if(WIN32 AND WCY_SDK_AVAILABLE AND EXISTS "${WCY_SDK_DIR}/WCY_DisLab_SDK.h")
    target_include_directories(appSensors PRIVATE ${WCY_SDK_DIR})
    message(STATUS "Added ${WCY_SDK_DIR} to include directories")
endif()

# QML модуль
qt_add_qml_module(appSensors
    URI Sensors
    VERSION 1.0
    QML_FILES
        Main.qml
)

# Настройки для разных платформ
if(APPLE)
    # Настройки для macOS/iOS
    set_target_properties(appSensors PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appSensors
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
    )
elseif(WIN32)
    # Настройки для Windows
    set_target_properties(appSensors PROPERTIES
        WIN32_EXECUTABLE TRUE
    )

    # Добавление protobuf поддержки (только для Windows)
    qt_add_protobuf(appSensors
        PROTO_FILES greet.proto sensor_service.proto
    )

    # Добавление gRPC клиента (только для Windows)
    qt_add_grpc(appSensors CLIENT
        PROTO_FILES
            greet.proto
            sensor_service.proto
    )
endif()

# Библиотеки для всех платформ
target_link_libraries(appSensors
    PRIVATE
        Qt6::HttpServer
        Qt6::Network
        Qt6::WebSockets
        Qt6::Widgets
        Qt6::WebEngineWidgets
        Qt6::Concurrent
)

# Дополнительные библиотеки для Windows (Protobuf и Grpc)
if(WIN32)
    target_link_libraries(appSensors
        PRIVATE
            Qt6::Protobuf
            Qt6::Grpc
    )
endif()

# Библиотека WCY_DisLab_SDK (только для Windows если найдена)
if(WIN32 AND WCY_SDK_AVAILABLE)
    target_link_libraries(appSensors
        PRIVATE
            WCY_DisLab_SDK
    )
    message(STATUS "Linked with WCY_DisLab_SDK")
endif()

# Условная компиляция
target_compile_definitions(appSensors
    PRIVATE
        $<$<BOOL:${WIN32}>:WITH_GRPC=1>
        $<$<AND:$<BOOL:${WIN32}>,$<BOOL:${WCY_SDK_AVAILABLE}>>:WITH_WCY_SDK=1>
)

# Копирование DLL (только для Windows)
if(WIN32 AND WCY_SDK_AVAILABLE)
    add_custom_command(TARGET appSensors POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WCY_SDK_DIR}/WCY_DisLab_SDK.dll"
            "$<TARGET_FILE_DIR:appSensors>/WCY_DisLab_SDK.dll"
        COMMENT "Copying WCY_DisLab_SDK.dll to output directory"
    )

    # Альтернативный вариант: установка при инсталляции
    install(FILES "${WCY_SDK_DIR}/WCY_DisLab_SDK.dll"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        CONFIGURATIONS Release RelWithDebInfo MinSizeRel
    )
endif()

# Инсталляция
include(GNUInstallDirs)
install(TARGETS appSensors
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Дополнительно: настройка для копирования конфигурационных файлов
if(WIN32)
    # Копирование папки config если она существует
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config")
        install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/config"
            DESTINATION .
        )
    endif()

    # Копирование папки WCY-SDK если она существует
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/WCY-SDK")
        install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/WCY-SDK"
            DESTINATION .
        )
    endif()
endif()
