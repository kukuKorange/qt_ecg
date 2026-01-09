#include "mainwindow.h"
#include "settingsdialog.h"
#include "historydialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QScrollArea>
#include <QSettings>
#include <QMessageBox>
#include <QApplication>
#include <QtMath>
#include <QRandomGenerator>
#include <QToolBar>
#include <QStatusBar>
#include <QScreen>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_mqttClient(new MqttClient(this))
    , m_dataManager(new DataManager(this))
    , m_alarmManager(new AlarmManager(m_dataManager, this))
    , m_cloudSyncer(new CloudSyncer(m_dataManager, this))
    , m_updateTimer(new QTimer(this))
    , m_simulationTimer(new QTimer(this))
{
    m_dataManager->initialize();
    
    setupUI();
    setupConnections();
    loadSettings();
    
    m_updateTimer->start(1000);
}

MainWindow::~MainWindow()
{
    saveSettings();
}

void MainWindow::setupUI()
{
    setWindowTitle(QStringLiteral("健康监护"));
    
    // 适配移动端尺寸 (竖屏手机比例)
    setMinimumSize(360, 640);
    resize(400, 720);
    
    // 简洁深色主题
    QString styleSheet = R"(
        * {
            font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
        }
        QMainWindow, QWidget {
            background-color: #1a1a2e;
            color: #eaeaea;
        }
        QToolBar {
            background-color: #16213e;
            border: none;
            padding: 8px;
            spacing: 12px;
        }
        QToolBar QToolButton {
            background-color: transparent;
            border: none;
            border-radius: 8px;
            padding: 10px 16px;
            color: #eaeaea;
            font-size: 14px;
        }
        QToolBar QToolButton:hover {
            background-color: #1f4068;
        }
        QToolBar QToolButton:pressed {
            background-color: #162447;
        }
        QPushButton {
            background-color: #1f4068;
            border: none;
            border-radius: 8px;
            padding: 14px 20px;
            color: white;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #2a5a8c;
        }
        QPushButton:pressed {
            background-color: #16213e;
        }
        QPushButton#connectBtn {
            background-color: #0f9b6e;
        }
        QPushButton#connectBtn:hover {
            background-color: #12b886;
        }
        QStatusBar {
            background-color: #16213e;
            color: #8892b0;
            font-size: 12px;
        }
        QLabel#statusLabel {
            color: #8892b0;
        }
    )";
    setStyleSheet(styleSheet);
    
    // 创建工具栏
    QToolBar* toolbar = new QToolBar();
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(24, 24));
    
    m_connectButton = new QPushButton(QStringLiteral("连接"));
    m_connectButton->setObjectName("connectBtn");
    m_connectButton->setFixedWidth(80);
    toolbar->addWidget(m_connectButton);
    
    toolbar->addSeparator();
    
    m_simulateButton = new QPushButton(QStringLiteral("模拟"));
    m_simulateButton->setFixedWidth(80);
    toolbar->addWidget(m_simulateButton);
    
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);
    
    m_settingsButton = new QPushButton(QStringLiteral("设置"));
    m_settingsButton->setFixedWidth(80);
    toolbar->addWidget(m_settingsButton);
    
    m_historyButton = new QPushButton(QStringLiteral("历史"));
    m_historyButton->setFixedWidth(80);
    toolbar->addWidget(m_historyButton);
    
    addToolBar(Qt::TopToolBarArea, toolbar);
    
    // 主内容区
    QWidget* centralWidget = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    
    // 生命体征卡片区域
    QWidget* vitalsWidget = new QWidget();
    QGridLayout* vitalsGrid = new QGridLayout(vitalsWidget);
    vitalsGrid->setSpacing(12);
    vitalsGrid->setContentsMargins(0, 0, 0, 0);
    
    // 体温卡片
    QWidget* tempCard = createVitalCard(
        QStringLiteral("体温"), "--.-", QStringLiteral("°C"),
        QColor("#e74c3c"), &m_tempValueLabel);
    vitalsGrid->addWidget(tempCard, 0, 0);
    
    // 心率卡片
    QWidget* hrCard = createVitalCard(
        QStringLiteral("心率"), "---", QStringLiteral("bpm"),
        QColor("#3498db"), &m_hrValueLabel);
    vitalsGrid->addWidget(hrCard, 0, 1);
    
    // 血氧卡片
    QWidget* spo2Card = createVitalCard(
        QStringLiteral("血氧"), "---", QStringLiteral("%"),
        QColor("#2ecc71"), &m_spo2ValueLabel);
    vitalsGrid->addWidget(spo2Card, 1, 0);
    
    // 状态卡片
    QWidget* statusCard = createStatusCard();
    vitalsGrid->addWidget(statusCard, 1, 1);
    
    mainLayout->addWidget(vitalsWidget);
    
    // 心电图区域
    QWidget* ecgContainer = new QWidget();
    ecgContainer->setStyleSheet(R"(
        QWidget {
            background-color: #16213e;
            border-radius: 12px;
        }
    )");
    QVBoxLayout* ecgLayout = new QVBoxLayout(ecgContainer);
    ecgLayout->setContentsMargins(12, 8, 12, 12);
    ecgLayout->setSpacing(4);
    
    QLabel* ecgTitle = new QLabel(QStringLiteral("心电图"));
    ecgTitle->setStyleSheet("color: #8892b0; font-size: 13px; font-weight: bold;");
    ecgLayout->addWidget(ecgTitle);
    
    m_ecgChart = new EcgChartWidget();
    m_ecgChart->setMinimumHeight(180);
    m_ecgChart->setLineColor(QColor("#00d9ff"));
    m_ecgChart->setBackgroundColor(QColor("#16213e"));
    m_ecgChart->setGridColor(QColor("#1f4068"));
    ecgLayout->addWidget(m_ecgChart);
    
    mainLayout->addWidget(ecgContainer, 1);
    
    // 报警区域 (初始隐藏)
    m_alarmFrame = new QFrame();
    m_alarmFrame->setStyleSheet(R"(
        QFrame {
            background-color: #c0392b;
            border-radius: 10px;
            padding: 12px;
        }
    )");
    m_alarmFrame->setVisible(false);
    
    QHBoxLayout* alarmLayout = new QHBoxLayout(m_alarmFrame);
    alarmLayout->setContentsMargins(16, 12, 16, 12);
    
    m_alarmLabel = new QLabel(QStringLiteral("报警信息"));
    m_alarmLabel->setStyleSheet("color: white; font-weight: bold; font-size: 14px;");
    alarmLayout->addWidget(m_alarmLabel, 1);
    
    m_acknowledgeButton = new QPushButton(QStringLiteral("确认"));
    m_acknowledgeButton->setFixedSize(70, 36);
    m_acknowledgeButton->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(255,255,255,0.2);
            border-radius: 6px;
        }
    )");
    alarmLayout->addWidget(m_acknowledgeButton);
    
    mainLayout->addWidget(m_alarmFrame);
    
    setCentralWidget(centralWidget);
    
    // 状态栏
    m_connectionStatusLabel = new QLabel(QStringLiteral("未连接"));
    m_connectionStatusLabel->setObjectName("statusLabel");
    statusBar()->addWidget(m_connectionStatusLabel);
    
    m_lastUpdateLabel = new QLabel();
    m_lastUpdateLabel->setObjectName("statusLabel");
    statusBar()->addPermanentWidget(m_lastUpdateLabel);
    
    // 隐藏未使用的成员
    m_mqttStatusLabel = m_connectionStatusLabel;
    m_cloudStatusLabel = new QLabel();
    m_recordCountLabel = new QLabel();
    m_alarmCountLabel = new QLabel();
    m_vitalsChart = nullptr;  // 移动端不显示趋势图
}

QWidget* MainWindow::createVitalCard(const QString& title, const QString& value,
                                      const QString& unit, const QColor& color,
                                      QLabel** valueLabel)
{
    QFrame* card = new QFrame();
    card->setStyleSheet(QString(R"(
        QFrame {
            background-color: #16213e;
            border-radius: 12px;
            border-left: 4px solid %1;
        }
    )").arg(color.name()));
    
    QVBoxLayout* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);
    
    // 标题
    QLabel* titleLabel = new QLabel(title);
    titleLabel->setStyleSheet("color: #8892b0; font-size: 13px; font-weight: bold;");
    layout->addWidget(titleLabel);
    
    // 数值行
    QHBoxLayout* valueLayout = new QHBoxLayout();
    valueLayout->setSpacing(4);
    valueLayout->setAlignment(Qt::AlignBottom);
    
    *valueLabel = new QLabel(value);
    (*valueLabel)->setStyleSheet(QString(R"(
        color: %1;
        font-size: 36px;
        font-weight: bold;
        font-family: "Consolas", "Monaco", monospace;
    )").arg(color.name()));
    valueLayout->addWidget(*valueLabel);
    
    QLabel* unitLabel = new QLabel(unit);
    unitLabel->setStyleSheet(QString("color: %1; font-size: 14px;").arg(color.darker(120).name()));
    valueLayout->addWidget(unitLabel);
    valueLayout->addStretch();
    
    layout->addLayout(valueLayout);
    
    return card;
}

QWidget* MainWindow::createStatusCard()
{
    QFrame* card = new QFrame();
    card->setStyleSheet(R"(
        QFrame {
            background-color: #16213e;
            border-radius: 12px;
        }
    )");
    
    QVBoxLayout* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    
    QLabel* titleLabel = new QLabel(QStringLiteral("状态"));
    titleLabel->setStyleSheet("color: #8892b0; font-size: 13px; font-weight: bold;");
    layout->addWidget(titleLabel);
    
    // 连接指示器
    QHBoxLayout* connLayout = new QHBoxLayout();
    QLabel* connDot = new QLabel(QStringLiteral("●"));
    connDot->setObjectName("connDot");
    connDot->setStyleSheet("color: #e74c3c; font-size: 12px;");
    connLayout->addWidget(connDot);
    
    QLabel* connText = new QLabel(QStringLiteral("MQTT"));
    connText->setStyleSheet("color: #eaeaea; font-size: 13px;");
    connLayout->addWidget(connText);
    connLayout->addStretch();
    
    layout->addLayout(connLayout);
    layout->addStretch();
    
    return card;
}

QWidget* MainWindow::createSidebar()
{
    return nullptr; // 移动端不需要侧边栏
}

QWidget* MainWindow::createDashboard()
{
    return nullptr; // 使用新的简洁布局
}

void MainWindow::setupConnections()
{
    // MQTT连接
    connect(m_mqttClient, &MqttClient::connected, this, &MainWindow::onMqttConnected);
    connect(m_mqttClient, &MqttClient::disconnected, this, &MainWindow::onMqttDisconnected);
    connect(m_mqttClient, &MqttClient::connectionError, this, &MainWindow::onMqttError);
    connect(m_mqttClient, &MqttClient::statusChanged, this, &MainWindow::onMqttStatusChanged);
    
    // 数据接收
    connect(m_mqttClient, &MqttClient::temperatureReceived, this, &MainWindow::onTemperatureReceived);
    connect(m_mqttClient, &MqttClient::heartRateReceived, this, &MainWindow::onHeartRateReceived);
    connect(m_mqttClient, &MqttClient::bloodOxygenReceived, this, &MainWindow::onBloodOxygenReceived);
    connect(m_mqttClient, &MqttClient::ecgDataReceived, this, &MainWindow::onEcgDataReceived);
    
    // 报警
    connect(m_alarmManager, &AlarmManager::alarmTriggered, this, &MainWindow::onAlarmTriggered);
    connect(m_alarmManager, &AlarmManager::alarmCleared, this, &MainWindow::onAlarmCleared);
    
    // 按钮
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(m_settingsButton, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    connect(m_historyButton, &QPushButton::clicked, this, &MainWindow::onHistoryClicked);
    connect(m_acknowledgeButton, &QPushButton::clicked, this, &MainWindow::onAcknowledgeAlarmClicked);
    connect(m_simulateButton, &QPushButton::clicked, this, &MainWindow::onSimulateDataClicked);
    
    // 定时器
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::onUpdateTimer);
    connect(m_simulationTimer, &QTimer::timeout, this, &MainWindow::onSimulationTimer);
}

void MainWindow::loadSettings()
{
    QSettings settings("HealthMonitor", "QtECG");
    
    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
    }
    
    AlarmThresholds thresholds;
    thresholds.tempHigh = settings.value("alarm/tempHigh", 37.5).toDouble();
    thresholds.tempLow = settings.value("alarm/tempLow", 35.0).toDouble();
    thresholds.hrHigh = settings.value("alarm/hrHigh", 100).toInt();
    thresholds.hrLow = settings.value("alarm/hrLow", 50).toInt();
    thresholds.spo2Low = settings.value("alarm/spo2Low", 90).toInt();
    m_alarmManager->setThresholds(thresholds);
    m_alarmManager->setSoundEnabled(settings.value("alarm/soundEnabled", true).toBool());
    
    m_cloudSyncer->setEnabled(settings.value("cloud/enabled", false).toBool());
    m_cloudSyncer->setServerUrl(settings.value("cloud/url", "").toString());
    m_cloudSyncer->setApiKey(settings.value("cloud/apiKey", "").toString());
    m_cloudSyncer->setDeviceId(settings.value("cloud/deviceId", "device_001").toString());
    
    m_ecgChart->setDisplayDuration(settings.value("display/ecgDuration", 5).toInt());
}

void MainWindow::saveSettings()
{
    QSettings settings("HealthMonitor", "QtECG");
    settings.setValue("geometry", saveGeometry());
}

void MainWindow::onMqttConnected()
{
    updateConnectionStatus(true);
}

void MainWindow::onMqttDisconnected()
{
    updateConnectionStatus(false);
}

void MainWindow::onMqttError(const QString& error)
{
    m_connectionStatusLabel->setText(QStringLiteral("错误: %1").arg(error));
    QMessageBox::warning(this, QStringLiteral("连接错误"), error);
}

void MainWindow::onMqttStatusChanged(const QString& status)
{
    m_connectionStatusLabel->setText(status);
}

void MainWindow::updateConnectionStatus(bool connected)
{
    // 更新状态栏
    if (connected) {
        m_connectionStatusLabel->setText(QStringLiteral("已连接"));
        m_connectButton->setText(QStringLiteral("断开"));
        m_connectButton->setStyleSheet(R"(
            QPushButton {
                background-color: #e74c3c;
                border: none;
                border-radius: 8px;
                padding: 14px 20px;
                color: white;
                font-weight: bold;
            }
        )");
    } else {
        m_connectionStatusLabel->setText(QStringLiteral("未连接"));
        m_connectButton->setText(QStringLiteral("连接"));
        m_connectButton->setStyleSheet("");
        m_connectButton->setObjectName("connectBtn");
    }
    
    // 更新状态卡片中的指示点
    QLabel* connDot = findChild<QLabel*>("connDot");
    if (connDot) {
        connDot->setStyleSheet(connected ? 
            "color: #2ecc71; font-size: 12px;" : 
            "color: #e74c3c; font-size: 12px;");
    }
}

void MainWindow::onTemperatureReceived(double temp)
{
    m_currentTemp = temp;
    m_tempValueLabel->setText(QString::number(temp, 'f', 1));
    
    m_dataManager->saveTemperature(temp);
    m_alarmManager->checkTemperature(temp);
    
    m_lastUpdateLabel->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
}

void MainWindow::onHeartRateReceived(int hr)
{
    m_currentHr = hr;
    m_hrValueLabel->setText(QString::number(hr));
    
    m_dataManager->saveHeartRate(hr);
    m_alarmManager->checkHeartRate(hr);
    
    m_lastUpdateLabel->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
}

void MainWindow::onBloodOxygenReceived(int spo2)
{
    m_currentSpo2 = spo2;
    m_spo2ValueLabel->setText(QString::number(spo2));
    
    m_dataManager->saveBloodOxygen(spo2);
    m_alarmManager->checkBloodOxygen(spo2);
    
    m_lastUpdateLabel->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
}

void MainWindow::onEcgDataReceived(const QVector<double>& data)
{
    m_ecgChart->addDataPoints(data);
    m_dataManager->saveEcgData(data);
    
    m_lastUpdateLabel->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
}

void MainWindow::onAlarmTriggered(const AlarmInfo& alarm)
{
    showAlarmIndicator(true);
    m_alarmLabel->setText(alarm.message);
    m_acknowledgeButton->setEnabled(true);
}

void MainWindow::onAlarmCleared()
{
    showAlarmIndicator(false);
    m_acknowledgeButton->setEnabled(false);
}

void MainWindow::showAlarmIndicator(bool show)
{
    m_alarmFrame->setVisible(show);
}

void MainWindow::onConnectClicked()
{
    if (m_mqttClient->isConnected()) {
        m_mqttClient->disconnectFromHost();
    } else {
        QSettings settings("HealthMonitor", "QtECG");
        QString host = settings.value("mqtt/host", "47.115.148.200").toString();
        quint16 port = settings.value("mqtt/port", 1883).toInt();
        QString username = settings.value("mqtt/username", "").toString();
        QString password = settings.value("mqtt/password", "").toString();
        
        m_mqttClient->setTopics(
            settings.value("mqtt/tempTopic", "health/temperature").toString(),
            settings.value("mqtt/hrTopic", "health/heartrate").toString(),
            settings.value("mqtt/spo2Topic", "health/spo2").toString(),
            settings.value("mqtt/ecgTopic", "health/ecg").toString()
        );
        
        m_mqttClient->connectToHost(host, port, username, password);
    }
}

void MainWindow::onSettingsClicked()
{
    SettingsDialog dialog(this);
    
    if (dialog.exec() == QDialog::Accepted) {
        loadSettings();
        
        m_mqttClient->setTopics(
            dialog.getTempTopic(),
            dialog.getHrTopic(),
            dialog.getSpo2Topic(),
            dialog.getEcgTopic()
        );
        
        m_alarmManager->setThresholds(dialog.getAlarmThresholds());
        m_alarmManager->setSoundEnabled(dialog.isAlarmSoundEnabled());
        
        m_cloudSyncer->setEnabled(dialog.isCloudEnabled());
        m_cloudSyncer->setServerUrl(dialog.getCloudServerUrl());
        m_cloudSyncer->setApiKey(dialog.getCloudApiKey());
        m_cloudSyncer->setDeviceId(dialog.getDeviceId());
        
        m_ecgChart->setDisplayDuration(dialog.getEcgDisplayDuration());
    }
}

void MainWindow::onHistoryClicked()
{
    HistoryDialog dialog(m_dataManager, this);
    dialog.exec();
}

void MainWindow::onAcknowledgeAlarmClicked()
{
    m_alarmManager->acknowledgeAllAlarms();
}

void MainWindow::onSimulateDataClicked()
{
    m_simulating = !m_simulating;
    
    if (m_simulating) {
        m_simulateButton->setText(QStringLiteral("停止"));
        m_simulateButton->setStyleSheet("background-color: #e67e22;");
        m_simulationTimer->start(40);
    } else {
        m_simulateButton->setText(QStringLiteral("模拟"));
        m_simulateButton->setStyleSheet("");
        m_simulationTimer->stop();
    }
}

void MainWindow::onUpdateTimer()
{
    // 更新时间等
}

void MainWindow::onSimulationTimer()
{
    static int ecgCounter = 0;
    QVector<double> ecgData;
    
    for (int i = 0; i < 5; ++i) {
        double t = m_simPhase;
        double value = 0;
        
        // P波
        if (fmod(t, 1.0) < 0.1) {
            value = 0.15 * qSin(M_PI * fmod(t, 1.0) / 0.1);
        }
        // QRS波群
        else if (fmod(t, 1.0) >= 0.15 && fmod(t, 1.0) < 0.17) {
            value = -0.2;
        }
        else if (fmod(t, 1.0) >= 0.17 && fmod(t, 1.0) < 0.20) {
            value = 1.0 * qSin(M_PI * (fmod(t, 1.0) - 0.17) / 0.03);
        }
        else if (fmod(t, 1.0) >= 0.20 && fmod(t, 1.0) < 0.22) {
            value = -0.3;
        }
        // T波
        else if (fmod(t, 1.0) >= 0.30 && fmod(t, 1.0) < 0.45) {
            value = 0.25 * qSin(M_PI * (fmod(t, 1.0) - 0.30) / 0.15);
        }
        
        value += (QRandomGenerator::global()->bounded(100) - 50) / 500.0;
        
        ecgData.append(value);
        m_simPhase += 1.0 / 250.0;
    }
    
    m_ecgChart->addDataPoints(ecgData);
    
    ecgCounter++;
    if (ecgCounter >= 25) {
        ecgCounter = 0;
        
        double temp = 36.5 + QRandomGenerator::global()->bounded(100) / 100.0;
        onTemperatureReceived(temp);
        
        int hr = 70 + QRandomGenerator::global()->bounded(30);
        onHeartRateReceived(hr);
        
        int spo2 = 96 + QRandomGenerator::global()->bounded(4);
        onBloodOxygenReceived(spo2);
    }
}

void MainWindow::updateVitalDisplay(const QString& type, double value, const QString& unit)
{
    Q_UNUSED(type)
    Q_UNUSED(value)
    Q_UNUSED(unit)
}
