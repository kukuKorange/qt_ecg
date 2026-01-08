#include "datamanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QFileInfo>
#include <QDataStream>

DataManager::DataManager(QObject* parent)
    : QObject(parent)
{
}

DataManager::~DataManager()
{
    close();
}

bool DataManager::initialize(const QString& dbPath)
{
    if (dbPath.isEmpty()) {
        QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dataDir);
        m_dbPath = dataDir + "/health_monitor.db";
    } else {
        m_dbPath = dbPath;
    }
    
    m_db = QSqlDatabase::addDatabase("QSQLITE", "health_db");
    m_db.setDatabaseName(m_dbPath);
    
    if (!m_db.open()) {
        emit error(QStringLiteral("无法打开数据库: %1").arg(m_db.lastError().text()));
        return false;
    }
    
    return createTables();
}

void DataManager::close()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DataManager::createTables()
{
    QSqlQuery query(m_db);
    
    // 生命体征数据表
    QString createVitals = R"(
        CREATE TABLE IF NOT EXISTS vital_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            temperature REAL,
            heart_rate INTEGER,
            blood_oxygen INTEGER,
            ecg_data BLOB,
            synced INTEGER DEFAULT 0
        )
    )";
    
    if (!query.exec(createVitals)) {
        emit error(QStringLiteral("创建vital_data表失败: %1").arg(query.lastError().text()));
        return false;
    }
    
    // 报警记录表
    QString createAlarms = R"(
        CREATE TABLE IF NOT EXISTS alarms (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            type INTEGER,
            message TEXT,
            value REAL,
            acknowledged INTEGER DEFAULT 0
        )
    )";
    
    if (!query.exec(createAlarms)) {
        emit error(QStringLiteral("创建alarms表失败: %1").arg(query.lastError().text()));
        return false;
    }
    
    // 创建索引
    query.exec("CREATE INDEX IF NOT EXISTS idx_vital_timestamp ON vital_data(timestamp)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_alarm_timestamp ON alarms(timestamp)");
    
    return true;
}

bool DataManager::saveVitalData(const VitalData& data)
{
    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT INTO vital_data (timestamp, temperature, heart_rate, blood_oxygen, ecg_data)
        VALUES (:timestamp, :temp, :hr, :spo2, :ecg)
    )");
    
    query.bindValue(":timestamp", data.timestamp.toString(Qt::ISODate));
    query.bindValue(":temp", data.temperature);
    query.bindValue(":hr", data.heartRate);
    query.bindValue(":spo2", data.bloodOxygen);
    
    // 序列化ECG数据
    QByteArray ecgBytes;
    if (!data.ecgData.isEmpty()) {
        QDataStream stream(&ecgBytes, QIODevice::WriteOnly);
        stream << data.ecgData;
    }
    query.bindValue(":ecg", ecgBytes);
    
    if (!query.exec()) {
        emit error(QStringLiteral("保存数据失败: %1").arg(query.lastError().text()));
        return false;
    }
    
    emit dataSaved();
    return true;
}

bool DataManager::saveTemperature(double temp, const QDateTime& timestamp)
{
    VitalData data;
    data.timestamp = timestamp;
    data.temperature = temp;
    return saveVitalData(data);
}

bool DataManager::saveHeartRate(int hr, const QDateTime& timestamp)
{
    VitalData data;
    data.timestamp = timestamp;
    data.heartRate = hr;
    return saveVitalData(data);
}

bool DataManager::saveBloodOxygen(int spo2, const QDateTime& timestamp)
{
    VitalData data;
    data.timestamp = timestamp;
    data.bloodOxygen = spo2;
    return saveVitalData(data);
}

bool DataManager::saveEcgData(const QVector<double>& ecgData, const QDateTime& timestamp)
{
    VitalData data;
    data.timestamp = timestamp;
    data.ecgData = ecgData;
    return saveVitalData(data);
}

QVector<VitalData> DataManager::getVitalDataRange(const QDateTime& start, const QDateTime& end)
{
    QVector<VitalData> results;
    QSqlQuery query(m_db);
    
    query.prepare(R"(
        SELECT id, timestamp, temperature, heart_rate, blood_oxygen, ecg_data
        FROM vital_data
        WHERE timestamp BETWEEN :start AND :end
        ORDER BY timestamp ASC
    )");
    
    query.bindValue(":start", start.toString(Qt::ISODate));
    query.bindValue(":end", end.toString(Qt::ISODate));
    
    if (query.exec()) {
        while (query.next()) {
            VitalData data;
            data.id = query.value(0).toLongLong();
            data.timestamp = QDateTime::fromString(query.value(1).toString(), Qt::ISODate);
            data.temperature = query.value(2).toDouble();
            data.heartRate = query.value(3).toInt();
            data.bloodOxygen = query.value(4).toInt();
            
            QByteArray ecgBytes = query.value(5).toByteArray();
            if (!ecgBytes.isEmpty()) {
                QDataStream stream(&ecgBytes, QIODevice::ReadOnly);
                stream >> data.ecgData;
            }
            
            results.append(data);
        }
    }
    
    return results;
}

QVector<VitalData> DataManager::getRecentData(int minutes)
{
    QDateTime end = QDateTime::currentDateTime();
    QDateTime start = end.addSecs(-minutes * 60);
    return getVitalDataRange(start, end);
}

VitalData DataManager::getLatestData()
{
    VitalData data;
    QSqlQuery query(m_db);
    
    query.prepare(R"(
        SELECT id, timestamp, temperature, heart_rate, blood_oxygen, ecg_data
        FROM vital_data
        ORDER BY timestamp DESC
        LIMIT 1
    )");
    
    if (query.exec() && query.next()) {
        data.id = query.value(0).toLongLong();
        data.timestamp = QDateTime::fromString(query.value(1).toString(), Qt::ISODate);
        data.temperature = query.value(2).toDouble();
        data.heartRate = query.value(3).toInt();
        data.bloodOxygen = query.value(4).toInt();
        
        QByteArray ecgBytes = query.value(5).toByteArray();
        if (!ecgBytes.isEmpty()) {
            QDataStream stream(&ecgBytes, QIODevice::ReadOnly);
            stream >> data.ecgData;
        }
    }
    
    return data;
}

double DataManager::getAverageTemperature(const QDateTime& start, const QDateTime& end)
{
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT AVG(temperature) FROM vital_data
        WHERE timestamp BETWEEN :start AND :end AND temperature > 0
    )");
    query.bindValue(":start", start.toString(Qt::ISODate));
    query.bindValue(":end", end.toString(Qt::ISODate));
    
    if (query.exec() && query.next()) {
        return query.value(0).toDouble();
    }
    return 0.0;
}

double DataManager::getAverageHeartRate(const QDateTime& start, const QDateTime& end)
{
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT AVG(heart_rate) FROM vital_data
        WHERE timestamp BETWEEN :start AND :end AND heart_rate > 0
    )");
    query.bindValue(":start", start.toString(Qt::ISODate));
    query.bindValue(":end", end.toString(Qt::ISODate));
    
    if (query.exec() && query.next()) {
        return query.value(0).toDouble();
    }
    return 0.0;
}

double DataManager::getAverageBloodOxygen(const QDateTime& start, const QDateTime& end)
{
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT AVG(blood_oxygen) FROM vital_data
        WHERE timestamp BETWEEN :start AND :end AND blood_oxygen > 0
    )");
    query.bindValue(":start", start.toString(Qt::ISODate));
    query.bindValue(":end", end.toString(Qt::ISODate));
    
    if (query.exec() && query.next()) {
        return query.value(0).toDouble();
    }
    return 0.0;
}

QPair<double, double> DataManager::getTemperatureRange(const QDateTime& start, const QDateTime& end)
{
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT MIN(temperature), MAX(temperature) FROM vital_data
        WHERE timestamp BETWEEN :start AND :end AND temperature > 0
    )");
    query.bindValue(":start", start.toString(Qt::ISODate));
    query.bindValue(":end", end.toString(Qt::ISODate));
    
    if (query.exec() && query.next()) {
        return qMakePair(query.value(0).toDouble(), query.value(1).toDouble());
    }
    return qMakePair(0.0, 0.0);
}

QPair<int, int> DataManager::getHeartRateRange(const QDateTime& start, const QDateTime& end)
{
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT MIN(heart_rate), MAX(heart_rate) FROM vital_data
        WHERE timestamp BETWEEN :start AND :end AND heart_rate > 0
    )");
    query.bindValue(":start", start.toString(Qt::ISODate));
    query.bindValue(":end", end.toString(Qt::ISODate));
    
    if (query.exec() && query.next()) {
        return qMakePair(query.value(0).toInt(), query.value(1).toInt());
    }
    return qMakePair(0, 0);
}

QPair<int, int> DataManager::getBloodOxygenRange(const QDateTime& start, const QDateTime& end)
{
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT MIN(blood_oxygen), MAX(blood_oxygen) FROM vital_data
        WHERE timestamp BETWEEN :start AND :end AND blood_oxygen > 0
    )");
    query.bindValue(":start", start.toString(Qt::ISODate));
    query.bindValue(":end", end.toString(Qt::ISODate));
    
    if (query.exec() && query.next()) {
        return qMakePair(query.value(0).toInt(), query.value(1).toInt());
    }
    return qMakePair(0, 0);
}

bool DataManager::exportToCsv(const QString& filePath, const QDateTime& start, const QDateTime& end)
{
    QVector<VitalData> data = getVitalDataRange(start, end);
    if (data.isEmpty()) {
        return false;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit error(QStringLiteral("无法创建文件: %1").arg(filePath));
        return false;
    }
    
    QTextStream out(&file);
    out << "时间戳,体温(°C),心率(bpm),血氧(%)\n";
    
    for (const VitalData& d : data) {
        out << d.timestamp.toString("yyyy-MM-dd HH:mm:ss") << ","
            << d.temperature << ","
            << d.heartRate << ","
            << d.bloodOxygen << "\n";
    }
    
    file.close();
    return true;
}

bool DataManager::exportToJson(const QString& filePath, const QDateTime& start, const QDateTime& end)
{
    QVector<VitalData> data = getVitalDataRange(start, end);
    if (data.isEmpty()) {
        return false;
    }
    
    QJsonArray jsonArray;
    for (const VitalData& d : data) {
        jsonArray.append(d.toJson());
    }
    
    QJsonDocument doc(jsonArray);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit error(QStringLiteral("无法创建文件: %1").arg(filePath));
        return false;
    }
    
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool DataManager::deleteOldData(int daysToKeep)
{
    QSqlQuery query(m_db);
    QDateTime cutoff = QDateTime::currentDateTime().addDays(-daysToKeep);
    
    query.prepare("DELETE FROM vital_data WHERE timestamp < :cutoff");
    query.bindValue(":cutoff", cutoff.toString(Qt::ISODate));
    
    return query.exec();
}

qint64 DataManager::getDatabaseSize() const
{
    QFileInfo info(m_dbPath);
    return info.size();
}

int DataManager::getRecordCount() const
{
    QSqlQuery query(m_db);
    if (query.exec("SELECT COUNT(*) FROM vital_data") && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

bool DataManager::saveAlarm(const AlarmInfo& alarm)
{
    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT INTO alarms (timestamp, type, message, value, acknowledged)
        VALUES (:timestamp, :type, :message, :value, :acknowledged)
    )");
    
    query.bindValue(":timestamp", alarm.timestamp.toString(Qt::ISODate));
    query.bindValue(":type", static_cast<int>(alarm.type));
    query.bindValue(":message", alarm.message);
    query.bindValue(":value", alarm.value);
    query.bindValue(":acknowledged", alarm.acknowledged ? 1 : 0);
    
    return query.exec();
}

QVector<AlarmInfo> DataManager::getAlarms(const QDateTime& start, const QDateTime& end)
{
    QVector<AlarmInfo> results;
    QSqlQuery query(m_db);
    
    query.prepare(R"(
        SELECT id, timestamp, type, message, value, acknowledged
        FROM alarms
        WHERE timestamp BETWEEN :start AND :end
        ORDER BY timestamp DESC
    )");
    
    query.bindValue(":start", start.toString(Qt::ISODate));
    query.bindValue(":end", end.toString(Qt::ISODate));
    
    if (query.exec()) {
        while (query.next()) {
            AlarmInfo alarm;
            alarm.timestamp = QDateTime::fromString(query.value(1).toString(), Qt::ISODate);
            alarm.type = static_cast<AlarmInfo::AlarmType>(query.value(2).toInt());
            alarm.message = query.value(3).toString();
            alarm.value = query.value(4).toDouble();
            alarm.acknowledged = query.value(5).toBool();
            results.append(alarm);
        }
    }
    
    return results;
}

bool DataManager::acknowledgeAlarm(qint64 alarmId)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE alarms SET acknowledged = 1 WHERE id = :id");
    query.bindValue(":id", alarmId);
    return query.exec();
}

bool DataManager::executeQuery(const QString& queryStr)
{
    QSqlQuery query(m_db);
    if (!query.exec(queryStr)) {
        emit error(query.lastError().text());
        return false;
    }
    return true;
}
