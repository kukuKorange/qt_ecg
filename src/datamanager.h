#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QVector>
#include <QDateTime>
#include "vitaldata.h"

class DataManager : public QObject {
    Q_OBJECT

public:
    explicit DataManager(QObject* parent = nullptr);
    ~DataManager();

    bool initialize(const QString& dbPath = QString());
    void close();
    
    // 数据存储
    bool saveVitalData(const VitalData& data);
    bool saveTemperature(double temp, const QDateTime& timestamp = QDateTime::currentDateTime());
    bool saveHeartRate(int hr, const QDateTime& timestamp = QDateTime::currentDateTime());
    bool saveBloodOxygen(int spo2, const QDateTime& timestamp = QDateTime::currentDateTime());
    bool saveEcgData(const QVector<double>& ecgData, const QDateTime& timestamp = QDateTime::currentDateTime());
    
    // 数据查询
    QVector<VitalData> getVitalDataRange(const QDateTime& start, const QDateTime& end);
    QVector<VitalData> getRecentData(int minutes = 60);
    VitalData getLatestData();
    
    // 统计查询
    double getAverageTemperature(const QDateTime& start, const QDateTime& end);
    double getAverageHeartRate(const QDateTime& start, const QDateTime& end);
    double getAverageBloodOxygen(const QDateTime& start, const QDateTime& end);
    
    QPair<double, double> getTemperatureRange(const QDateTime& start, const QDateTime& end);
    QPair<int, int> getHeartRateRange(const QDateTime& start, const QDateTime& end);
    QPair<int, int> getBloodOxygenRange(const QDateTime& start, const QDateTime& end);
    
    // 数据导出
    bool exportToCsv(const QString& filePath, const QDateTime& start, const QDateTime& end);
    bool exportToJson(const QString& filePath, const QDateTime& start, const QDateTime& end);
    
    // 数据清理
    bool deleteOldData(int daysToKeep = 30);
    qint64 getDatabaseSize() const;
    int getRecordCount() const;
    
    // 报警记录
    bool saveAlarm(const AlarmInfo& alarm);
    QVector<AlarmInfo> getAlarms(const QDateTime& start, const QDateTime& end);
    bool acknowledgeAlarm(qint64 alarmId);

signals:
    void dataSaved();
    void error(const QString& message);

private:
    bool createTables();
    bool executeQuery(const QString& query);
    
    QSqlDatabase m_db;
    QString m_dbPath;
};
