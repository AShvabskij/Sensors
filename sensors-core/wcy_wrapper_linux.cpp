// wcy_wrapper_linux.cpp
#include "wcy_wrapper_linux.h"
#include <QDebug>
#include <QCoreApplication>
#include <QThread>
#include <QDir>

// Инициализация статического указателя
WCY_Wrapper_Linux* WCY_Wrapper_Linux::m_instance = nullptr;

WCY_Wrapper_Linux::WCY_Wrapper_Linux(QObject *parent)
    : IWCY_Wrapper(parent)
    , m_library(nullptr)
    , m_initialized(false)
    , m_initialize(nullptr)
    , m_setDeviceScan(nullptr)
    , m_bleScan(nullptr)
    , m_hidScan(nullptr)
    , m_setDataCallback(nullptr)
    , m_setDeviceRemovedCallback(nullptr)
    , m_connect(nullptr)
    , m_disconnect(nullptr)
    , m_start(nullptr)
    , m_stop(nullptr)
    , m_getDataTypes(nullptr)
    , m_freeDataTypes(nullptr)
    , m_setSensorLang(nullptr)
    , m_getSwitchRanges(nullptr)
    , m_freeSwitchRanges(nullptr)
    , m_setSwitchRange(nullptr)
    , m_getFrequency(nullptr)
    , m_getSupportFrequency(nullptr)
    , m_freeSupportFrequency(nullptr)
    , m_setFrequency(nullptr)
    , m_clearZero(nullptr)
    , m_shutdown(nullptr)
    , m_dispose(nullptr)
{
    m_instance = this;
}

WCY_Wrapper_Linux::~WCY_Wrapper_Linux()
{
    dispose();
    if (m_library) {
        if (m_library->isLoaded()) {
            m_library->unload();
        }
        delete m_library;
        m_library = nullptr;
    }
    m_instance = nullptr;
}

QString WCY_Wrapper_Linux::bytesToString(const char* bytes, int maxLength)
{
    int length = 0;
    while (length < maxLength && bytes[length] != '\0') {
        length++;
    }
    return QString::fromUtf8(bytes, length);
}

void WCY_Wrapper_Linux::setupLibraryPath()
{
    // Устанавливаем путь к библиотеке как в тесте
    // QString libraryPath = "/home/alex/projects/wcystest/lib";
    // QCoreApplication::addLibraryPath(libraryPath);
    // qputenv("LD_LIBRARY_PATH", QByteArray(libraryPath.toUtf8() + ":" + qgetenv("LD_LIBRARY_PATH")));

    // Загружаем зависимости
    dlopen("libdbus-1.so.3", RTLD_NOW | RTLD_GLOBAL);
}

bool WCY_Wrapper_Linux::resolveFunctions()
{
    if (!m_library) {
        //
        QByteArray ld_path = qgetenv("LD_LIBRARY_PATH");
        qDebug() << "WCY:" << "Пробуем загрузить бибилиотеку wcy_dislab_sdk через системный путь" << "LD_LIBRARY_PATH:" << ld_path;

        m_library = new QLibrary("wcy_dislab_sdk");
        if (!m_library->load()) {
            QString errorMsg = "Failed to load WCY SDK library: " + m_library->errorString();
            emit errorOccurred(errorMsg);
            delete m_library;
            m_library = nullptr;
        }
    }

    if (!m_library) {
        // Пробуем разные пути для загрузки библиотеки
        QStringList possiblePaths = {
            "libwcy_dislab_sdk.so",
            "./libwcy_dislab_sdk.so",
            "/usr/local/lib/libwcy_dislab_sdk.so",
            "/usr/lib/libwcy_dislab_sdk.so"
        };

        for (const QString& path : possiblePaths) {
            if (QLibrary::isLibrary(path)) {
                m_library = new QLibrary(path);
                if (m_library->load()) {
                    qDebug() << "WCY:" << "loaded SDK library from:" << path;
                    break;
                } else {
                    qDebug() << "WCY:" << "failed to load from:" << path << "-" << m_library->errorString();
                    delete m_library;
                    m_library = nullptr;
                }
            }
        }
    }

    if (!m_library || m_library && !m_library->isLoaded()) return false;

    qInfo() << "WCY:" << "loaded SDK library from:" << m_library->fileName();
    // Разрешение функций
    m_initialize = (InitializeFunc)m_library->resolve("Initialize");
    m_setDeviceScan = (SetDeviceScanFunc)m_library->resolve("SetDeviceScan");
    m_bleScan = (BLEScanFunc)m_library->resolve("BLEScan");
    m_hidScan = (HIDScanFunc)m_library->resolve("HIDScan");
    m_setDataCallback = (SetDataCallbackFunc)m_library->resolve("SetDataCallback");
    m_setDeviceRemovedCallback = (SetDeviceRemovedCallbackFunc)m_library->resolve("SetDeviceRemovedCallback");
    m_connect = (ConnectFunc)m_library->resolve("Connect");
    m_disconnect = (DisConnectFunc)m_library->resolve("DisConnect");
    m_start = (StartFunc)m_library->resolve("Start");
    m_stop = (StopFunc)m_library->resolve("Stop");
    m_getDataTypes = (GetDataTypesFunc)m_library->resolve("GetDataTypes");
    m_freeDataTypes = (FreeDataTypesFunc)m_library->resolve("FreeDataTypes");
    m_setSensorLang = (SetSensorLangFunc)m_library->resolve("SetSensorLang");
    m_getSwitchRanges = (GetSwitchRangesFunc)m_library->resolve("GetSwitchRanges");
    m_freeSwitchRanges = (FreeSwitchRangesFunc)m_library->resolve("FreeSwitchRanges");
    m_setSwitchRange = (SetSwitchRangeFunc)m_library->resolve("SetSwitchRange");
    m_getFrequency = (GetFrequencyFunc)m_library->resolve("GetFrequency");
    m_getSupportFrequency = (GetSupportFrequencyFunc)m_library->resolve("GetSupportFrequency");
    m_freeSupportFrequency = (FreeSupportFrequencyFunc)m_library->resolve("FreeSupportFrequency");
    m_setFrequency = (SetFrequencyFunc)m_library->resolve("SetFrequency");
    m_clearZero = (ClearZeroFunc)m_library->resolve("ClearZero");
    m_shutdown = (ShutdownFunc)m_library->resolve("Shutdown");
    m_dispose = (DisposeFunc)m_library->resolve("Dispose");

    bool allResolved = m_initialize && m_setDeviceScan && m_bleScan && m_hidScan &&
                       m_setDataCallback && m_setDeviceRemovedCallback && m_connect &&
                       m_disconnect && m_start && m_stop && m_getDataTypes &&
                       m_freeDataTypes && m_setSensorLang && m_getSwitchRanges &&
                       m_freeSwitchRanges && m_setSwitchRange && m_getFrequency &&
                       m_getSupportFrequency && m_freeSupportFrequency && m_setFrequency &&
                       m_clearZero && m_shutdown && m_dispose;

    if (!allResolved) {
        QStringList missing;
        if (!m_initialize) missing << "Initialize";
        if (!m_setDeviceScan) missing << "SetDeviceScan";
        if (!m_bleScan) missing << "BLEScan";
        if (!m_hidScan) missing << "HIDScan";
        if (!m_setDataCallback) missing << "SetDataCallback";
        if (!m_setDeviceRemovedCallback) missing << "SetDeviceRemovedCallback";
        if (!m_connect) missing << "Connect";
        if (!m_disconnect) missing << "DisConnect";
        if (!m_start) missing << "Start";
        if (!m_stop) missing << "Stop";
        if (!m_getDataTypes) missing << "GetDataTypes";
        if (!m_freeDataTypes) missing << "FreeDataTypes";
        if (!m_setSensorLang) missing << "SetSensorLang";
        if (!m_getSwitchRanges) missing << "GetSwitchRanges";
        if (!m_freeSwitchRanges) missing << "FreeSwitchRanges";
        if (!m_setSwitchRange) missing << "SetSwitchRange";
        if (!m_getFrequency) missing << "GetFrequency";
        if (!m_getSupportFrequency) missing << "GetSupportFrequency";
        if (!m_freeSupportFrequency) missing << "FreeSupportFrequency";
        if (!m_setFrequency) missing << "SetFrequency";
        if (!m_clearZero) missing << "ClearZero";
        if (!m_shutdown) missing << "Shutdown";
        if (!m_dispose) missing << "Dispose";

        emit errorOccurred("Failed to resolve SDK functions: " + missing.join(", "));
    }

    return allResolved;
}

bool WCY_Wrapper_Linux::initialize()
{
    if (m_initialized) return true;

    setupLibraryPath();

    if (!resolveFunctions()) {
        return false;
    }

    bool result = m_initialize();
    if (result) {
        m_timer.start();
        m_initialized = true;
        qInfo() << "WCY: SDK initialized successfully";
    } else {
        emit errorOccurred("Failed to initialize WCY SDK");
    }

    return result;
}

void WCY_Wrapper_Linux::dispose()
{
    if (m_dispose && m_initialized) {
        m_dispose();
        m_initialized = false;
        qDebug() << "WCY: SDK is disposed";
    }
}

void WCY_Wrapper_Linux::deviceScanCallbackStatic(const DeviceScan* deviceScan)
{
    if (m_instance && deviceScan) {
        emit m_instance->deviceScanned(*deviceScan);
        if (m_instance->m_deviceScanCallback) {
            m_instance->m_deviceScanCallback(*deviceScan);
        }
    }
}

void WCY_Wrapper_Linux::dataCallbackStatic(const DataCallback* dataCallback)
{
    if (m_instance && dataCallback) {
        RData send_dat;
        send_dat.rangeId = dataCallback->comboFetcher.rangeId;
        send_dat.value = dataCallback->data;
        send_dat.timestamp = m_instance->m_timer.nsecsElapsed() / 1000; // микросекунды

        emit m_instance->dataReceived(send_dat);

        if (m_instance->m_dataReceivedCallback) {
            m_instance->m_dataReceivedCallback(*dataCallback);
        }
    }
}

void WCY_Wrapper_Linux::deviceRemovedCallbackStatic(const DeviceScan& deviceScan)
{
    if (m_instance) {
        emit m_instance->deviceRemoved(deviceScan);
        if (m_instance->m_deviceRemovedCallback) {
            m_instance->m_deviceRemovedCallback(deviceScan);
        }
    }
}

void WCY_Wrapper_Linux::startBLEScan(int timeoutSeconds)
{
    if (!m_initialized) {
        emit errorOccurred("SDK not initialized");
        return;
    }
    m_setDeviceScan(deviceScanCallbackStatic);
    m_bleScan(timeoutSeconds);
}

QVector<DeviceScan> WCY_Wrapper_Linux::startHIDScan()
{
    QVector<DeviceScan> devices;
    if (!m_initialized) {
        emit errorOccurred("SDK not initialized");
        return devices;
    }

    m_setDeviceScan(deviceScanCallbackStatic);
    DeviceScan* deviceArray = nullptr;
    int deviceCount = 0;

    int res = m_hidScan(&deviceArray, &deviceCount);

    if (res == 1 && deviceCount > 0 && deviceArray) {
        for (int i = 0; i < deviceCount; i++) {
            devices.append(deviceArray[i]);
        }
    }

    return devices;
}

bool WCY_Wrapper_Linux::connectDevice(const DeviceScan& device)
{
    if (!m_initialized) {
        emit errorOccurred("SDK not initialized");
        return false;
    }
    return m_connect(&device);
}

void WCY_Wrapper_Linux::disconnectDevice(const DeviceScan& device)
{
    if (!m_initialized) return;
    m_disconnect(&device);
}

void WCY_Wrapper_Linux::startDataCollection(const DeviceScan& device)
{
    if (!m_initialized) return;
    m_setDataCallback(dataCallbackStatic);
    m_start(&device);
}

void WCY_Wrapper_Linux::stopDataCollection(const DeviceScan& device)
{
    if (!m_initialized) return;
    m_stop(&device);
}

QVector<ComboFetcher> WCY_Wrapper_Linux::getDataTypes(const DeviceScan& device)
{
    QVector<ComboFetcher> fetchers;
    if (!m_initialized) return fetchers;

    ComboFetcher* comboArray = nullptr;
    int count = 0;

    int res = m_getDataTypes(&device, &comboArray, &count);
    if (res == 1 && count > 0 && comboArray) {
        for (int i = 0; i < count; i++) {
            fetchers.append(comboArray[i]);
        }
        m_freeDataTypes(comboArray);
    }

    return fetchers;
}

bool WCY_Wrapper_Linux::setSensorLanguage(const DeviceScan& device, int langType)
{
    if (!m_initialized) return false;
    return m_setSensorLang(&device, langType);
}

QVector<SwitchRange> WCY_Wrapper_Linux::getSwitchRanges(const DeviceScan& device)
{
    QVector<SwitchRange> ranges;
    if (!m_initialized) return ranges;

    SwitchRange* rangeArray = nullptr;
    int count = 0;

    int res = m_getSwitchRanges(&device, &rangeArray, &count);
    if (res == 1 && count > 0 && rangeArray) {
        for (int i = 0; i < count; i++) {
            ranges.append(rangeArray[i]);
        }
        m_freeSwitchRanges(rangeArray);
    }

    return ranges;
}

bool WCY_Wrapper_Linux::setSwitchRange(const DeviceScan& device, int rangeId)
{
    if (!m_initialized) return false;
    return m_setSwitchRange(&device, rangeId);
}

int WCY_Wrapper_Linux::getCurrentFrequency(const DeviceScan& device)
{
    if (!m_initialized) return 0;
    return m_getFrequency(&device);
}

QVector<Frequency> WCY_Wrapper_Linux::getSupportedFrequencies(const DeviceScan& device)
{
    QVector<Frequency> frequencies;
    if (!m_initialized) return frequencies;

    Frequency* freqArray = nullptr;
    int count = 0;

    int res = m_getSupportFrequency(&device, &freqArray, &count);
    if (res == 1 && count > 0 && freqArray) {
        for (int i = 0; i < count; i++) {
            frequencies.append(freqArray[i]);
        }
        m_freeSupportFrequency(freqArray);
    }

    return frequencies;
}

bool WCY_Wrapper_Linux::setFrequency(const DeviceScan& device, int frequency)
{
    if (!m_initialized) return false;
    return m_setFrequency(&device, frequency);
}

bool WCY_Wrapper_Linux::clearZero(const DeviceScan& device)
{
    if (!m_initialized) return false;
    return m_clearZero(&device);
}

void WCY_Wrapper_Linux::shutdownDevice(const DeviceScan& device)
{
    if (!m_initialized) return;
    m_shutdown(&device);
}

void WCY_Wrapper_Linux::setDeviceScanCallback(DeviceScanCallback callback)
{
    m_deviceScanCallback = callback;
}

void WCY_Wrapper_Linux::setDataReceivedCallback(DataReceivedCallback callback)
{
    m_dataReceivedCallback = callback;
}

void WCY_Wrapper_Linux::setDeviceRemovedCallback(DeviceRemovedCallback callback)
{
    m_deviceRemovedCallback = callback;
}
