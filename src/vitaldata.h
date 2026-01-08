#pragma once
#include <QString>
#include <QDateTime>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>

// 生命体征数据结构
struct VitalData {
    qint64 id = 0;
    QDateTime timestamp;
    double temperature = 0.0;      // 体温 (°C)
    int heartRate = 0;             // 心率 (bpm)
    int bloodOxygen = 0;           // 血氧 (%)
    QVector<double> ecgData;       // 心电图数据
    
    bool isValid() const {
        return temperature > 0 || heartRate > 0 || bloodOxygen > 0 || !ecgData.isEmpty();
    }
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["timestamp"] = timestamp.toString(Qt::ISODate);
        obj["temperature"] = temperature;
        obj["heartRate"] = heartRate;
        obj["bloodOxygen"] = bloodOxygen;
        
        QJsonArray ecgArray;
        for (double val : ecgData) {
            ecgArray.append(val);
        }
        obj["ecgData"] = ecgArray;
        
        return obj;
    }
    
    static VitalData fromJson(const QJsonObject& obj) {
        VitalData data;
        data.id = obj["id"].toVariant().toLongLong();
        data.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
        data.temperature = obj["temperature"].toDouble();
        data.heartRate = obj["heartRate"].toInt();
        data.bloodOxygen = obj["bloodOxygen"].toInt();
        
        QJsonArray ecgArray = obj["ecgData"].toArray();
        for (const QJsonValue& val : ecgArray) {
            data.ecgData.append(val.toDouble());
        }
        
        return data;
    }
};

// 报警信息结构
struct AlarmInfo {
    enum AlarmType {
        HighTemperature,
        LowTemperature,
        HighHeartRate,
        LowHeartRate,
        LowBloodOxygen,
        AbnormalECG
    };
    
    AlarmType type;
    QString message;
    QDateTime timestamp;
    double value;
    bool acknowledged = false;
    
    static QString typeToString(AlarmType type) {
        switch (type) {
            case HighTemperature: return QStringLiteral("体温过高");
            case LowTemperature: return QStringLiteral("体温过低");
            case HighHeartRate: return QStringLiteral("心率过快");
            case LowHeartRate: return QStringLiteral("心率过缓");
            case LowBloodOxygen: return QStringLiteral("血氧过低");
            case AbnormalECG: return QStringLiteral("心电异常");
            default: return QStringLiteral("未知报警");
        }
    }
};

// 报警阈值设置
struct AlarmThresholds {
    double tempHigh = 37.5;
    double tempLow = 35.0;
    int hrHigh = 100;
    int hrLow = 50;
    int spo2Low = 90;
};
