#include "sensorserviceclient.h"

#include <QtCore/QUrl>
#include <QtGrpc/QGrpcChannelOptions>
#include <QtGrpc/QGrpcHttp2Channel>
#include <QtCore/QDebug>

using namespace std::chrono_literals;

WCYServiceWrapper::WCYServiceWrapper(QObject *parent)
    : QObject(parent)
    , m_connected(false)
{
    m_channelOptions = QGrpcChannelOptions();
}

WCYServiceWrapper::~WCYServiceWrapper()
{
    disconnectFromServer();
}

bool WCYServiceWrapper::initializeClient(const QString &address)
{
    QUrl m_hostUri(address);
    if (!m_hostUri.isValid()) {
        emit errorOccurred(QString("Invalid address: %1").arg(address));
        return false;
    }

    QGrpcChannelOptions opts = m_channelOptions;

    m_client = std::make_unique<SensorService::Client>();
    if (!m_client->attachChannel(std::make_shared<QGrpcHttp2Channel>(m_hostUri, opts))) {
        qWarning() << "Failed to attach channel to SensorService client";
        m_client.reset();
        return false;
    }

    return true;
}

void WCYServiceWrapper::connectToServer(const QString &address)
{
    if (m_connected) return;

    try {
        if (!initializeClient(address)) {
            emit errorOccurred(QString("Failed to initialize SensorService client for %1").arg(address));
            return;
        }

        m_connected = true;
        emit connectedChanged(true);
        emit messageReceived("Successfully connected to SensorService server: " + address);

        qDebug() << "Connected to SensorService gRPC server at:" << address;

    } catch (const std::exception &e) {
        m_connected = false;
        emit connectedChanged(false);
        emit errorOccurred(QString("Connection error: %1").arg(e.what()));
        qCritical() << "Connection error:" << e.what();
    }
}

void WCYServiceWrapper::disconnectFromServer()
{
    if (m_sensorDataStream) {
        m_sensorDataStream.reset();
    }

    if (m_sensorEventsStream) {
        m_sensorEventsStream.reset();
    }

    if (m_client) {
        m_client.reset();
    }

    m_connected = false;
    emit connectedChanged(false);
    emit messageReceived("Disconnected from SensorService server");
    qDebug() << "Disconnected from SensorService server";
}

// Scanning methods
void WCYServiceWrapper::scanBLE(int32_t timeout)
{
    if (!m_connected || !m_client) {
        emit errorOccurred("Client not connected to server");
        return;
    }

    try {
        BLEScanRequest request;
        request.setTimeout(timeout);

        auto reply = m_client->BLEScan(request, QGrpcCallOptions{}.setDeadlineTimeout(20s));
        const auto *replyPtr = reply.get();

        connect(replyPtr, &QGrpcCallReply::finished, this,
            [this, reply = std::move(reply)](const QGrpcStatus &status) mutable {
                if (status.isOk()) {
                    BLEScanResponse response;
                    if (reply->read(&response)) {
                        emit devScanFinished(response.devices(), response.error());
                    } else {
                        emit errorOccurred("Failed to deserialize BLEScan response");
                    }
                } else {
                    emit errorOccurred(QString("RPC error: %1").arg(status.message()));
                }
            },
            Qt::SingleShotConnection);

    } catch (const std::exception &e) {
        emit errorOccurred(QString("Exception in scanBLE: %1").arg(e.what()));
    }
}

void WCYServiceWrapper::scanUSB()
{
    if (!m_connected || !m_client) {
        emit errorOccurred("Client not connected to server");
        return;
    }

    try {
        USBScanRequest request;

        auto reply = m_client->USBScan(request, QGrpcCallOptions{}.setDeadlineTimeout(20s));
        const auto *replyPtr = reply.get();

        connect(replyPtr, &QGrpcCallReply::finished, this,
            [this, reply = std::move(reply)](const QGrpcStatus &status) mutable {
                if (status.isOk()) {
                    USBScanResponse response;
                    if (reply->read(&response)) {
                        emit devScanFinished(response.devices(), response.error());
                    } else {
                        emit errorOccurred("Failed to deserialize USBScan response");
                    }
                } else {
                    emit errorOccurred(QString("RPC error: %1").arg(status.message()));
                }
            },
            Qt::SingleShotConnection);

    } catch (const std::exception &e) {
        emit errorOccurred(QString("Exception in scanUSB: %1").arg(e.what()));
    }
}

// Connection methods
void WCYServiceWrapper::connectDevice(const QString &mac)
{
    if (!m_connected || !m_client) {
        emit errorOccurred("Client not connected to server");
        return;
    }

    try {
        ConnectRequest request;
        request.setMac(mac);

        auto reply = m_client->Connect(request, QGrpcCallOptions{}.setDeadlineTimeout(15s));
        const auto *replyPtr = reply.get();

        connect(replyPtr, &QGrpcCallReply::finished, this,
            [this, mac, reply = std::move(reply)](const QGrpcStatus &status) mutable {
                if (status.isOk()) {
                    ConnectResponse response;
                    if (reply->read(&response)) {
                        emit deviceConnected(mac, response.result());
                    } else {
                        emit errorOccurred("Failed to deserialize Connect response");
                    }
                } else {
                    emit errorOccurred(QString("RPC error: %1").arg(status.message()));
                }
            },
            Qt::SingleShotConnection);

    } catch (const std::exception &e) {
        emit errorOccurred(QString("Exception in connectDevice: %1").arg(e.what()));
    }
}

void WCYServiceWrapper::disconnectDevice(const QString &deviceMac)
{
    if (!m_connected || !m_client) {
        emit errorOccurred("Client not connected to server");
        return;
    }

    try {
        DisconnectRequest request;
        request.setDeviceMac(deviceMac);

        auto reply = m_client->Disconnect(request, QGrpcCallOptions{}.setDeadlineTimeout(10s));
        const auto *replyPtr = reply.get();

        connect(replyPtr, &QGrpcCallReply::finished, this,
            [this, reply = std::move(reply)](const QGrpcStatus &status) mutable {
                if (status.isOk()) {
                    DisconnectResponse response;
                    if (reply->read(&response)) {
                        emit deviceDisconnected(response.success(), response.error());
                    } else {
                        emit errorOccurred("Failed to deserialize Disconnect response");
                    }
                } else {
                    emit errorOccurred(QString("RPC error: %1").arg(status.message()));
                }
            },
            Qt::SingleShotConnection);

    } catch (const std::exception &e) {
        emit errorOccurred(QString("Exception in disconnectDevice: %1").arg(e.what()));
    }
}

void WCYServiceWrapper::deviceExit(const QString &deviceMac)
{
    if (!m_connected || !m_client) {
        emit errorOccurred("Client not connected to server");
        return;
    }

    try {
        DeviceExitRequest request;
        request.setDeviceMac(deviceMac);

        auto reply = m_client->DeviceExit(request, QGrpcCallOptions{}.setDeadlineTimeout(10s));
        const auto *replyPtr = reply.get();

        connect(replyPtr, &QGrpcCallReply::finished, this,
            [this, reply = std::move(reply)](const QGrpcStatus &status) mutable {
                if (status.isOk()) {
                    DeviceExitResponse response;
                    if (reply->read(&response)) {
                        emit deviceExited(response.success(), response.error());
                    } else {
                        emit errorOccurred("Failed to deserialize DeviceExit response");
                    }
                } else {
                    emit errorOccurred(QString("RPC error: %1").arg(status.message()));
                }
            },
            Qt::SingleShotConnection);

    } catch (const std::exception &e) {
        emit errorOccurred(QString("Exception in deviceExit: %1").arg(e.what()));
    }
}

// Data operations
void WCYServiceWrapper::getSensorRange(const QString &deviceMac)
{
    if (!m_connected || !m_client) {
        emit errorOccurred("Client not connected to server");
        return;
    }

    try {
        GetSensorRangeRequest request;
        request.setDeviceMac(deviceMac);

        auto reply = m_client->GetSensorRange(request, QGrpcCallOptions{}.setDeadlineTimeout(10s));
        const auto *replyPtr = reply.get();

        connect(replyPtr, &QGrpcCallReply::finished, this,
            [this, deviceMac, reply = std::move(reply)](const QGrpcStatus &status) mutable {
                if (status.isOk()) {
                    GetSensorRangeResponse response;
                    if (reply->read(&response)) {
                        emit sensorRangeReceived(deviceMac, response.ranges());
                    } else {
                        emit errorOccurred("Failed to deserialize GetSensorRange response");
                    }
                } else {
                    emit errorOccurred(QString("RPC error: %1").arg(status.message()));
                }
            },
            Qt::SingleShotConnection);

    } catch (const std::exception &e) {
        emit errorOccurred(QString("Exception in getSensorRange: %1").arg(e.what()));
    }
}

void WCYServiceWrapper::startSendData(const QString &deviceMac, const QList<int32_t> &rangeIdList)
{
    if (!m_connected || !m_client) {
        emit errorOccurred("Client not connected to server");
        return;
    }

    QtProtobuf::int32List rangeIdList_proto;
    for (auto v: rangeIdList) {
        rangeIdList_proto.append(v);
    }

    try {
        StartSendDataRequest request;
        request.setDeviceMac(deviceMac);
        request.setRangeIdList(rangeIdList_proto);

        auto reply = m_client->StartSendData(request, QGrpcCallOptions{}.setDeadlineTimeout(10s));
        const auto *replyPtr = reply.get();

        connect(replyPtr, &QGrpcCallReply::finished, this,
            [this, reply = std::move(reply)](const QGrpcStatus &status) mutable {
                if (status.isOk()) {
                    StartSendDataResponse response;
                    if (reply->read(&response)) {
                        emit startSendDataFinished(response.success(), response.error());
                    } else {
                        emit errorOccurred("Failed to deserialize StartSendData response");
                    }
                } else {
                    emit errorOccurred(QString("RPC error: %1").arg(status.message()));
                }
            },
            Qt::SingleShotConnection);

    } catch (const std::exception &e) {
        emit errorOccurred(QString("Exception in startSendData: %1").arg(e.what()));
    }
}

void WCYServiceWrapper::stopSendData(const QString &deviceMac)
{
    if (!m_connected || !m_client) {
        emit errorOccurred("Client not connected to server");
        return;
    }

    try {
        StopSendDataRequest request;
        request.setDeviceMac(deviceMac);

        auto reply = m_client->StopSendData(request, QGrpcCallOptions{}.setDeadlineTimeout(10s));
        const auto *replyPtr = reply.get();

        connect(replyPtr, &QGrpcCallReply::finished, this,
            [this, reply = std::move(reply)](const QGrpcStatus &status) mutable {
                if (status.isOk()) {
                    StopSendDataResponse response;
                    if (reply->read(&response)) {
                        emit stopSendDataFinished(response.success(), response.error());
                    } else {
                        emit errorOccurred("Failed to deserialize StopSendData response");
                    }
                } else {
                    emit errorOccurred(QString("RPC error: %1").arg(status.message()));
                }
            },
            Qt::SingleShotConnection);

    } catch (const std::exception &e) {
        emit errorOccurred(QString("Exception in stopSendData: %1").arg(e.what()));
    }
}

void WCYServiceWrapper::clearZero(const QString &deviceMac, const QList<int32_t> &rangeIdList)
{
    if (!m_connected || !m_client) {
        emit errorOccurred("Client not connected to server");
        return;
    }

    try {
        QtProtobuf::int32List rangeIdList_proto;
        for (auto v: rangeIdList) {
            rangeIdList_proto.append(v);
        }

        ClearZeroRequest request;
        request.setDeviceMac(deviceMac);
        request.setRangeIdList(rangeIdList_proto);

        auto reply = m_client->ClearZero(request, QGrpcCallOptions{}.setDeadlineTimeout(10s));
        const auto *replyPtr = reply.get();

        connect(replyPtr, &QGrpcCallReply::finished, this,
            [this, reply = std::move(reply)](const QGrpcStatus &status) mutable {
                if (status.isOk()) {
                    ClearZeroResponse response;
                    if (reply->read(&response)) {
                        emit clearZeroFinished(response.success(), response.error());
                    } else {
                        emit errorOccurred("Failed to deserialize ClearZero response");
                    }
                } else {
                    emit errorOccurred(QString("RPC error: %1").arg(status.message()));
                }
            },
            Qt::SingleShotConnection);

    } catch (const std::exception &e) {
        emit errorOccurred(QString("Exception in clearZero: %1").arg(e.what()));
    }
}

// Configuration methods
void WCYServiceWrapper::setSensorLang(const QString &deviceMac, int32_t langType)
{
    if (!m_connected || !m_client) {
        emit errorOccurred("Client not connected to server");
        return;
    }

    try {
        SetSensorLangRequest request;
        request.setDeviceMac(deviceMac);
        request.setLangType(langType);

        auto reply = m_client->SetSensorLang(request, QGrpcCallOptions{}.setDeadlineTimeout(10s));
        const auto *replyPtr = reply.get();

        connect(replyPtr, &QGrpcCallReply::finished, this,
            [this, reply = std::move(reply)](const QGrpcStatus &status) mutable {
                if (status.isOk()) {
                    SetSensorLangResponse response;
                    if (reply->read(&response)) {
                        emit sensorLangSet(response.success(), response.error());
                    } else {
                        emit errorOccurred("Failed to deserialize SetSensorLang response");
                    }
                } else {
                    emit errorOccurred(QString("RPC error: %1").arg(status.message()));
                }
            },
            Qt::SingleShotConnection);

    } catch (const std::exception &e) {
        emit errorOccurred(QString("Exception in setSensorLang: %1").arg(e.what()));
    }
}

void WCYServiceWrapper::getSwitchRange(const QString &deviceMac)
{
    if (!m_connected || !m_client) {
        emit errorOccurred("Client not connected to server");
        return;
    }

    try {
        GetSwitchRangeRequest request;
        request.setDeviceMac(deviceMac);

        auto reply = m_client->GetSwitchRange(request, QGrpcCallOptions{}.setDeadlineTimeout(10s));
        const auto *replyPtr = reply.get();

        connect(replyPtr, &QGrpcCallReply::finished, this,
            [this, reply = std::move(reply)](const QGrpcStatus &status) mutable {
                if (status.isOk()) {
                    GetSwitchRangeResponse response;
                    if (reply->read(&response)) {
                        emit switchRangeReceived(response.ranges(), response.error());
                    } else {
                        emit errorOccurred("Failed to deserialize GetSwitchRange response");
                    }
                } else {
                    emit errorOccurred(QString("RPC error: %1").arg(status.message()));
                }
            },
            Qt::SingleShotConnection);

    } catch (const std::exception &e) {
        emit errorOccurred(QString("Exception in getSwitchRange: %1").arg(e.what()));
    }
}

void WCYServiceWrapper::setRange(const QString &deviceMac, int32_t rangeId)
{
    if (!m_connected || !m_client) {
        emit errorOccurred("Client not connected to server");
        return;
    }

    try {
        SetRangeRequest request;
        request.setDeviceMac(deviceMac);
        request.setRangeId(rangeId);

        auto reply = m_client->SetRange(request, QGrpcCallOptions{}.setDeadlineTimeout(10s));
        const auto *replyPtr = reply.get();

        connect(replyPtr, &QGrpcCallReply::finished, this,
            [this, reply = std::move(reply)](const QGrpcStatus &status) mutable {
                if (status.isOk()) {
                    SetRangeResponse response;
                    if (reply->read(&response)) {
                        emit rangeSet(response.success(), response.error());
                    } else {
                        emit errorOccurred("Failed to deserialize SetRange response");
                    }
                } else {
                    emit errorOccurred(QString("RPC error: %1").arg(status.message()));
                }
            },
            Qt::SingleShotConnection);

    } catch (const std::exception &e) {
        emit errorOccurred(QString("Exception in setRange: %1").arg(e.what()));
    }
}

void WCYServiceWrapper::getCurrentFrequency(const QString &deviceMac)
{
    if (!m_connected || !m_client) {
        emit errorOccurred("Client not connected to server");
        return;
    }

    try {
        GetCurrentFrequencyRequest request;
        request.setDeviceMac(deviceMac);

        auto reply = m_client->GetCurrentFrequency(request, QGrpcCallOptions{}.setDeadlineTimeout(10s));
        const auto *replyPtr = reply.get();

        connect(replyPtr, &QGrpcCallReply::finished, this,
            [this, reply = std::move(reply)](const QGrpcStatus &status) mutable {
                if (status.isOk()) {
                    GetCurrentFrequencyResponse response;
                    if (reply->read(&response)) {
                        emit currentFrequencyReceived(response.frequency(), response.error());
                    } else {
                        emit errorOccurred("Failed to deserialize GetCurrentFrequency response");
                    }
                } else {
                    emit errorOccurred(QString("RPC error: %1").arg(status.message()));
                }
            },
            Qt::SingleShotConnection);

    } catch (const std::exception &e) {
        emit errorOccurred(QString("Exception in getCurrentFrequency: %1").arg(e.what()));
    }
}

void WCYServiceWrapper::getSupportFrequency(const QString &deviceMac)
{
    if (!m_connected || !m_client) {
        emit errorOccurred("Client not connected to server");
        return;
    }

    try {
        GetSupportFrequencyRequest request;
        request.setDeviceMac(deviceMac);

        auto reply = m_client->GetSupportFrequency(request, QGrpcCallOptions{}.setDeadlineTimeout(10s));
        const auto *replyPtr = reply.get();

        connect(replyPtr, &QGrpcCallReply::finished, this,
            [this, reply = std::move(reply)](const QGrpcStatus &status) mutable {
                if (status.isOk()) {
                    GetSupportFrequencyResponse response;
                    if (reply->read(&response)) {
                        emit supportFrequencyReceived(response.frequencies(), response.error());
                    } else {
                        emit errorOccurred("Failed to deserialize GetSupportFrequency response");
                    }
                } else {
                    emit errorOccurred(QString("RPC error: %1").arg(status.message()));
                }
            },
            Qt::SingleShotConnection);

    } catch (const std::exception &e) {
        emit errorOccurred(QString("Exception in getSupportFrequency: %1").arg(e.what()));
    }
}

void WCYServiceWrapper::setFrequency(const QString &deviceMac, int32_t frequency)
{
    if (!m_connected || !m_client) {
        emit errorOccurred("Client not connected to server");
        return;
    }

    try {
        SetFrequencyRequest request;
        request.setDeviceMac(deviceMac);
        request.setFrequency(frequency);

        auto reply = m_client->SetFrequency(request, QGrpcCallOptions{}.setDeadlineTimeout(10s));
        const auto *replyPtr = reply.get();

        connect(replyPtr, &QGrpcCallReply::finished, this,
            [this, reply = std::move(reply)](const QGrpcStatus &status) mutable {
                if (status.isOk()) {
                    SetFrequencyResponse response;
                    if (reply->read(&response)) {
                        emit frequencySet(response.success(), response.error());
                    } else {
                        emit errorOccurred("Failed to deserialize SetFrequency response");
                    }
                } else {
                    emit errorOccurred(QString("RPC error: %1").arg(status.message()));
                }
            },
            Qt::SingleShotConnection);

    } catch (const std::exception &e) {
        emit errorOccurred(QString("Exception in setFrequency: %1").arg(e.what()));
    }
}

// Streaming methods
void WCYServiceWrapper::subscribeToSensorData(const QString &deviceMac)
{
    if (!m_connected || !m_client) {
        emit errorOccurred("Client not connected to server");
        return;
    }

    try {
        SubscribeRequest request;
        request.setDeviceMac(deviceMac);

        m_sensorDataStream = m_client->SubscribeToSensorData(request, QGrpcCallOptions{});

        if (!m_sensorDataStream) {
            emit errorOccurred("Failed to start sensor data stream");
            return;
        }

        connect(m_sensorDataStream.get(), &QGrpcServerStream::messageReceived, this,
                [this]() mutable {
                    SensorDataMessage message;
                    if (!m_sensorDataStream->read(&message))
                        return;
                    emit sensorDataReceived(message.sensorData());
                });

        connect(m_sensorDataStream.get(), &QGrpcServerStream::finished, this,
                [this](const QGrpcStatus &status) {
                    if (!status.isOk()) {
                        emit errorOccurred(QString("Sensor data stream error: %1").arg(status.message()));
                    }
                });

        emit messageReceived("Sensor data stream started for device: " + deviceMac);

    } catch (const std::exception &e) {
        emit errorOccurred(QString("Exception in subscribeToSensorData: %1").arg(e.what()));
    }
}

void WCYServiceWrapper::subscribeToSensorEvents(const QString &deviceMac)
{
    if (!m_connected || !m_client) {
        emit errorOccurred("Client not connected to server");
        return;
    }

    try {
        SubscribeRequest request;
        request.setDeviceMac(deviceMac);

        m_sensorEventsStream = m_client->SubscribeToSensorEvents(request, QGrpcCallOptions{});

        if (!m_sensorEventsStream) {
            emit errorOccurred("Failed to start sensor events stream");
            return;
        }

        connect(m_sensorEventsStream.get(), &QGrpcServerStream::messageReceived, this,
                [this]() mutable {
                    SensorEventMessage message;
                    if (!m_sensorEventsStream->read(&message))
                        return;
                    emit sensorEventReceived(message.deviceMac(), message.eventType(), message.message());
                });

        connect(m_sensorEventsStream.get(), &QGrpcServerStream::finished, this,
                [this](const QGrpcStatus &status) {
                    if (!status.isOk()) {
                        emit errorOccurred(QString("Sensor events stream error: %1").arg(status.message()));
                    }
                });

        emit messageReceived("Sensor events stream started for device: " + deviceMac);

    } catch (const std::exception &e) {
        emit errorOccurred(QString("Exception in subscribeToSensorEvents: %1").arg(e.what()));
    }
}
