#include "core.h"

#include "requestmanager.h"
#include "responsemanager.h"
#include "streammanager.h"
#include "wcy_wrapper_linux.h"
#include "wcydeviceservice.h"

#include <QObject>
#include <QtWebSockets>
#include <QtCore>
#include <QtConcurrent/QtConcurrent>

#ifdef Q_OS_WIN
#include "wcydeviceservice.h"

// #include "wcygrpcdeviceservice.h"
#endif

#include "device_generator.h"
#include "demodeviceservice.h"

const QString CMD_SYSTEM_INIT = "system_init";
const QString SCRIPT_ON_START = "start";

Core::Core(): BaseReqHandler()
{
}

Core::~Core()
{
    RequestManager::instance()->remove(this);
    delete m_cmdServer;
    delete m_streamServer;

    qDebug() << "Core is destroied";
}

int Core::handle(const QJsonObject &request)
{
    QJsonObject cmdObj = request.value("cmd").toObject();
    QString cmdName = cmdObj.value("name").toString();

    if (cmdName == CMD_SYSTEM_INIT) {
        handleSystemInit(request);
        return 1;
    }

    QString action = request.value("action").toString();
    QString address = request.value("address").toString();

    if (action == "connect") {
        _devService->deviceConnect(address);
    } else if (action == "disconnect") {
        _devService->deviceDisconnect(address);
    } else if (action == "rescan") {
        _devService->startScan();
    }
    else if (action == "start") {
        if (!address.isEmpty()) {
            _devService->deviceStartStream(address);
        } else {
            auto devList = _devService->getConnectedDeviceList();
            for (const auto& dev: devList) {
                if (dev.connectState == StateConnected) {
                    _devService->deviceStartStream(dev.deviceMac);
                }
            }
        }
    }
    else if (action == "stop") {
        if (!address.isEmpty()) {
            _devService->deviceStopStream(address);
        } else {
            auto devList = _devService->getConnectedDeviceList();
            for (const auto& dev: devList) {
                if (dev.connectState == StateStarted) {
                    _devService->deviceStopStream(dev.deviceMac);
                }
            }
        }
    }

    return BaseReqHandler::handle(request);
}

void Core::handleSystemInit(const QJsonObject& request)
{
    if (m_isActivated) return;

    int requestId = request.value("request_id").toInt();
    if (requestId <= 0) return;

    int res;
    if (request.contains("sys_type_id")) {
        res = request.value("sys_type_id").toInt();
    } else {
        res = 0;
    }

    SysType sysType = (SysType)res;

    if (sysType <= SysType::SysType_Undefined || sysType >= SysType::SysType_Unknown) {
        qWarning() << "undefined system type";
        return;
    }

    m_isActivated = true;

    start(sysType);

    QJsonObject response = createEmptyResponse(requestId);
    send(response);

    return;
}

void Core::init()
{

#ifdef Q_OS_WIN
    m_supportedSysTypes.append(SysType::DEMO);
    m_supportedSysTypes.append(SysType::WCY);
#elif defined(Q_OS_LINUX)
    m_supportedSysTypes.append(SysType::DEMO);
    m_supportedSysTypes.append(SysType::WCY); // WCY поддерживается и в Linux
#else
    m_supportedSysTypes.append(SysType::DEMO);
#endif

    RequestManager::instance()->registerHandler(this); // todo: it's better to create a separate handler
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

    QTimer::singleShot(1000, [/*this*/]() {
    // executeScript(SCRIPT_ON_START);
    });

#ifdef Q_OS_WIN
    auto wcyWrapper = new WCY_Wrapper_Win();
    auto wcy = new WCYDeviceService(wcyWrapper);
    registerSensorService(SysType::WCY, wcy);
#elif defined(Q_OS_LINUX)
    auto wcyWrapper = new WCY_Wrapper_Linux();
    auto wcy = new WCYDeviceService(wcyWrapper);
    registerSensorService(SysType::WCY, wcy);
#endif

    auto demo = new DemoDeviceService();
    registerSensorService(SysType::DEMO, demo);
}

void Core::start(SysType sysType)
{
    QMutexLocker locker(&m_mutex);
    qInfo() << "Starting" << SYS_TYPES::sysTypeToString(sysType);

    if (!m_services.contains(sysType)) {
        qWarning() << "Start system error: system type is not defined: " << SYS_TYPES::sysTypeToString(sysType);
    }

    _devService = m_services.value(sysType);
    Q_ASSERT(_devService);

    _devService->init();
    _devService->startScan();
    // QtConcurrent::run([this]() {
    //     _devService->startScan();
    // });

//     if (!m_scanTimer) {
//         m_scanTimer = new QTimer(this);
//         m_scanTimer->setSingleShot(true);
//         m_scanTimer->setInterval(15 * 1000); // first scanning interval
//         connect(m_scanTimer, &QTimer::timeout, this, [this]() {
//             m_scanTimer->setInterval(30 * 1000); // next scanning interval

//             QtConcurrent::run([this]() {
//                 _devService->startScan();
//             });

// //          m_scanTimer->start();
//         });
//     }

//     m_scanTimer->start();

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
    if (m_scanTimer) {
        m_scanTimer->stop();
    }

    qDebug() << "Core stopping";

    if (_devService == nullptr) {
        qDebug() << "Core stopped (no device service)";
        emit stopCompleted(); // Optional: emit signal when done
        return;
    }

    _devService->disconnect(); // disconnect from all subscriped signals
    _devService->stopAll();

    m_streamServer->stop();
    // Non-blocking delay using QTimer
    QTimer::singleShot(1000, [this]() {
        _devService->exit();

        _devService->deleteLater();
        _devService = nullptr;

        emit stopCompleted(); // Optional: emit signal when done
    });
}

// void Core::thread_proc()
// {
//     QThread::msleep(1000);

//     while (1)
//     {
//         m_mutex.lock();


//         m_mutex.unlock();

//         QThread::msleep(5);
//     }
// }

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
    m_process.start(scriptPath);
    // _process.start("bash", QStringList() << scriptPath);
#endif

    if (!m_process.waitForStarted(5000)) {
        qDebug() << "Error: Failed to start script";
        return _return_FAIL;
    }

    qDebug() << "Script" << scriptPath << "started successfully";
    qDebug() << "Process PID:" << m_process.processId();

    // Не ждем завершения, так как процесс должен работать постоянно
    // Подключаем сигналы для отслеживания состояния
    QObject::connect(&m_process, &QProcess::readyReadStandardOutput, [this]() {
        qDebug() << "Script stdout:" << m_process.readAllStandardOutput();
    });

    QObject::connect(&m_process, &QProcess::readyReadStandardError, [this]() {
        qDebug() << "Script stderr:" << m_process.readAllStandardError();
    });

    QObject::connect(&m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     [](int exitCode, QProcess::ExitStatus exitStatus) {
                         qDebug() << "Script finished with exit code:" << exitCode
                                  << "exit status:" << exitStatus;
                     });

    return _return_OK;
}

void Core::registerSensorService(SysType sysType, IDeviceService *service)
{
    if (m_services.contains(sysType)) return;
    m_services.insert(sysType, service);

    QObject::connect(service, &IDeviceService::scanFinished, [this](IDeviceService* source) {
        auto devList = source->getConnectedDeviceList();
        auto dtoToSend = convertInfoToDTO(devList);
        this->send(dtoToSend);
        // if (m_scanTimer) {
        //     m_scanTimer->start();
        // }
    });

    QObject::connect(service, &IDeviceService::updatedeviceStatus, [this](IDeviceService* source, const DeviceInfo& dInfo) {
        auto devList = source->getConnectedDeviceList();
        auto dtoToSend = convertInfoToDTO(devList);
        qInfo() << "[SEND]: " << "update device" << dInfo.deviceMac << "state" << dInfo.connectState;

        this->send(dtoToSend);
    });

    QObject::connect(service, &IDeviceService::dataRecieve, [this](DataFlow data) {
        QJsonObject dataToSend = convertDataFlowToDTO(data);
        QList<QJsonObject> data_list;
        data_list.append(dataToSend);

        emit stream(data_list);
        });
}

QJsonObject Core::convertInfoToDTO(QList<DeviceInfo> dev_list) {

    QJsonObject root;
    root["type"] = "devices_update";

    QJsonArray devices = QJsonArray();

    for (const auto& dev: dev_list) {
        QJsonArray channels;

        for (int ii = 0; ii < DeviceInfo::MAX_CHANNELS; ii++) {
            auto ch = dev.channels[ii];
            if (ch.id == 0) break;

            QJsonArray units;
            for (int jj = 0; jj < MAX_UNITS; jj++) {
                int unitId = ch.availableUnits[jj];
                if (unitId == 0) break;

                units << ch.availableUnits[jj];
            }

            auto ch_obj = DeviceGenerator::createChannel(ch.id, ch.symbol, ch.name, ch.baseUnit, units);
            channels.append(ch_obj);
        }

        bool isConnected = (dev.connectState == CONNECT_STATE::StateStarted) || (dev.connectState == CONNECT_STATE::StateConnected);
        QString connType = (dev.connectType == BLECONNECT) ? "BLE" : "USB";
        QJsonObject dto = DeviceGenerator::createSensor(
            dev.deviceMac, dev.deviceType, dev.deviceId, dev.deviceName,dev.displayType, connType, isConnected, channels, dev.frequency);

        devices.append(dto);
    }

    root["data"] = devices;

    return root;
}

QJsonObject Core::convertDataFlowToDTO(DataFlow data) {

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
