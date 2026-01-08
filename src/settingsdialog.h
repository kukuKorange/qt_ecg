#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QTabWidget>
#include "vitaldata.h"

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog();

    // MQTT设置
    QString getMqttHost() const;
    quint16 getMqttPort() const;
    QString getMqttUsername() const;
    QString getMqttPassword() const;
    QString getTempTopic() const;
    QString getHrTopic() const;
    QString getSpo2Topic() const;
    QString getEcgTopic() const;
    
    void setMqttSettings(const QString& host, quint16 port,
                         const QString& username, const QString& password);
    void setMqttTopics(const QString& temp, const QString& hr,
                       const QString& spo2, const QString& ecg);
    
    // 报警阈值
    AlarmThresholds getAlarmThresholds() const;
    void setAlarmThresholds(const AlarmThresholds& thresholds);
    
    // 云同步设置
    QString getCloudServerUrl() const;
    QString getCloudApiKey() const;
    QString getDeviceId() const;
    bool isCloudEnabled() const;
    int getAutoSyncInterval() const;
    
    void setCloudSettings(const QString& url, const QString& apiKey,
                          const QString& deviceId, bool enabled, int interval);
    
    // 显示设置
    int getEcgDisplayDuration() const;
    int getVitalsTimeRange() const;
    bool isAlarmSoundEnabled() const;
    
    void setDisplaySettings(int ecgDuration, int vitalsRange, bool alarmSound);

signals:
    void settingsChanged();
    void mqttTestRequested(const QString& host, quint16 port,
                           const QString& username, const QString& password);

private slots:
    void onTestMqttClicked();
    void onTestCloudClicked();
    void onAccepted();
    void onResetDefaults();

private:
    void setupUI();
    void loadSettings();
    void saveSettings();
    
    QTabWidget* m_tabWidget;
    
    // MQTT设置控件
    QLineEdit* m_mqttHostEdit;
    QSpinBox* m_mqttPortSpin;
    QLineEdit* m_mqttUserEdit;
    QLineEdit* m_mqttPassEdit;
    QLineEdit* m_tempTopicEdit;
    QLineEdit* m_hrTopicEdit;
    QLineEdit* m_spo2TopicEdit;
    QLineEdit* m_ecgTopicEdit;
    QPushButton* m_testMqttButton;
    
    // 报警阈值控件
    QDoubleSpinBox* m_tempHighSpin;
    QDoubleSpinBox* m_tempLowSpin;
    QSpinBox* m_hrHighSpin;
    QSpinBox* m_hrLowSpin;
    QSpinBox* m_spo2LowSpin;
    QCheckBox* m_alarmSoundCheck;
    
    // 云同步控件
    QLineEdit* m_cloudUrlEdit;
    QLineEdit* m_cloudApiKeyEdit;
    QLineEdit* m_deviceIdEdit;
    QCheckBox* m_cloudEnabledCheck;
    QSpinBox* m_syncIntervalSpin;
    QPushButton* m_testCloudButton;
    
    // 显示设置控件
    QSpinBox* m_ecgDurationSpin;
    QComboBox* m_vitalsRangeCombo;
};
