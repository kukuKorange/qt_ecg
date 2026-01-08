#pragma once
#include <QObject>
#include <QTimer>
#include <QVector>
#include "vitaldata.h"

class DataManager;

class AlarmManager : public QObject {
    Q_OBJECT

public:
    explicit AlarmManager(DataManager* dataManager, QObject* parent = nullptr);
    ~AlarmManager();

    void setThresholds(const AlarmThresholds& thresholds);
    AlarmThresholds getThresholds() const { return m_thresholds; }
    
    void checkVitalData(const VitalData& data);
    void checkTemperature(double temp);
    void checkHeartRate(int hr);
    void checkBloodOxygen(int spo2);
    
    void setSoundEnabled(bool enabled) { m_soundEnabled = enabled; }
    bool isSoundEnabled() const { return m_soundEnabled; }
    
    void acknowledgeAllAlarms();
    QVector<AlarmInfo> getActiveAlarms() const { return m_activeAlarms; }
    int getActiveAlarmCount() const { return m_activeAlarms.size(); }

signals:
    void alarmTriggered(const AlarmInfo& alarm);
    void alarmCleared();
    void activeAlarmsChanged(int count);

private slots:
    void onAlarmTimeout();

private:
    void triggerAlarm(AlarmInfo::AlarmType type, double value);
    void playAlarmSound();
    
    DataManager* m_dataManager;
    AlarmThresholds m_thresholds;
    QVector<AlarmInfo> m_activeAlarms;
    
    QTimer* m_alarmTimer;
    bool m_soundEnabled = true;
    int m_alarmCooldown = 30;  // 报警冷却时间（秒）
    QDateTime m_lastAlarmTime;
};
