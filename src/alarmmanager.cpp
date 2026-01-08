#include "alarmmanager.h"
#include "datamanager.h"
#include <QApplication>
#include <QMessageBox>
#include <QDebug>

AlarmManager::AlarmManager(DataManager* dataManager, QObject* parent)
    : QObject(parent)
    , m_dataManager(dataManager)
    , m_alarmTimer(new QTimer(this))
{
    connect(m_alarmTimer, &QTimer::timeout, this, &AlarmManager::onAlarmTimeout);
}

AlarmManager::~AlarmManager()
{
}

void AlarmManager::setThresholds(const AlarmThresholds& thresholds)
{
    m_thresholds = thresholds;
}

void AlarmManager::checkVitalData(const VitalData& data)
{
    if (data.temperature > 0) {
        checkTemperature(data.temperature);
    }
    if (data.heartRate > 0) {
        checkHeartRate(data.heartRate);
    }
    if (data.bloodOxygen > 0) {
        checkBloodOxygen(data.bloodOxygen);
    }
}

void AlarmManager::checkTemperature(double temp)
{
    if (temp >= m_thresholds.tempHigh) {
        triggerAlarm(AlarmInfo::HighTemperature, temp);
    } else if (temp <= m_thresholds.tempLow) {
        triggerAlarm(AlarmInfo::LowTemperature, temp);
    }
}

void AlarmManager::checkHeartRate(int hr)
{
    if (hr >= m_thresholds.hrHigh) {
        triggerAlarm(AlarmInfo::HighHeartRate, hr);
    } else if (hr <= m_thresholds.hrLow) {
        triggerAlarm(AlarmInfo::LowHeartRate, hr);
    }
}

void AlarmManager::checkBloodOxygen(int spo2)
{
    if (spo2 <= m_thresholds.spo2Low) {
        triggerAlarm(AlarmInfo::LowBloodOxygen, spo2);
    }
}

void AlarmManager::triggerAlarm(AlarmInfo::AlarmType type, double value)
{
    // 检查冷却时间
    if (m_lastAlarmTime.isValid() && 
        m_lastAlarmTime.secsTo(QDateTime::currentDateTime()) < m_alarmCooldown) {
        return;
    }
    
    // 检查是否已有相同类型的活动报警
    for (const AlarmInfo& alarm : m_activeAlarms) {
        if (alarm.type == type && !alarm.acknowledged) {
            return;
        }
    }
    
    AlarmInfo alarm;
    alarm.type = type;
    alarm.value = value;
    alarm.timestamp = QDateTime::currentDateTime();
    alarm.message = QString("%1: %2").arg(AlarmInfo::typeToString(type)).arg(value);
    
    m_activeAlarms.append(alarm);
    m_lastAlarmTime = QDateTime::currentDateTime();
    
    // 保存到数据库
    if (m_dataManager) {
        m_dataManager->saveAlarm(alarm);
    }
    
    emit alarmTriggered(alarm);
    emit activeAlarmsChanged(m_activeAlarms.size());
    
    if (m_soundEnabled) {
        playAlarmSound();
    }
    
    // 启动闪烁定时器
    if (!m_alarmTimer->isActive()) {
        m_alarmTimer->start(500);
    }
}

void AlarmManager::playAlarmSound()
{
    // 使用系统提示音
    QApplication::beep();
}

void AlarmManager::acknowledgeAllAlarms()
{
    for (AlarmInfo& alarm : m_activeAlarms) {
        alarm.acknowledged = true;
    }
    
    m_activeAlarms.clear();
    m_alarmTimer->stop();
    
    emit alarmCleared();
    emit activeAlarmsChanged(0);
}

void AlarmManager::onAlarmTimeout()
{
    // 用于触发UI闪烁效果
    static bool toggle = false;
    toggle = !toggle;
    
    if (m_activeAlarms.isEmpty()) {
        m_alarmTimer->stop();
        emit alarmCleared();
    }
}
