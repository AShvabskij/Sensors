#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include <QMainWindow>
#include <QApplication>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QWebEngineView>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include "simplewebserver.h"

#include "core.h"

#include "grpc_client.h"

int main2(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

// Enable WebEngine if needed
#ifdef QT_WEBENGINE_CORE_LIB
    QtWebEngine::initialize();
#endif

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("Sensors", "Main");


    SimpleWebServer webServer;

    QString webRoot = "./front/build";
    webServer.setWebRoot(webRoot);

    // Start the server on port 3000
    if (!webServer.startServer(3000)) {
        QMessageBox::critical(nullptr, "Server Error",
                              "Failed to start SimpleWebServer on port 3000!");
        return -1;
    }

    Core core;
    core.init();

    core.start();

    return app.exec();
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Create simple web server
    SimpleWebServer webServer;

    QString webRoot = "./front/build";
    webServer.setWebRoot(webRoot);

    // Start the server on port 3000
    if (!webServer.startServer(3000)) {
        QMessageBox::critical(nullptr, "Server Error",
                              "Failed to start SimpleWebServer on port 3000!");
        return -1;
    }

    QMainWindow window;
    QWidget *centralWidget = new QWidget(&window);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    // WebView
    QWebEngineView *webView = new QWebEngineView();
    webView->setUrl(QUrl(QString("http://localhost:%1").arg(webServer.getPort())));

    // Optional: Enable dev tools (right-click → Inspect)
    webView->page()->setDevToolsPage(webView->page());
    layout->addWidget(webView);

    window.setCentralWidget(centralWidget);

    // Connect to server signals
    QObject::connect(&webServer, &SimpleWebServer::requestReceived,
                     [](const QString &method, const QString &path) {
                        qDebug() << QString("Last request: %1 %2 | %3")
                                                  .arg(method)
                                                  .arg(path)
                                                  .arg(QDateTime::currentDateTime().toString("hh:mm:ss"));
                     });

    QObject::connect(&webServer, &SimpleWebServer::serverStarted,
                     [](quint16 port) {
                         qDebug() << "Server started successfully on port" << port;
                     });

    QObject::connect(&webServer, &SimpleWebServer::serverStopped,
                     []() {
                         qDebug() << "Server stopped";
                     });

    // Get the available screen geometry
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();

    // Calculate 3/4 of screen size
    int width = screenGeometry.width() * 3 / 4;
    int height = screenGeometry.height() * 3 / 4;

    // Resize and center the window
    window.resize(width, height);
    window.move(screenGeometry.center() - window.rect().center());

    window.show();

    Core core;
    core.init();
    core.start();

    // GrpcClient client;

    // QString address = "http://localhost:5001";
    // QString name = "test_name";

    // // Подключаем сигналы для вывода в консоль
    // QObject::connect(&client, &GrpcClient::messageReceived, [](const QString &msg) {
    //     qInfo().noquote() << "[INFO]" << msg;
    // });

    // QObject::connect(&client, &GrpcClient::streamMessageReceived, [](const QString &msg) {
    //     qInfo().noquote() << "[STREAM]" << msg;
    // });

    // QObject::connect(&client, &GrpcClient::errorOccurred, [](const QString &error) {
    //     qCritical().noquote() << "[ERROR]" << error;
    // });

    // // Автоматический тест при запуске
    // QTimer::singleShot(100, [&]() {
    //     qInfo() << "=== Starting gRPC Client Test ===";
    //     qInfo() << "Server:" << address;
    //     qInfo() << "Test name:" << name;

    //     client.connectToServer(address);
    // });

    // // Выполняем тесты после подключения
    // QObject::connect(&client, &GrpcClient::connectedChanged, [&](bool connected) {
    //     if (connected) {
    //         QTimer::singleShot(500, [&]() {
    //             qInfo() << "\n=== Testing SayHello ===";
    //             client.sayHello(name);
    //         });

    //         QTimer::singleShot(2000, [&]() {
    //             qInfo() << "\n=== Testing Stream ===";
    //             client.startStream(name);
    //         });

    //         // Завершаем приложение через 10 секунд
    //         QTimer::singleShot(10000, [&]() {
    //             qInfo() << "\n=== Test completed ===";
    //             client.disconnectFromServer();
    //         });
    //     }
    // });

    return app.exec();
}
