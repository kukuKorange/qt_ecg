#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QFrame>
#include "mqttclient.h"
#include "datamanager.h"
#include "alarmmanager.h"
#include "cloudsyncer.h"
#include "ecgchartwidget.h"
#include "vitalschartwidget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    // MQTT slots
    void onMqttConnected();
    void onMqttDisconnected();
    void onMqttError(const QString& error);
    void onMqttStatusChanged(const QString& status);
    
    // Data slots
    void onTemperatureReceived(double temp);
    void onHeartRateReceived(int hr);
    void onBloodOxygenReceived(int spo2);
    void onEcgDataReceived(const QVector<double>& data);
    
    // Alarm slots
    void onAlarmTriggered(const AlarmInfo& alarm);
    void onAlarmCleared();
    
    // UI slots
    void onConnectClicked();
    void onSettingsClicked();
    void onHistoryClicked();
    void onAcknowledgeAlarmClicked();
    void onSimulateDataClicked();
    
    // Timer slots
    void onUpdateTimer();
    void onSimulationTimer();

private:
    void setupUI();
    void setupConnections();
    void loadSettings();
    void saveSettings();
    void updateConnectionStatus(bool connected);
    void updateVitalDisplay(const QString& type, double value, const QString& unit);
    void showAlarmIndicator(bool show);
    
    QWidget* createVitalCard(const QString& title, const QString& value, 
                              const QString& unit, const QColor& color, 
                              QLabel** valueLabel);
    QWidget* createStatusCard();
    QWidget* createSidebar();
    QWidget* createDashboard();
    
    // Core components
    MqttClient* m_mqttClient;
    DataManager* m_dataManager;
    AlarmManager* m_alarmManager;
    CloudSyncer* m_cloudSyncer;
    
    // Charts
    EcgChartWidget* m_ecgChart;
    VitalsChartWidget* m_vitalsChart;
    
    // Vital value labels
    QLabel* m_tempValueLabel;
    QLabel* m_hrValueLabel;
    QLabel* m_spo2ValueLabel;
    
    // Status labels
    QLabel* m_connectionStatusLabel;
    QLabel* m_mqttStatusLabel;
    QLabel* m_cloudStatusLabel;
    QLabel* m_recordCountLabel;
    QLabel* m_lastUpdateLabel;
    QLabel* m_alarmCountLabel;
    
    // Buttons
    QPushButton* m_connectButton;
    QPushButton* m_settingsButton;
    QPushButton* m_historyButton;
    QPushButton* m_acknowledgeButton;
    QPushButton* m_simulateButton;
    
    // Alarm indicator
    QFrame* m_alarmFrame;
    QLabel* m_alarmLabel;
    
    // Timers
    QTimer* m_updateTimer;
    QTimer* m_simulationTimer;
    
    // Current values
    double m_currentTemp = 0.0;
    int m_currentHr = 0;
    int m_currentSpo2 = 0;
    
    // Simulation
    bool m_simulating = false;
    double m_simPhase = 0.0;
};
