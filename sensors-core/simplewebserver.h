#ifndef SIMPLEWEBSERVER_H
#define SIMPLEWEBSERVER_H

#include <QObject>
#include <QHttpServer>
#include <QTcpServer>
#include <QJsonObject>

class SimpleWebServer : public QObject
{
    Q_OBJECT

public:
    explicit SimpleWebServer(QObject *parent = nullptr);
    ~SimpleWebServer();

    bool startServer(quint16 port = 3000);
    void stopServer();
    bool isRunning() const { return m_isRunning; }
    quint16 getPort() const { return m_port; }

    void setWebRoot(const QString &path) { m_webRoot = path; }
    QString getWebRoot() const { return m_webRoot; }

signals:
    void serverStarted(quint16 port);
    void serverStopped();
    void requestReceived(const QString &method, const QString &path);

private:
    void setupRoutes();
    bool validateWebRoot();
    QHttpServerResponse serveStaticFile(const QString &filePath);
    QString detectContentType(const QString &filePath) const;
    QHttpServerResponse createJsonResponse(const QJsonObject &json);

    QHttpServer m_httpServer;
    QTcpServer *m_tcpServer;
    quint16 m_port;
    bool m_isRunning;
    QString m_webRoot;
};

#endif // SIMPLEWEBSERVER_H
