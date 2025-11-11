cmake_minimum_required(VERSION 3.16)

project(Sensors VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

cmake_minimum_required(VERSION 3.16)

find_package(Qt6 REQUIRED COMPONENTS Core HttpServer WebSockets Widgets WebEngineWidgets Concurrent Network )

find_package(Qt6 REQUIRED COMPONENTS
    Protobuf
    Grpc
    ProtobufTools
)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(appSensors
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
)

qt_add_qml_module(appSensors
    URI Sensors
    VERSION 1.0
    QML_FILES
        Main.qml
        SOURCES grpc_client.h
        SOURCES grpc_client.cpp
        SOURCES sensorserviceclient.h sensorserviceclient.cpp
        SOURCES wcydeviceservice.h wcydeviceservice.cpp
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appSensors PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appSensors
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Добавление protobuf поддержки
qt_add_protobuf(appSensors
    PROTO_FILES greet.proto sensor_service.proto
)

# Добавление gRPC клиента
qt_add_grpc(appSensors CLIENT
    PROTO_FILES
        greet.proto
        sensor_service.proto
)


target_link_libraries(appSensors
#      PRIVATE Qt6::Quick Qt6::Network Qt6::WebSockets Qt6::Widgets Qt6::WebEngineWidgets Qt6::WebEngineQuick
      PRIVATE Qt6::HttpServer Qt6::Network Qt6::WebSockets Qt6::Widgets Qt6::WebEngineWidgets
      Qt6::Concurrent
       Qt6::Network
       Qt6::Protobuf
       Qt6::Grpc
)

include(GNUInstallDirs)
install(TARGETS appSensors
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
