#include "core.h"

#include "qforeach.h"
#include "requestmanager.h"
#include "responsemanager.h"
#include "streammanager.h"
#include "wcydeviceservice.h";
#include "device_generator.h"

#include <QObject>
#include <QtWebSockets>
#include <QtCore>
#include <QtConcurrent/QtConcurrent>


const QString CMD_SYSTEM_INIT = "system_init";
const QString SCRIPT_ON_START = "start";

Core::Core(): BaseReqHandler()
{
}

Core::~Core()
{
    delete m_cmdServer;
    delete m_streamServer;
}

int Core::handle(const QJsonObject &request)
{
    QJsonObject cmdObj = request.value("cmd").toObject();
    QString cmdName = cmdObj.value("name").toString();

    if (cmdName == CMD_SYSTEM_INIT) {
        handleSystemInit(request);
        return 1;
    }

    return BaseReqHandler::handle(request);
}

void Core::handleSystemInit(const QJsonObject& request)
{
    if (m_isActivated) return;

    int requestId = request.value("request_id").toInt();
    if (requestId <= 0) return;

    m_isActivated = true;

    onSystemInit();

    QJsonObject response = createEmptyResponse(requestId);
    send(response);

    return;
}

void Core::init()
{

    RequestManager::instance()->registerHandler(this);
    ResponseManager::instance()->registerHandler(this);
    StreamManager::instance()->registerHandler(this);

    // m_cmdServer = new SocketServer(1235);
    // m_cmdServer->setRequestManager(RequestManager::instance());
    // m_cmdServer->setResponseManager(ResponseManager::instance());
    // m_cmdServer->start();

    m_streamServer = new SocketServer(8082);
    m_streamServer->setRequestManager(RequestManager::instance());
    m_streamServer->setResponseManager(StreamManager::instance());
    m_streamServer->start();

    // m_startTimer = new QTimer(this);
    // m_startTimer->setSingleShot(true);
    // QObject::connect(m_startTimer, &QTimer::timeout, [this]() {
    //     executeScript(SCRIPT_ON_START);
    // });
    // m_startTimer->start(7000);

    _devService = new WCYDeviceService();

    QObject::connect(_devService, &IDeviceService::scanFinished, [](IDeviceService* source) {
        auto devList = source->getCurrentConnectDevice();
        for (auto dev: devList) {
            source->deviceStartStream(dev.deviceMac.c_str());
        }
    });

    QObject::connect(_devService, &IDeviceService::updatedeviceStatus, [this](IDeviceService* source, DeviceInfo deviceInfo) {
        auto dtoToSend = convertInfoToDTO(deviceInfo);
        this->send(dtoToSend);
        source->clearDevice();

        if (deviceInfo.connectState == CONNECT_STATE::StateConnected) {
            source->deviceStartStream(deviceInfo.deviceMac.c_str());
        }
    });

    QObject::connect(_devService, &IDeviceService::dataRecieve, [this](DataFlow data) {
        QJsonObject dtoToSend = convertFlowToDTO(data);
        QList<QJsonObject> dto_list;
        dto_list.append(dtoToSend);

        emit stream(dto_list);
    });
}

void Core::start()
{
//      QMutexLocker locker(&m_mutex);

//  _devService->startScan();

// #ifdef __linux__
//     if (!m_threadFuture.isRunning()) {
//         m_threadFuture = QtConcurrent::run(this, &Core::thread_proc);
//     }
// #else
//     if (!m_threadFuture.isRunning()) {
//         m_threadFuture = QtConcurrent::run(&Core::thread_proc, this);
//     }
// #endif
}

void Core::thread_proc()
{
    QThread::msleep(1000);

    while (1)
    {
        m_mutex.lock();

        QList<QJsonObject> dat_list = generateData();

        for (QJsonObject dat: dat_list) {
            QJsonDocument doc(dat);
            qDebug() << "Generating data " << doc.toJson(QJsonDocument::Compact) << '\n';
        }

        if (!dat_list.isEmpty()) {
            emit stream(dat_list);
        }

        m_mutex.unlock();

        QThread::msleep(5);
    }
}

void Core::onSystemInit()
{
    QMutexLocker locker(&m_mutex);
    qInfo() << "Send all devices Info";

    QJsonObject res = generateDevicesUpdate();
    this->send(res);

    _devService->startScan();

//  TestSensorRPC();
}

void Core::TestSensorRPC()
{
    qInfo() << "Testing RPC call to WCY Sensor API:";

    if (_wcyAPI) return;

    _wcyAPI = std::make_unique<WCYServiceWrapper>(this);

    // Подключаем сигналы для обработки событий
    QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::messageReceived, [](const QString &msg) {
        qInfo().noquote() << "[INFO]" << msg;
    });

    QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::errorOccurred, [](const QString &error) {
        qCritical().noquote() << "[ERROR]" << error;
    });

    QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::connectedChanged, [this](bool connected) {
        if (connected) {
            //  client->scanBLE(5000); // 5 секунд сканирования
            _wcyAPI->scanUSB();
        } else {
            qCritical().noquote() << "[ERROR]" << "server is disconnected";
        }
    });

    QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::devScanFinished, [this](const QList<DeviceModel> &devices, const QString &error) {
        qInfo().noquote() << "Found devices: " << devices.count();
        for(auto dev: devices) {
            qInfo().noquote() << "[INFO]" << dev.deviceName() << "MAC: " << dev.deviceMac();
        }

        if (!_connectedDevice.deviceName().isEmpty() || devices.count() == 0) {
            return;
        }


        _connectedDevice = devices.first();

        _wcyAPI->connectDevice(_connectedDevice.deviceMac());
    });

    QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::deviceConnected, [this](ConnectEnumGadget::ConnectEnum result, const QString &error) {
        if (result == ConnectEnumGadget::ConnectEnum::SUCCESSFUL)  {
            _wcyAPI->getSensorRange(_connectedDevice.deviceMac());
        }
    });

    QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::sensorRangeReceived, [this](QList<DataTypeModel> ranges, const QString &error) {
        QList<int32> ranges_idList;
        for (const auto r: ranges) {
            ranges_idList.append(r.rangeId());
        }

        _wcyAPI->startSendData(_connectedDevice.deviceMac(), ranges_idList);
        _wcyAPI->subscribeToSensorData(_connectedDevice.deviceMac());
        _wcyAPI->subscribeToSensorEvents(_connectedDevice.deviceMac());
    });

    QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::sensorDataReceived, [](const QList<SensorDataModel> &sensorData) {

        if (sensorData.empty()) return;
        qInfo() << "[STREAM]: " << "range id" << sensorData.first().rangeId() << ", value" << sensorData.first().dataValue();
    });

    QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::sensorEventReceived, [](const QString &deviceMac, const QString &eventType, const QString &message) {
        qInfo() << "[INFO]: " << "device " << deviceMac << "event" << eventType << "message" << message;
    });

    // Вызов методов
    _wcyAPI->connectToServer("http://localhost:5001");
}

long Core::executeScript(const QString& script)
{

#ifndef Q_OS_WIN
    QString scriptPath = QDir::homePath() + "/projects/scripts/" + script + ".sh";
#else
    QString scriptPath = QCoreApplication::applicationDirPath() + "/" + script + ".bat";
#endif


    QFileInfo scriptInfo(scriptPath);

    if (!scriptInfo.exists()) {
        qDebug() << "Error: Script file does not exist at" << script;
        return _return_FAIL;
    }

#ifndef Q_OS_WIN
    if (!scriptInfo.isExecutable()) {
        qDebug() << "Error: Script is not executable. Run: chmod +x" << scriptPath;
        return _return_FAIL;
    }
#endif

    QProcess process;
//    process.setProcessChannelMode(QProcess::MergedChannels); // Combine stdout and stderr

    static QString SCRIPT_NAME = "";
    if (SCRIPT_NAME != script) {
        qDebug() << "Running " << scriptPath << "...";
    }

#ifdef Q_OS_WIN
    process.start("cmd.exe", QStringList() << "/C" << scriptPath);
#else
    // Make sure script has execute permissions
    process.start("bash", QStringList() << scriptPath);
#endif

    if (!process.waitForStarted()) {
        qDebug() << "Error: Failed to start script";
        return _return_FAIL;
    }

    if (!process.waitForFinished(30000)) {
        qDebug() << "Error: Process timed out";
        process.kill();
        return _return_FAIL;
    }

    QString allOutput = QString::fromUtf8(process.readAll());
    int exitCode = process.exitCode();

    if (exitCode != 0) {
        qDebug() << "Error: Script failed with exit code" << exitCode;
        qDebug() << "Output:" << allOutput;
        return _return_FAIL;
    }

    if (SCRIPT_NAME != script) {
        qDebug() << "Script" << scriptPath << "executed successfully";
    }

    //  qDebug() << "Output:" << allOutput;
    SCRIPT_NAME = script;

    return _return_OK;
}

QJsonObject Core::generateDevicesUpdate() {

    QJsonObject root;
    root["type"] = "devices_update";

    _devices = QJsonArray();
    // Вольтметр
    QJsonArray voltUnits;
    voltUnits << 2 << 3;
    QJsonArray voltChannels;
    voltChannels.append(DeviceGenerator::createChannel(1, "U", "voltage", 3, voltUnits));

    _devices.append(DeviceGenerator::generateSensor(
        "38:3B:26:E8:A3:3A", "voltmeter", "A3A3", "VM-1001_A3A3",
        "вольтметр", true, voltChannels
        ));

    // Амперметр
    QJsonArray ampUnits;
    ampUnits << 0 << 1 << 7;
    QJsonArray ampChannels;
    ampChannels.append(DeviceGenerator::createChannel(18, "f", "f", 1, ampUnits));

    _devices.append(DeviceGenerator::generateSensor(
        "38:3B:26:E8:A3:3D", "frequency", "3B38", "A2-1009_3B38",
        "амперметр", true, ampChannels
        ));

    // Датчик давления
    QJsonArray pressureUnits;
    pressureUnits << 6 << 7;
    QJsonArray tempUnits;
    tempUnits << 11;

    QJsonArray pressureChannels;
    pressureChannels.append(DeviceGenerator::createChannel(10, "P", "pressure", 7, pressureUnits));
    pressureChannels.append(DeviceGenerator::createChannel(11, "T", "temperature", 11, tempUnits));

    _devices.append(DeviceGenerator::generateSensor(
        "38:3B:26:A2:1E:85", "pressure", "8585", "PM-020_8585",
        "датчик давления", false, pressureChannels
        ));

    root["data"] = _devices;
    return root;
}

QJsonObject Core::convertInfoToDTO(DeviceInfo dev) {

    QJsonObject root;
    root["type"] = "devices_update";

    _devices = QJsonArray();
    // Вольтметр
    QJsonArray units;
    QJsonArray channels;

    for (int ii = 0; ii < 3; ii++) {
        auto ch = dev.channels[ii];
        if (ch.id == 0) break;

        for (int jj = 0; jj < 10; jj++) {
            int unitId = ch.availableUnits[jj];
            if (unitId > 0) {
                units << ch.availableUnits[jj];
            }
        }

        channels.append(DeviceGenerator::createChannel(ch.id, ch.symbol.c_str(), ch.name.c_str(), ch.baseUnit, units));
    }

    _devices.append(DeviceGenerator::generateSensor(
        dev.deviceMac.c_str(), dev.deviceType.c_str(), dev.deviceId.c_str(), dev.deviceName.c_str(),dev.displayType.c_str(), true, channels));

    root["data"] = _devices;

    return root;
}

QJsonObject Core::convertFlowToDTO(DataFlow data) {

    QJsonObject root;
    root["type"] = "sensor_data";

    QJsonObject dataObj;
    dataObj["address"] = data.deviceMac.c_str();

    QJsonObject sensorData;

    // Текущее время в формате timestamp
    qint64 timestamp = data.timestamp;
    sensorData["0"] = timestamp;

    for (const auto& pair : data.values) {
        sensorData[QString::number(pair.first)] = pair.second;
    }

    dataObj["data"] = sensorData;

    root["data"] = dataObj;
    return root;
}

QList<QJsonObject> Core::generateData() {

    QList<QJsonObject> sensorsDataArray;
    // Проходим по всем устройствам в массиве _devices
    for (const QJsonValue& deviceValue : _devices) {
        QJsonObject root;
        root["type"] = "sensor_data";

        QJsonObject device = deviceValue.toObject();

        // Генерируем данные только для подключенных датчиков
        bool connected = device["connected"].toBool();
        if (!connected) {
            continue;
        }

        QString address = device["address"].toString();
        QJsonArray channels = device["channels"].toArray();

        // Генерируем данные для этого датчика
        QJsonObject sensorData = DeviceGenerator::generateSensorData(address, channels);
        root["data"] = sensorData;

        sensorsDataArray.append(sensorData);
    }

    return sensorsDataArray;
}
