#include "wcy_dislab_wrapper.h"
#include "qwindowdefs_win.h"
#include <QDebug>
#include <QThread>
#include <qcoreapplication.h>

#include <QApplication>
#include <windows.h>

// Инициализация статического указателя
WCY_DisLab_Wrapper* WCY_DisLab_Wrapper::instance = nullptr;

WCY_DisLab_Wrapper::WCY_DisLab_Wrapper(QObject *parent)
    : QObject(parent)
{
    instance = this;
    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
}

WCY_DisLab_Wrapper::~WCY_DisLab_Wrapper()
{
    dispose();
    instance = nullptr;
}

bool WCY_DisLab_Wrapper::initialize()
{

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        qDebug() << "COM initialization failed:" << hr;
        emit errorOccurred(QString("COM initialization failed: %1").arg(hr));
        return false;
    }

    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());

    m_timer.start();
    return ::Initialize();
}

void WCY_DisLab_Wrapper::dispose()
{
    ::Dispose();
}

void WCY_DisLab_Wrapper::deviceScanCallbackStatic(const DeviceScan* deviceScan)
{
    if (instance && deviceScan) {
        emit instance->deviceScanned(*deviceScan);
        if (instance->deviceScanCallback) {
            instance->deviceScanCallback(*deviceScan);
        }
    }
}

void WCY_DisLab_Wrapper::dataCallbackStatic(const DataCallback* dataCallback)
{
    if (instance && dataCallback) {

        WCY_DisLab_Wrapper::RData send_dat;
        send_dat.rangeId = dataCallback->comboFetcher.rangeId;
        send_dat.value = dataCallback->data;

        send_dat.timestamp = instance->m_timer.nsecsElapsed() / 1000; // микросекунды
//        qInfo() << "timestamp2: " << send_dat.timestamp;

        // Статический таймер для измерения частоты
        // static int iCounter = 0;
        // static int rangeId = dataCallback->comboFetcher.rangeId;
        // static QElapsedTimer hzTimer;
        // static bool timerStarted = false;

        // if (rangeId == dataCallback->comboFetcher.rangeId) {
        //     iCounter++;
        // }

        // if (!timerStarted) {
        //     hzTimer.start();
        //     timerStarted = true;
        // }

        // // Проверяем, прошла ли секунда
        // if (hzTimer.elapsed() >= 1000) {
        //     qInfo() << "real frequency:" << iCounter << "packets per second";
        //     iCounter = 0;
        //     hzTimer.restart(); // Сбрасываем для следующей секунды
        // }

        emit instance->dataReceived(send_dat);

        if (instance->dataReceivedCallback) {
            instance->dataReceivedCallback(*dataCallback);
        }

        // QMetaObject::invokeMethod(instance, "handleDataReceived",
        //                           Qt::QueuedConnection,
        //                           Q_ARG(DataCallback, dataCopy));
    };
}

void WCY_DisLab_Wrapper::handleDataReceived(const DataCallback& data)
{
    QMutexLocker locker(&m_callbackMutex);

    WCY_DisLab_Wrapper::RData send_dat;
    send_dat.rangeId = data.comboFetcher.rangeId;
    send_dat.value = data.data;

    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    send_dat.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();

    emit dataReceived(send_dat);
    if (dataReceivedCallback) {
        dataReceivedCallback(data);
    }
}

void WCY_DisLab_Wrapper::deviceRemovedCallbackStatic(const DeviceScan& deviceScan)
{
    if (instance) {
        emit instance->deviceRemoved(deviceScan);
        if (instance->deviceRemovedCallback) {
            instance->deviceRemovedCallback(deviceScan);
        }
    }
}

void WCY_DisLab_Wrapper::startBLEScan(int timeoutSeconds)
{
    ::SetDeviceScan(deviceScanCallbackStatic);
    ::BLEScan(timeoutSeconds);
}

QVector<DeviceScan> WCY_DisLab_Wrapper::startHIDScan()
{
    QVector<DeviceScan> devices;
    DeviceScan* deviceArray = nullptr;
    int deviceCount = 0;

    int result = ::HIDScan(&deviceArray, &deviceCount);

    if (result == 1 && deviceCount > 0 && deviceArray) {
        for (int i = 0; i < deviceCount; i++) {
            devices.append(deviceArray[i]);
        }
    }

    return devices;
}

bool WCY_DisLab_Wrapper::connectDevice(const DeviceScan& device)
{
    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());

    try {
        return ::Connect(&device);
    } catch (const std::exception& e) {
        QString error = QString("Connection error: %1").arg(e.what());
        qCritical() << error;
        return false;
    } catch (...) {
        qCritical() << "Unknown connection error";
        return false;
    }

}

void WCY_DisLab_Wrapper::disconnectDevice(const DeviceScan& device)
{
    ::DisConnect(&device);
}

void WCY_DisLab_Wrapper::startDataCollection(const DeviceScan& device)
{
    try {
        ::SetDataCallback(dataCallbackStatic);
        ::Start(&device);
        return;
    } catch (const std::exception& e) {
        QString error = QString("Start data collection error: %1").arg(e.what());
        qCritical() << error;
        return;
    } catch (...) {
        qCritical() << "Unknown start data collection error";
        return;
    }

}

void WCY_DisLab_Wrapper::stopDataCollection(const DeviceScan& device)
{
    ::Stop(&device);
}

QVector<ComboFetcher> WCY_DisLab_Wrapper::getDataTypes(const DeviceScan& device)
{
    QVector<ComboFetcher> fetchers;
    ComboFetcher* comboArray = nullptr;
    int count = 0;

    if (::GetDataTypes(&device, &comboArray, &count) && count > 0 && comboArray) {
        for (int i = 0; i < count; i++) {
            fetchers.append(comboArray[i]);
        }
        ::FreeDataTypes(comboArray);
    }

    return fetchers;
}

bool WCY_DisLab_Wrapper::setSensorLanguage(const DeviceScan& device, int langType)
{
    return ::SetSensorLang(&device, langType);
}

QVector<SwitchRange> WCY_DisLab_Wrapper::getSwitchRanges(const DeviceScan& device)
{
    QVector<SwitchRange> ranges;
    SwitchRange* rangeArray = nullptr;
    int count = 0;

    if (::GetSwitchRanges(&device, &rangeArray, &count) && count > 0 && rangeArray) {
        for (int i = 0; i < count; i++) {
            ranges.append(rangeArray[i]);
        }
        ::FreeSwitchRanges(rangeArray);
    }

    return ranges;
}

bool WCY_DisLab_Wrapper::setSwitchRange(const DeviceScan& device, int rangeId)
{
    return ::SetSwitchRange(&device, rangeId);
}

int WCY_DisLab_Wrapper::getCurrentFrequency(const DeviceScan& device)
{
    return ::GetFrequency(&device);
}

QVector<Frequency> WCY_DisLab_Wrapper::getSupportedFrequencies(const DeviceScan& device)
{
    QVector<Frequency> frequencies;
    Frequency* freqArray = nullptr;
    int count = 0;

    if (::GetSupportFrequency(&device, &freqArray, &count) && count > 0 && freqArray) {
        for (int i = 0; i < count; i++) {
            frequencies.append(freqArray[i]);
        }
        ::FreeSupportFrequency(freqArray);
    }

    return frequencies;
}

bool WCY_DisLab_Wrapper::setFrequency(const DeviceScan& device, int frequency)
{
    return ::SetFrequency(&device, frequency);
}

bool WCY_DisLab_Wrapper::clearZero(const DeviceScan& device)
{
    return ::ClearZero(&device);
}

void WCY_DisLab_Wrapper::shutdownDevice(const DeviceScan& device)
{
    ::Shutdown(&device);
}

void WCY_DisLab_Wrapper::setDeviceScanCallback(DeviceScanCallback callback)
{
    deviceScanCallback = callback;
}

void WCY_DisLab_Wrapper::setDataReceivedCallback(DataReceivedCallback callback)
{
    dataReceivedCallback = callback;
    ::SetDataCallback(dataCallbackStatic);
}

void WCY_DisLab_Wrapper::setDeviceRemovedCallback(DeviceRemovedCallback callback)
{
    deviceRemovedCallback = callback;
    ::SetDeviceRemovedCallback(deviceRemovedCallbackStatic);
}
