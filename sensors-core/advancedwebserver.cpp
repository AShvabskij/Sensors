#include "AdvancedWebServer.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkInterface>
#include <QProcess>
#include <QBuffer>
#include <QMimeDatabase>

AdvancedWebServer::AdvancedWebServer(QObject *parent)
    : QObject(parent)
    , m_fileWatcher(new QFileSystemWatcher(this))
    , m_port(0)
    , m_isRunning(false)
{
    m_fileCache.setMaxCost(100 * 1024 * 1024); // 100MB cache
    
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged,
            this, &AdvancedWebServer::onFileChanged);
    connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged,
            this, &AdvancedWebServer::onFileChanged);
}

AdvancedWebServer::~AdvancedWebServer()
{
    stopServer();
}

bool AdvancedWebServer::startServer(const ServerConfig &config)
{
    if (m_isRunning) {
        stopServer();
    }

    m_config = config;
    initializeWebDirectory();

    // Setup middleware first
    setupMiddleware();
    setupErrorHandling();
    setupRoutes();

    // Start listening
    m_port = m_httpServer.listen(QHostAddress::Any, config.port);
    if (!m_port) {
        emit errorOccurred("Failed to start server on port " + QString::number(config.port));
        return false;
    }

    m_isRunning = true;
    
    if (config.liveReload) {
        setupFileWatcher();
    }

    qDebug() << "Advanced QHttpServer started on port" << m_port;
    qDebug() << "Web root:" << QDir(m_config.webRoot).absolutePath();
    qDebug() << "Caching:" << (config.enableCaching ? "enabled" : "disabled");
    qDebug() << "Live reload:" << (config.liveReload ? "enabled" : "disabled");

    emit serverStarted(m_port);
    return true;
}

void AdvancedWebServer::stopServer()
{
    if (m_isRunning) {
        m_httpServer.stopListening();
        m_isRunning = false;
        emit serverStopped();
    }
}

void AdvancedWebServer::initializeWebDirectory()
{
    QDir webDir(m_config.webRoot);
    if (!webDir.exists()) {
        webDir.mkpath(".");
    }

    // Create default index.html if it doesn't exist
    QFile indexFile(webDir.filePath("index.html"));
    if (!indexFile.exists()) {
        if (indexFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QString html = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Advanced Qt QHttpServer</title>
    <style>
        /* Advanced styling */
        :root {
            --primary: #2563eb;
            --secondary: #7c3aed;
            --success: #10b981;
            --warning: #f59e0b;
            --error: #ef4444;
        }
        
        body {
            font-family: 'Inter', system-ui, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            margin: 0;
            padding: 20px;
        }
        
        .container {
            max-width: 1200px;
            margin: 0 auto;
        }
        
        .card {
            background: rgba(255, 255, 255, 0.95);
            backdrop-filter: blur(10px);
            border-radius: 20px;
            padding: 2rem;
            margin-bottom: 2rem;
            box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.25);
            border: 1px solid rgba(255, 255, 255, 0.2);
        }
        
        .stats-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 1rem;
            margin: 2rem 0;
        }
        
        .stat-card {
            background: white;
            padding: 1.5rem;
            border-radius: 12px;
            text-align: center;
            box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
        }
        
        .stat-value {
            font-size: 2rem;
            font-weight: bold;
            color: var(--primary);
        }
        
        .api-test {
            display: flex;
            gap: 1rem;
            flex-wrap: wrap;
        }
        
        .btn {
            padding: 0.75rem 1.5rem;
            border: none;
            border-radius: 8px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s;
        }
        
        .btn-primary { background: var(--primary); color: white; }
        .btn-secondary { background: var(--secondary); color: white; }
        .btn-success { background: var(--success); color: white; }
        
        .btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 8px 15px -3px rgba(0, 0, 0, 0.2);
        }
        
        #results {
            margin-top: 2rem;
        }
        
        .result-item {
            background: #f8fafc;
            border: 1px solid #e2e8f0;
            border-radius: 8px;
            padding: 1rem;
            margin-bottom: 1rem;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="card">
            <h1>🚀 Advanced Qt QHttpServer</h1>
            <p>Qt 6.10 with advanced features: caching, live reload, WebSocket, and real-time stats</p>
            
            <div class="stats-grid" id="statsGrid">
                <!-- Stats will be populated by JavaScript -->
            </div>
            
            <div class="api-test">
                <button class="btn btn-primary" onclick="testApi('stats')">Get Server Stats</button>
                <button class="btn btn-secondary" onclick="testApi('system')">System Info</button>
                <button class="btn btn-success" onclick="testApi('echo')">Test Echo API</button>
                <button class="btn" onclick="clearCache()">Clear Cache</button>
            </div>
            
            <div id="results"></div>
        </div>
    </div>

    <script>
        async function testApi(endpoint) {
            try {
                const response = await fetch(`/api/${endpoint}`);
                const data = await response.json();
                
                displayResult({
                    endpoint: `/api/${endpoint}`,
                    status: response.status,
                    data: data,
                    timestamp: new Date().toISOString()
                });
                
                updateStats();
            } catch (error) {
                displayResult({
                    endpoint: `/api/${endpoint}`,
                    error: error.toString(),
                    timestamp: new Date().toISOString()
                });
            }
        }
        
        async function clearCache() {
            try {
                const response = await fetch('/api/clear-cache', { method: 'POST' });
                const data = await response.json();
                alert('Cache cleared: ' + data.message);
                updateStats();
            } catch (error) {
                alert('Error clearing cache: ' + error);
            }
        }
        
        function displayResult(result) {
            const resultsDiv = document.getElementById('results');
            const resultItem = document.createElement('div');
            resultItem.className = 'result-item';
            
            if (result.error) {
                resultItem.innerHTML = `
                    <strong>❌ ${result.endpoint}</strong><br>
                    <span style="color: var(--error)">Error: ${result.error}</span><br>
                    <small>${result.timestamp}</small>
                `;
            } else {
                resultItem.innerHTML = `
                    <strong>✅ ${result.endpoint} (${result.status})</strong><br>
                    <pre>${JSON.stringify(result.data, null, 2)}</pre>
                    <small>${result.timestamp}</small>
                `;
            }
            
            resultsDiv.prepend(resultItem);
        }
        
        async function updateStats() {
            try {
                const response = await fetch('/api/stats');
                const stats = await response.json();
                
                const statsGrid = document.getElementById('statsGrid');
                statsGrid.innerHTML = `
                    <div class="stat-card">
                        <div class="stat-value">${stats.totalRequests}</div>
                        <div>Total Requests</div>
                    </div>
                    <div class="stat-card">
                        <div class="stat-value">${stats.apiRequests}</div>
                        <div>API Requests</div>
                    </div>
                    <div class="stat-card">
                        <div class="stat-value">${stats.cache.size}</div>
                        <div>Cache Items</div>
                    </div>
                    <div class="stat-card">
                        <div class="stat-value">${stats.uptime}</div>
                        <div>Uptime (s)</div>
                    </div>
                `;
            } catch (error) {
                console.error('Failed to update stats:', error);
            }
        }
        
        // Initial stats load
        updateStats();
        setInterval(updateStats, 5000); // Update every 5 seconds
    </script>
</body>
</html>
)";
            indexFile.write(html.toUtf8());
            indexFile.close();
        }
    }
}

void AdvancedWebServer::setupMiddleware()
{
    // Global request logging middleware
    m_httpServer.beforeRequest([](QHttpServerRequest &request) {
        // You can modify requests before they reach handlers
        // Example: Add CORS headers, authentication, etc.
        return std::nullopt; // Continue processing
    });

    // Global response middleware
    m_httpServer.afterRequest([](QHttpServerResponse &&response) {
        // Add common headers to all responses
        response.setHeader("Server", "Qt Advanced WebServer 6.10");
        response.setHeader("X-Powered-By", "Qt");
        return std::move(response);
    });
}

void AdvancedWebServer::setupErrorHandling()
{
    m_httpServer.setMissingHandler([](const QHttpServerRequest &request) {
        return QHttpServerResponse("Route not found: " + request.url().path(),
                                 QHttpServerResponse::StatusCode::NotFound);
    });

    // Error handler for exceptions
    m_httpServer.setErrorHandler([](std::exception_ptr &&ptr, const QHttpServerRequest &request) {
        try {
            if (ptr) std::rethrow_exception(ptr);
        } catch (const std::exception &e) {
            qWarning() << "Request error:" << e.what();
        }
        return QHttpServerResponse("Internal Server Error",
                                 QHttpServerResponse::StatusCode::InternalServerError);
    });
}

void AdvancedWebServer::setupRoutes()
{
    // Static file serving with caching
    m_httpServer.route("/<arg>", [this](const QUrl &url, const QHttpServerRequest &request) {
        QString path = url.path();
        if (path == "/") {
            path = "/index.html";
        }
        
        QString filePath = m_config.webRoot + path;
        return serveStaticFile(filePath);
    });

    // API routes
    m_httpServer.route("/api/stats", [this](const QHttpServerRequest &request) {
        return handleApiStats(request);
    });

    m_httpServer.route("/api/system", [this](const QHttpServerRequest &request) {
        return handleApiSystemInfo(request);
    });

    m_httpServer.route("/api/echo", [this](const QHttpServerRequest &request) {
        return handleApiEcho(request);
    });

    m_httpServer.route("/api/clear-cache", QHttpServerRequest::Method::Post,
                      [this](const QHttpServerRequest &request) {
        clearCache();
        QJsonObject response;
        response["message"] = "Cache cleared successfully";
        response["timestamp"] = QDateTime::currentDateTime().toString();
        return createJsonResponse(response);
    });

    // File upload endpoint
    m_httpServer.route("/api/upload", QHttpServerRequest::Method::Post,
                      [this](const QHttpServerRequest &request) {
        return handleFileUpload(request);
    });

    // WebSocket upgrade endpoint
    m_httpServer.route("/ws", [this](const QHttpServerRequest &request) {
        return handleWebSocketUpgrade(request);
    });
}

QHttpServerResponse AdvancedWebServer::serveStaticFile(const QString &filePath)
{
    QMutexLocker locker(&m_cacheMutex);
    m_stats.staticRequests++;
    m_stats.totalRequests++;

    // Check cache first
    if (m_config.enableCaching) {
        QByteArray cachedData;
        QString contentType;
        if (getFromCache(filePath, cachedData, contentType)) {
            emit requestReceived("GET", filePath, 200);
            return QHttpServerResponse(cachedData, contentType.toUtf8());
        }
    }

    QFile file(filePath);
    if (!file.exists()) {
        emit requestReceived("GET", filePath, 404);
        return createErrorResponse(QHttpServerResponse::StatusCode::NotFound, 
                                 "File not found: " + filePath);
    }

    if (!file.open(QIODevice::ReadOnly)) {
        emit requestReceived("GET", filePath, 500);
        return createErrorResponse(QHttpServerResponse::StatusCode::InternalServerError,
                                 "Cannot read file: " + filePath);
    }

    QByteArray data = file.readAll();
    QString contentType = detectContentType(filePath);

    // Add to cache
    if (m_config.enableCaching) {
        addToCache(filePath, data, contentType);
    }

    emit requestReceived("GET", filePath, 200);
    return QHttpServerResponse(data, contentType.toUtf8());
}

QHttpServerResponse AdvancedWebServer::handleApiStats(const QHttpServerRequest &request)
{
    QMutexLocker locker(&m_statsMutex);
    m_stats.apiRequests++;
    m_stats.totalRequests++;
    m_stats.endpointHits["/api/stats"]++;

    QJsonObject stats;
    stats["totalRequests"] = static_cast<qint64>(m_stats.totalRequests);
    stats["apiRequests"] = static_cast<qint64>(m_stats.apiRequests);
    stats["staticRequests"] = static_cast<qint64>(m_stats.staticRequests);
    stats["errors"] = static_cast<qint64>(m_stats.errors);
    
    QJsonObject cacheStats;
    cacheStats["size"] = m_fileCache.totalCost() / 1024 / 1024; // MB
    cacheStats["count"] = m_fileCache.count();
    cacheStats["maxSize"] = m_fileCache.maxCost() / 1024 / 1024; // MB
    stats["cache"] = cacheStats;
    
    stats["uptime"] = QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000; // Simple uptime

    emit requestReceived("GET", "/api/stats", 200);
    return createJsonResponse(stats);
}

QHttpServerResponse AdvancedWebServer::handleApiSystemInfo(const QHttpServerRequest &request)
{
    m_stats.apiRequests++;
    m_stats.totalRequests++;
    m_stats.endpointHits["/api/system"]++;

    QJsonObject systemInfo;
    systemInfo["hostname"] = QSysInfo::machineHostName();
    systemInfo["platform"] = QSysInfo::prettyProductName();
    systemInfo["architecture"] = QSysInfo::currentCpuArchitecture();
    systemInfo["qtVersion"] = qVersion();
    systemInfo["serverPort"] = m_port;
    systemInfo["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    // Network interfaces
    QJsonArray networks;
    for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces()) {
        if (interface.flags() & QNetworkInterface::IsUp && 
            !(interface.flags() & QNetworkInterface::IsLoopBack)) {
            QJsonObject netInfo;
            netInfo["name"] = interface.humanReadableName();
            netInfo["hardwareAddress"] = interface.hardwareAddress();
            
            QJsonArray addresses;
            for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    addresses.append(entry.ip().toString());
                }
            }
            netInfo["addresses"] = addresses;
            networks.append(netInfo);
        }
    }
    systemInfo["networks"] = networks;

    emit requestReceived("GET", "/api/system", 200);
    return createJsonResponse(systemInfo);
}

QHttpServerResponse AdvancedWebServer::handleApiEcho(const QHttpServerRequest &request)
{
    m_stats.apiRequests++;
    m_stats.totalRequests++;
    m_stats.endpointHits["/api/echo"]++;

    QJsonObject echo;
    echo["method"] = QString::fromUtf8(request.method());
    echo["url"] = request.url().toString();
    echo["query"] = QJsonObject::fromVariantHash(request.query().toVariantHash());
    echo["headers"] = QJsonObject::fromVariantHash(request.headers());
    echo["body"] = QString::fromUtf8(request.body());
    echo["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    emit requestReceived("GET", "/api/echo", 200);
    return createJsonResponse(echo);
}

QHttpServerResponse AdvancedWebServer::handleWebSocketUpgrade(const QHttpServerRequest &request)
{
    // WebSocket upgrade handling would go here
    // This is a placeholder for WebSocket functionality
    return createErrorResponse(QHttpServerResponse::StatusCode::NotImplemented,
                             "WebSocket support not implemented");
}

QHttpServerResponse AdvancedWebServer::handleFileUpload(const QHttpServerRequest &request)
{
    // File upload handling would go here
    return createErrorResponse(QHttpServerResponse::StatusCode::NotImplemented,
                             "File upload not implemented");
}

QString AdvancedWebServer::detectContentType(const QString &filePath) const
{
    QMimeDatabase mimeDb;
    QMimeType mimeType = mimeDb.mimeTypeForFile(filePath);
    return mimeType.name();
}

QHttpServerResponse AdvancedWebServer::createJsonResponse(const QJsonObject &json, 
                                                        QHttpServerResponse::StatusCode status)
{
    QJsonDocument doc(json);
    return QHttpServerResponse(doc.toJson(QJsonDocument::Indented), "application/json", status);
}

QHttpServerResponse AdvancedWebServer::createErrorResponse(QHttpServerResponse::StatusCode status,
                                                         const QString &message)
{
    QJsonObject error;
    error["error"] = QHttpServerResponse::reasonPhrase(status);
    error["statusCode"] = static_cast<int>(status);
    error["message"] = message;
    error["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    m_stats.errors++;
    m_stats.statusCodes[static_cast<int>(status)]++;
    
    return createJsonResponse(error, status);
}

void AdvancedWebServer::addToCache(const QString &key, const QByteArray &data, const QString &contentType)
{
    QMutexLocker locker(&m_cacheMutex);
    auto *cacheItem = new QPair<QByteArray, QString>(data, contentType);
    m_fileCache.insert(key, cacheItem, data.size());
}

bool AdvancedWebServer::getFromCache(const QString &key, QByteArray &data, QString &contentType)
{
    QMutexLocker locker(&m_cacheMutex);
    auto *cacheItem = m_fileCache.object(key);
    if (cacheItem) {
        data = cacheItem->first;
        contentType = cacheItem->second;
        return true;
    }
    return false;
}

void AdvancedWebServer::clearCache()
{
    QMutexLocker locker(&m_cacheMutex);
    m_fileCache.clear();
    qDebug() << "Cache cleared";
}

void AdvancedWebServer::reloadWebContent()
{
    clearCache();
    initializeWebDirectory();
    qDebug() << "Web content reloaded";
}

void AdvancedWebServer::onFileChanged(const QString &path)
{
    if (m_config.liveReload) {
        qDebug() << "File changed, reloading:" << path;
        QTimer::singleShot(100, this, &AdvancedWebServer::reloadWebContent);
    }
}

void AdvancedWebServer::setupFileWatcher()
{
    QDir webDir(m_config.webRoot);
    m_fileWatcher->addPath(webDir.absolutePath());
    
    // Watch all subdirectories recursively
    for (const QFileInfo &info : webDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        m_fileWatcher->addPath(info.absoluteFilePath());
    }
    
    // Watch all HTML, CSS, JS files
    QStringList filters = {"*.html", "*.css", "*.js", "*.json"};
    for (const QFileInfo &info : webDir.entryInfoList(filters, QDir::Files)) {
        m_fileWatcher->addPath(info.absoluteFilePath());
    }
}

void AdvancedWebServer::addApiRoute(const QString &path,
                                  const std::function<QHttpServerResponse(const QHttpServerRequest &)> &handler)
{
    m_httpServer.route(path, handler);
}

QJsonObject AdvancedWebServer::getServerStats() const
{
    QMutexLocker locker(&m_statsMutex);
    QJsonObject stats;
    stats["totalRequests"] = static_cast<qint64>(m_stats.totalRequests);
    stats["apiRequests"] = static_cast<qint64>(m_stats.apiRequests);
    stats["staticRequests"] = static_cast<qint64>(m_stats.staticRequests);
    stats["errors"] = static_cast<qint64>(m_stats.errors);
    stats["isRunning"] = m_isRunning;
    stats["port"] = m_port;
    
    return stats;
}
