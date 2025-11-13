#include "core.h"

#include "requestmanager.h"
#include "responsemanager.h"
#include "streammanager.h"
#include "wcydeviceservice.h"
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
    // executeScript(SCRIPT_ON_START);
    startSensorServer();
    // });
    // m_startTimer->start(7000);

    _devService = new WCYDeviceService();

    QObject::connect(_devService, &IDeviceService::scanFinished, [](IDeviceService* source) {
        // auto devList = source->getCurrentConnectDevice();
        // for (auto dev: devList) {
        //     source->deviceStartStream(dev.deviceMac);
        // }
    });

    QObject::connect(_devService, &IDeviceService::updatedeviceStatus, [this](IDeviceService* source, DeviceInfo deviceInfo) {
        auto dtoToSend = convertInfoToDTO(deviceInfo);
        this->send(dtoToSend);

        if (deviceInfo.connectState == CONNECT_STATE::StateConnected) {
            source->deviceStartStream(deviceInfo.deviceMac);
        } else {
            source->deviceDisconnect(deviceInfo.deviceMac);
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

void Core::stop()
{
    if (_devService == nullptr) {
        qDebug() << "Core stopped (no device service)";
        emit stopCompleted(); // Optional: emit signal when done
    }

    _devService->disconnect();

    _devService->closeAll();

    // Non-blocking delay using QTimer
    QTimer::singleShot(3000, [this]() {
        _devService->exit();
        qDebug() << "Device service cleanup completed";
        stopSensorServer();
        emit stopCompleted(); // Optional: emit signal when done
    });

    _devService->deleteLater();
    _devService = nullptr;
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
}

long Core::executeScript(const QString& script)
{
#ifndef Q_OS_WIN
    QString scriptPath = QDir::homePath() + "/projects/scripts/" + script + ".bat";
#else
    QString scriptPath = QCoreApplication::applicationDirPath() + "/" + script + ".bat";
#endif

    QFileInfo scriptInfo(scriptPath);

    if (!scriptInfo.exists()) {
        qDebug() << "Error: Script file does not exist at" << scriptPath;
        return _return_FAIL;
    }

    qDebug() << "Running" << scriptPath << "...";

#ifdef Q_OS_WIN
    // Запускаем процесс напрямую, без cmd.exe
    _process.start(scriptPath);
#else
    _process.start("bash", QStringList() << scriptPath);
#endif

    if (!_process.waitForStarted(5000)) {
        qDebug() << "Error: Failed to start script";
        return _return_FAIL;
    }

    qDebug() << "Script" << scriptPath << "started successfully";
    qDebug() << "Process PID:" << _process.processId();

    // Не ждем завершения, так как процесс должен работать постоянно
    // Подключаем сигналы для отслеживания состояния
    QObject::connect(&_process, &QProcess::readyReadStandardOutput, [this]() {
        qDebug() << "Script stdout:" << _process.readAllStandardOutput();
    });

    QObject::connect(&_process, &QProcess::readyReadStandardError, [this]() {
        qDebug() << "Script stderr:" << _process.readAllStandardError();
    });

    QObject::connect(&_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     [](int exitCode, QProcess::ExitStatus exitStatus) {
                         qDebug() << "Script finished with exit code:" << exitCode
                                  << "exit status:" << exitStatus;
                     });

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

    QJsonArray devices = QJsonArray();
    // Вольтметр
    QJsonArray units;
    QJsonArray channels;

    for (int ii = 0; ii < DeviceInfo::MAX_CHANNELS; ii++) {
        auto ch = dev.channels[ii];
        if (ch.id == 0) break;

        for (int jj = 0; jj < ChannelInfo::MAX_UNITS; jj++) {
            int unitId = ch.availableUnits[jj];
            if (unitId == 0) break;

            units << ch.availableUnits[jj];
        }

        channels.append(DeviceGenerator::createChannel(ch.id, ch.symbol, ch.name, ch.baseUnit, units));
    }

    devices.append(DeviceGenerator::generateSensor(
        dev.deviceMac, dev.deviceType, dev.deviceId, dev.deviceName,dev.displayType, true, channels));

    root["data"] = devices;

    return root;
}

QJsonObject Core::convertFlowToDTO(DataFlow data) {

    QJsonObject root;
    root["type"] = "sensor_data";

    QJsonObject dataObj;
    dataObj["address"] = data.deviceMac;

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

long Core::startSensorServer()
{
#ifndef QT_DEBUG
    stopProcessByNameDirect("WCYSensorRPCApi.exe");
#endif

    QString exePath = QCoreApplication::applicationDirPath() + "/WCY-SDK/WCYSensorRPCApi.exe";
    QFileInfo exeInfo(exePath);

    if (!exeInfo.exists()) {
        qDebug() << "Error: WCYSensorRPCApi.exe not found at" << exePath;
        return _return_FAIL;
    }

    qDebug() << "Starting WCYSensorRPCApi.exe from" << exePath;

    if (_process.state() == QProcess::Running) {
        qDebug() << "Server WCYSensorRPCApi.exe is already running";
        return _return_OK;
    }

    _process.setWorkingDirectory(QCoreApplication::applicationDirPath() + "/WCY-SDK");
    _process.start(exePath);

    if (!_process.waitForStarted(5000)) {
        qDebug() << "Error: Failed to start WCYSensorRPCApi.exe";
        qDebug() << "Error string:" << _process.errorString();
        return _return_FAIL;
    }

    qDebug() << "WCYSensorRPCApi.exe started successfully";
    qDebug() << "Process PID:" << _process.processId();

    // Подключаем обработчики для чтения вывода
    QObject::connect(&_process, &QProcess::readyReadStandardOutput, [this]() {
        QString output = _process.readAllStandardOutput();
        qDebug() << "WCYSensorRPCApi:" << output.trimmed();
    });

    QObject::connect(&_process, &QProcess::readyReadStandardError, [this]() {
        QString error = _process.readAllStandardError();
        qDebug() << "WCYSensorRPCApi ERROR:" << error.trimmed();
    });

    QObject::connect(&_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     [](int exitCode, QProcess::ExitStatus exitStatus) {
                         qDebug() << "WCYSensorRPCApi.exe finished with exit code:" << exitCode;

                     });

    return _return_OK;
}

long Core::stopSensorServer()
{
    if (_process.state() == QProcess::Running) {
        qDebug() << "Stopping WCYSensorRPCApi.exe...";
        _process.terminate();

        if (!_process.waitForFinished(1000)) {
            qDebug() << "Force killing WCYSensorRPCApi.exe...";
            _process.kill();
        }

        qDebug() << "WCYSensorRPCApi.exe stopped";



        return _return_OK;
    }
    return _return_FAIL;
}

bool Core::stopProcessByNameDirect(const QString& processName)
{
    QProcess killProcess;
    killProcess.start("taskkill", QStringList() << "/IM" << processName << "/F");

    if (killProcess.waitForFinished(5000)) {
        qDebug() << "Process" << processName << "stopped";
        return true;
    } else {
        qDebug() << "Failed to stop process" << processName;
        return false;
    }
}
