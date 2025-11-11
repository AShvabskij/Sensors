#ifndef ADVANCEDWEBSERVER_H
#define ADVANCEDWEBSERVER_H

#include <QObject>
#include <QHttpServer>
#include <QCache>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QJsonObject>
#include <QMutex>

class AdvancedWebServer : public QObject
{
    Q_OBJECT

public:
    struct ServerConfig {
        quint16 port = 3000;
        QString webRoot = "web";
        bool enableCaching = true;
        int cacheSize = 100; // MB
        bool liveReload = false;
        int apiTimeout = 5000; // ms
    };

    explicit AdvancedWebServer(QObject *parent = nullptr);
    ~AdvancedWebServer();

    bool startServer(const ServerConfig &config = ServerConfig());
    void stopServer();
    quint16 getPort() const { return m_port; }
    bool isRunning() const { return m_isRunning; }

    // API methods
    QJsonObject getServerStats() const;
    void addApiRoute(const QString &path, 
                     const std::function<QHttpServerResponse(const QHttpServerRequest &)> &handler);

signals:
    void serverStarted(quint16 port);
    void serverStopped();
    void requestReceived(const QString &method, const QString &path, int statusCode);
    void errorOccurred(const QString &errorMessage);

public slots:
    void clearCache();
    void reloadWebContent();

private slots:
    void onFileChanged(const QString &path);

private:
    void setupRoutes();
    void setupMiddleware();
    void setupErrorHandling();
    void initializeWebDirectory();
    void setupFileWatcher();
    
    QHttpServerResponse serveStaticFile(const QString &filePath);
    QHttpServerResponse handleApiRequest(const QHttpServerRequest &request);
    QHttpServerResponse handleWebSocketUpgrade(const QHttpServerRequest &request);
    QHttpServerResponse handleFileUpload(const QHttpServerRequest &request);
    
    QString detectContentType(const QString &filePath) const;
    QHttpServerResponse createJsonResponse(const QJsonObject &json, 
                                         QHttpServerResponse::StatusCode status = QHttpServerResponse::StatusCode::Ok);
    QHttpServerResponse createErrorResponse(QHttpServerResponse::StatusCode status, 
                                          const QString &message = "");
    
    // Cache management
    void addToCache(const QString &key, const QByteArray &data, const QString &contentType);
    bool getFromCache(const QString &key, QByteArray &data, QString &contentType);
    
    // API handlers
    QHttpServerResponse handleApiStats(const QHttpServerRequest &request);
    QHttpServerResponse handleApiEcho(const QHttpServerRequest &request);
    QHttpServerResponse handleApiSystemInfo(const QHttpServerRequest &request);

    QHttpServer m_httpServer;
    QFileSystemWatcher *m_fileWatcher;
    ServerConfig m_config;
    
    QCache<QString, QPair<QByteArray, QString>> m_fileCache;
    mutable QMutex m_cacheMutex;
    
    quint16 m_port;
    bool m_isRunning;
    
    // Statistics
    struct {
        qint64 totalRequests = 0;
        qint64 apiRequests = 0;
        qint64 staticRequests = 0;
        qint64 errors = 0;
        QHash<int, qint64> statusCodes;
        QHash<QString, qint64> endpointHits;
    } m_stats;
    mutable QMutex m_statsMutex;
};

#endif // ADVANCEDWEBSERVER_H