#include "SimpleWebServer.h"
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QHostAddress>
#include <QFileInfo>
#include <QJsonArray>

SimpleWebServer::SimpleWebServer(QObject *parent)
    : QObject(parent)
    , m_tcpServer(nullptr)
    , m_port(0)
    , m_isRunning(false)
    , m_webRoot("./front/build") // Default to your React build folder
{
}

SimpleWebServer::~SimpleWebServer()
{
    stopServer();
}

bool SimpleWebServer::startServer(quint16 port)
{
    if (m_isRunning) {
        return true; // Already running
    }

    // Validate that the web root exists and has the expected structure
    if (!validateWebRoot()) {
        qWarning() << "Web root validation failed:" << m_webRoot;
        return false;
    }

    setupRoutes();

    // Create and configure TCP server
    m_tcpServer = new QTcpServer(this);

    // Try to listen on the specified port
    if (!m_tcpServer->listen(QHostAddress::Any, port)) {
        qWarning() << "Failed to start server on port" << port << ":" << m_tcpServer->errorString();
        delete m_tcpServer;
        m_tcpServer = nullptr;
        return false;
    }

    // Bind the HTTP server to the TCP server
    if (!m_httpServer.bind(m_tcpServer)) {
        qWarning() << "Failed to bind HTTP server to TCP server";
        delete m_tcpServer;
        m_tcpServer = nullptr;
        return false;
    }

    m_port = m_tcpServer->serverPort();
    m_isRunning = true;

    qDebug() << "SimpleWebServer started on http://localhost:" << m_port;
    qDebug() << "Web root:" << QDir(m_webRoot).absolutePath();
    qDebug() << "Serving React app from build folder";

    emit serverStarted(m_port);
    return true;
}

void SimpleWebServer::stopServer()
{
    if (m_isRunning && m_tcpServer) {
        m_tcpServer->close();
        m_tcpServer->deleteLater();
        m_tcpServer = nullptr;
        m_isRunning = false;
        emit serverStopped();
        qDebug() << "SimpleWebServer stopped";
    }
}

bool SimpleWebServer::validateWebRoot()
{
    QDir webDir(m_webRoot);

    if (!webDir.exists()) {
        qWarning() << "Web root directory does not exist:" << m_webRoot;
        return false;
    }

    // Check for essential React build files
    QFileInfo indexFile(webDir.filePath("index.html"));
    QDir staticDir(webDir.filePath("static"));

    if (!indexFile.exists()) {
        qWarning() << "index.html not found in web root:" << m_webRoot;
        return false;
    }


    if (!staticDir.exists()) {
        qWarning() << "static folder not found in web root:" << m_webRoot;
    }

    qDebug() << "Web root validated successfully";
    qDebug() << "  - index.html:" << (indexFile.exists() ? "found" : "missing");
    qDebug() << "  - static folder:" << (staticDir.exists() ? "found" : "missing");

    return true;
}

void SimpleWebServer::setupRoutes()
{
    // Serve index.html for root and all SPA routes
    m_httpServer.route("/", QHttpServerRequest::Method::Get,
                       [this](const QHttpServerRequest &request) {
                           qDebug() << "index.html";
                           emit requestReceived("GET", "/");
                           return serveStaticFile(m_webRoot + "/index.html");
                       });

    // Serve static files (CSS, JS, images, etc.)
    m_httpServer.route("/static/<arg>", QHttpServerRequest::Method::Get,
                       [this](const QUrl &url, const QHttpServerRequest &request) {
                           QString path = "/static/" + url.path();
                            qDebug() << "static file " << path;

                           emit requestReceived("GET", path);
                           return serveStaticFile(m_webRoot + path);
                       });

    // Serve other static files (favicon, manifest, etc.)
    m_httpServer.route("/<arg>", QHttpServerRequest::Method::Get,
                       [this](const QUrl &url, const QHttpServerRequest &request) {
                           QString path = url.path();
                           QString fullPath = m_webRoot + path;

                           // Check if the file exists
                           QFile file(fullPath);
                           if (file.exists()) {
                               emit requestReceived("GET", path);
                               return serveStaticFile(fullPath);
                           }

                           // For SPA routing, serve index.html for any unknown routes
                           // This allows client-side routing to work
                           emit requestReceived("GET", path);
                           return serveStaticFile(m_webRoot + "/index.html");
                       });

    // API routes for server information
    m_httpServer.route("/api/hello", QHttpServerRequest::Method::Get,
                       [this](const QHttpServerRequest &request) {
                           emit requestReceived("GET", "/api/hello");
                           QJsonObject response;
                           response["message"] = "Hello from React App Server!";
                           response["status"] = "success";
                           response["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
                           return createJsonResponse(response);
                       });

    m_httpServer.route("/api/info", QHttpServerRequest::Method::Get,
                       [this](const QHttpServerRequest &request) {
                           emit requestReceived("GET", "/api/info");
                           QJsonObject response;
                           response["server_name"] = "React App Server";
                           response["qt_version"] = qVersion();
                           response["port"] = m_port;
                           response["is_running"] = m_isRunning;
                           response["web_root"] = m_webRoot;
                           response["web_root_absolute"] = QDir(m_webRoot).absolutePath();

                           // Add React app info
                           QDir webDir(m_webRoot);
                           QJsonObject reactInfo;
                           reactInfo["index_html"] = QFileInfo(webDir.filePath("index.html")).exists();
                           reactInfo["asset_manifest"] = QFileInfo(webDir.filePath("asset-manifest.json")).exists();
                           reactInfo["static_folder"] = QDir(webDir.filePath("static")).exists();
                           response["react_app"] = reactInfo;

                           return createJsonResponse(response);
                       });

    // Обработчик POST запроса для /export
    m_httpServer.route("/export", QHttpServerRequest::Method::Post,
                       [this](const QHttpServerRequest &request) {
                           emit requestReceived("POST", "/export");

                           // Получаем тело запроса
                           QByteArray requestBody = request.body();

                           // Парсим JSON
                           QJsonParseError parseError;
                           QJsonDocument requestDoc = QJsonDocument::fromJson(requestBody, &parseError);

                           if (parseError.error != QJsonParseError::NoError) {
                               QJsonObject error;
                               error["error"] = "Invalid JSON";
                               error["message"] = "Failed to parse JSON: " + parseError.errorString();
                               error["statusCode"] = 400;
                               return createJsonResponse(error);
                           }

                           if (!requestDoc.isObject()) {
                               QJsonObject error;
                               error["error"] = "Invalid Data";
                               error["message"] = "Expected JSON object in request body";
                               error["statusCode"] = 400;
                               return createJsonResponse(error);
                           }

                           QJsonObject requestData = requestDoc.object();

                           // Обрабатываем данные (здесь можно добавить вашу логику)
                           QJsonObject response;
                           response["status"] = "success";
                           response["message"] = "Data received successfully";
                           response["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
                           response["received_data"] = requestData; // Опционально: вернуть полученные данные для отладки

                           // Здесь можно добавить обработку данных:
                           // - сохранение в файл
                           // - экспорт в Excel
                           // - и т.д.

                           qDebug() << "Received export data:";
                           qDebug() << "  xAxis:" << requestData["xAxis"].toString();
                           qDebug() << "  data count:" << (requestData["data"].isArray() ? requestData["data"].toArray().size() : 0);
                           qDebug() << "  timestamp:" << requestData["timestamp"].toString();

                           return createJsonResponse(response);
                       });


    // 404 handler for missing API routes
    auto func = ([this](const QHttpServerRequest &request) {
        QString path = request.url().path();
        QString method = QVariant::fromValue(request.method()).toString();
        emit requestReceived(method, path);

        // If it's an API route, return JSON error
        if (path.startsWith("/api/") || path == "/export") {
            QJsonObject error;
            error["error"] = "Not Found";
            error["message"] = "Endpoint not found: " + path;
            error["method"] = method;
            error["statusCode"] = 404;
            error["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            return QHttpServerResponse(QJsonDocument(error).toJson(),
                                       "application/json",
                                       QHttpServerResponse::StatusCode::NotFound);
        }

        // For non-API routes, serve index.html (SPA fallback)
        return serveStaticFile(m_webRoot + "/index.html");
    });

    QObject context;
    m_httpServer.setMissingHandler(&context, func);
}

QHttpServerResponse SimpleWebServer::serveStaticFile(const QString &filePath)
{
    QFile file(filePath);

    qDebug() << "server filePath" << filePath;
    if (!file.exists()) {
        // For SPA, if file doesn't exist but we're not in API, serve index.html
        if (!filePath.contains("/api/") && QFileInfo(filePath).fileName() != "index.html") {
            QFile indexFile(m_webRoot + "/index.html");
            if (indexFile.exists() && indexFile.open(QIODevice::ReadOnly)) {
                QByteArray data = indexFile.readAll();

                return QHttpServerResponse("text/html", data);
            }
        }

        QJsonObject error;
        error["error"] = "File Not Found";
        error["message"] = "File does not exist: " + filePath;
        error["statusCode"] = 404;
        return QHttpServerResponse(QJsonDocument(error).toJson(),
                                   "application/json",
                                   QHttpServerResponse::StatusCode::NotFound);
    }

    if (!file.open(QIODevice::ReadOnly)) {
        QJsonObject error;
        error["error"] = "Server Error";
        error["message"] = "Cannot read file: " + filePath;
        error["statusCode"] = 500;
        return QHttpServerResponse(QJsonDocument(error).toJson(),
                                   "application/json",
                                   QHttpServerResponse::StatusCode::InternalServerError);
    }

    QByteArray data = file.readAll();
    QString contentType = detectContentType(filePath);

    return QHttpServerResponse(contentType.toStdString().c_str(), data);
}

QString SimpleWebServer::detectContentType(const QString &filePath) const
{
    QHash<QString, QString> contentTypes = {
        {".html", "text/html"},
        {".htm", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".jsx", "application/javascript"},
        {".ts", "application/javascript"},
        {".tsx", "application/javascript"},
        {".json", "application/json"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".svg", "image/svg+xml"},
        {".ico", "image/x-icon"},
        {".woff", "font/woff"},
        {".woff2", "font/woff2"},
        {".ttf", "font/ttf"},
        {".eot", "application/vnd.ms-fontobject"},
        {".otf", "font/otf"},
        {".txt", "text/plain"},
        {".xml", "application/xml"},
        {".pdf", "application/pdf"}
    };

    for (auto it = contentTypes.begin(); it != contentTypes.end(); ++it) {
        if (filePath.endsWith(it.key(), Qt::CaseInsensitive)) {
            return it.value();
        }
    }

    // Fallback to MIME database detection
    QMimeDatabase mimeDb;
    return mimeDb.mimeTypeForFile(filePath).name();
}

QHttpServerResponse SimpleWebServer::createJsonResponse(const QJsonObject &json)
{
    QJsonDocument doc(json);
    return QHttpServerResponse(doc.toJson(QJsonDocument::Indented));
}
