#ifndef DEVICE_GENERATOR_H
#define DEVICE_GENERATOR_H

#include <QJsonObject>
#include <QRandomGenerator>

class DeviceGenerator {
public:
    static QJsonObject generateSensor(const QString& address,
                                      const QString& type,
                                      const QString& id,
                                      const QString& name,
                                      const QString& displayType,
                                      bool connected,
                                      const QJsonArray& channels) {
        QJsonObject sensor;
        sensor["address"] = address;
        sensor["type"] = type;
        sensor["id"] = id;
        sensor["name"] = name;
        sensor["displayType"] = displayType;
        sensor["connected"] = connected;
        sensor["process"] = false;
        sensor["lastFoundTimestamp"] = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
        sensor["channels"] = channels;
        return sensor;
    }

    static QJsonObject createChannel(int id, const QString& symbol, const QString& name,
                                     int unit, const QJsonArray& availableUnits) {
        QJsonObject channel;
        channel["id"] = id;
        channel["symbol"] = symbol;
        channel["name"] = name;
        channel["unit"] = unit;
        channel["availableUnits"] = availableUnits;
        return channel;
    }

    // Генерация синусоидальных данных для датчика
    static QJsonObject generateSensorData(const QString& address, const QJsonArray& channels) {
        QJsonObject root;
        root["type"] = "sensor_data";

        QJsonObject dataObj;
        dataObj["address"] = address;

        QJsonObject sensorData;

        // Текущее время в формате timestamp
        qint64 timestamp = QTime::currentTime().msecsSinceStartOfDay() * 1000;
        sensorData["0"] = timestamp;

        // Генерируем данные для каждого канала
        for (const QJsonValue& channelValue : channels) {
            QJsonObject channel = channelValue.toObject();
            int channelId = channel["id"].toInt();
            QString channelName = channel["name"].toString();

            // Пропускаем канал с id 0 (время)
            if (channelId == 0) continue;

            // Генерируем синусоидальное значение с разной частотой в зависимости от типа канала
            double value = generateSineWaveValue(channelId, channelName, timestamp);
            sensorData[QString::number(channelId)] = value;
        }

        dataObj["data"] = sensorData;
        root["data"] = dataObj;

        return root;
    }

private:
    // Генерация синусоидального значения с разными параметрами для разных типов каналов
    static double generateSineWaveValue(int channelId, const QString& channelName, qint64 timestamp) {
        static QHash<QString, double> frequencyMap = {
            {"voltage", 0.5},      // Низкая частота для напряжения
            {"current", 2.0},      // Средняя частота для тока
            {"frequency", 5.0},    // Высокая частота для частоты
            {"pressure", 0.3},     // Очень низкая частота для давления
            {"temperature", 0.1},  // Очень медленные колебания температуры
            {"ax", 10.0},          // Высокая частота для акселерометра X
            {"ay", 10.5},          // Высокая частота для акселерометра Y
            {"az", 9.5},           // Высокая частота для акселерометра Z
            {"f", 2.5}             // Средняя частота для частоты
        };

        static QHash<QString, double> amplitudeMap = {
            {"voltage", 220.0},    // Амплитуда 220V
            {"current", 10.0},     // Амплитуда 10A
            {"frequency", 50.0},   // Амплитуда 50Hz
            {"pressure", 100000.0},// Амплитуда 100000 Па
            {"temperature", 25.0}, // Амплитуда 25°C
            {"ax", 2.0},           // Амплитуда 2g
            {"ay", 2.0},           // Амплитуда 2g
            {"az", 2.0},           // Амплитуда 2g
            {"f", 100.0}           // Амплитуда 100Hz
        };

        static QHash<QString, double> offsetMap = {
            {"voltage", 220.0},    // Смещение 220V
            {"current", 5.0},      // Смещение 5A
            {"frequency", 50.0},   // Смещение 50Hz
            {"pressure", 101325.0},// Смещение атмосферное давление
            {"temperature", 20.0}, // Смещение 20°C
            {"ax", 0.0},           // Смещение 0g
            {"ay", 0.0},           // Смещение 0g
            {"az", 1.0},           // Смещение 1g (гравитация)
            {"f", 50.0}            // Смещение 50Hz
        };

        // Получаем параметры для данного канала
        double frequency = frequencyMap.value(channelName, 1.0);
        double amplitude = amplitudeMap.value(channelName, 1.0);
        double offset = offsetMap.value(channelName, 0.0);

        // Добавляем немного случайности к параметрам
        QRandomGenerator* generator = QRandomGenerator::global();
        double randomFactor = 1.0 + (generator->generateDouble() - 0.5) * 0.1; // ±5% случайности
        double noise = (generator->generateDouble() - 0.5) * amplitude * 0.02; // 2% шума

        // Вычисляем синусоидальное значение
        double timeInSeconds = timestamp / 1000000.0; // переводим в секунды
        double value = offset + amplitude * qSin(2 * M_PI * frequency * timeInSeconds) * randomFactor + noise;

        // Округляем до 2 знаков после запятой
        return qRound(value * 100.0) / 100.0;
    }
};

#endif // DEVICE_GENERATOR_H
