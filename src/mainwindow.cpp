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
#include <QGraphicsDropShadowEffect>
#include <QRandomGenerator>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_mqttClient(new MqttClient(this))
    , m_dataManager(new DataManager(this))
    , m_alarmManager(new AlarmManager(m_dataManager, this))
    , m_cloudSyncer(new CloudSyncer(m_dataManager, this))
    , m_updateTimer(new QTimer(this))
    , m_simulationTimer(new QTimer(this))
{
    // 初始化数据库
    m_dataManager->initialize();
    
    setupUI();
    setupConnections();
    loadSettings();
    
    // 启动更新定时器
    m_updateTimer->start(1000);
}

MainWindow::~MainWindow()
{
    saveSettings();
}

void MainWindow::setupUI()
{
    setWindowTitle(QStringLiteral("智能健康监护系统"));
    setMinimumSize(1400, 900);
    
    // 设置深色主题样式
    QString styleSheet = R"(
        QMainWindow {
            background-color: #050d1a;
        }
        QWidget {
            color: white;
            font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
        }
        QScrollArea {
            border: none;
            background-color: transparent;
        }
        QGroupBox {
            border: 1px solid #1a3a5c;
            border-radius: 12px;
            margin-top: 15px;
            padding: 20px;
            background-color: #0a1628;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 20px;
            padding: 0 10px;
            color: #4ecdc4;
            font-weight: bold;
            font-size: 14px;
        }
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #1a4a7c, stop:1 #0d2a4c);
            border: 1px solid #2a6a9c;
            border-radius: 8px;
            padding: 12px 24px;
            color: white;
            font-weight: bold;
            font-size: 13px;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #2a5a8c, stop:1 #1a4a6c);
            border-color: #3a8acc;
        }
        QPushButton:pressed {
            background: #0d2a4c;
        }
        QPushButton:disabled {
            background: #1a2a3c;
            color: #5a6a7c;
            border-color: #2a3a4c;
        }
        QPushButton#connectButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #2a8a5c, stop:1 #1a6a4c);
            border-color: #3aaa7c;
        }
        QPushButton#connectButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #3a9a6c, stop:1 #2a7a5c);
        }
        QPushButton#simulateButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #8a5a2a, stop:1 #6a4a1a);
            border-color: #aa7a3a;
        }
        QLabel#statusLabel {
            color: #7a9abc;
            font-size: 12px;
        }
    )";
    
    setStyleSheet(styleSheet);
    
    // 创建主布局
    QWidget* centralWidget = new QWidget();
    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // 创建侧边栏
    QWidget* sidebar = createSidebar();
    sidebar->setFixedWidth(280);
    mainLayout->addWidget(sidebar);
    
    // 创建仪表盘
    QWidget* dashboard = createDashboard();
    mainLayout->addWidget(dashboard, 1);
    
    setCentralWidget(centralWidget);
}

QWidget* MainWindow::createSidebar()
{
    QWidget* sidebar = new QWidget();
    sidebar->setStyleSheet(R"(
        QWidget {
            background-color: #0a1628;
            border-right: 1px solid #1a3a5c;
        }
    )");
    
    QVBoxLayout* layout = new QVBoxLayout(sidebar);
    layout->setSpacing(15);
    layout->setContentsMargins(20, 30, 20, 30);
    
    // Logo和标题
    QLabel* titleLabel = new QLabel(QStringLiteral("🏥 智能健康监护"));
    titleLabel->setStyleSheet(R"(
        font-size: 20px;
        font-weight: bold;
        color: #4ecdc4;
        padding: 15px 0;
        border-bottom: 1px solid #1a3a5c;
    )");
    layout->addWidget(titleLabel);
    
    layout->addSpacing(20);
    
    // 连接状态卡片
    QFrame* statusCard = new QFrame();
    statusCard->setStyleSheet(R"(
        QFrame {
            background-color: #0d1f35;
            border-radius: 10px;
            padding: 15px;
        }
    )");
    QVBoxLayout* statusLayout = new QVBoxLayout(statusCard);
    
    QLabel* statusTitle = new QLabel(QStringLiteral("📡 连接状态"));
    statusTitle->setStyleSheet("font-weight: bold; color: #4ecdc4; font-size: 14px;");
    statusLayout->addWidget(statusTitle);
    
    m_connectionStatusLabel = new QLabel(QStringLiteral("● 未连接"));
    m_connectionStatusLabel->setStyleSheet("color: #ff6b6b; font-size: 16px; font-weight: bold;");
    statusLayout->addWidget(m_connectionStatusLabel);
    
    m_mqttStatusLabel = new QLabel(QStringLiteral("等待连接..."));
    m_mqttStatusLabel->setStyleSheet("color: #7a9abc; font-size: 12px;");
    m_mqttStatusLabel->setWordWrap(true);
    statusLayout->addWidget(m_mqttStatusLabel);
    
    layout->addWidget(statusCard);
    
    // 数据统计卡片
    QFrame* statsCard = new QFrame();
    statsCard->setStyleSheet(R"(
        QFrame {
            background-color: #0d1f35;
            border-radius: 10px;
            padding: 15px;
        }
    )");
    QVBoxLayout* statsLayout = new QVBoxLayout(statsCard);
    
    QLabel* statsTitle = new QLabel(QStringLiteral("📊 数据统计"));
    statsTitle->setStyleSheet("font-weight: bold; color: #4ecdc4; font-size: 14px;");
    statsLayout->addWidget(statsTitle);
    
    m_recordCountLabel = new QLabel(QStringLiteral("记录数: 0"));
    m_recordCountLabel->setStyleSheet("color: #b0c4de; font-size: 13px;");
    statsLayout->addWidget(m_recordCountLabel);
    
    m_lastUpdateLabel = new QLabel(QStringLiteral("最后更新: --"));
    m_lastUpdateLabel->setStyleSheet("color: #7a9abc; font-size: 12px;");
    statsLayout->addWidget(m_lastUpdateLabel);
    
    m_cloudStatusLabel = new QLabel(QStringLiteral("☁️ 云同步: 未启用"));
    m_cloudStatusLabel->setStyleSheet("color: #7a9abc; font-size: 12px;");
    statsLayout->addWidget(m_cloudStatusLabel);
    
    layout->addWidget(statsCard);
    
    // 报警状态卡片
    m_alarmFrame = new QFrame();
    m_alarmFrame->setStyleSheet(R"(
        QFrame {
            background-color: #2a1a1a;
            border: 1px solid #ff4444;
            border-radius: 10px;
            padding: 15px;
        }
    )");
    m_alarmFrame->setVisible(false);
    
    QVBoxLayout* alarmLayout = new QVBoxLayout(m_alarmFrame);
    
    m_alarmLabel = new QLabel(QStringLiteral("⚠️ 无报警"));
    m_alarmLabel->setStyleSheet("color: #ff6b6b; font-weight: bold; font-size: 14px;");
    alarmLayout->addWidget(m_alarmLabel);
    
    m_alarmCountLabel = new QLabel(QStringLiteral("活动报警: 0"));
    m_alarmCountLabel->setStyleSheet("color: #ff9999; font-size: 12px;");
    alarmLayout->addWidget(m_alarmCountLabel);
    
    m_acknowledgeButton = new QPushButton(QStringLiteral("✓ 确认报警"));
    m_acknowledgeButton->setStyleSheet(R"(
        QPushButton {
            background: #aa3333;
            border: 1px solid #cc4444;
        }
        QPushButton:hover {
            background: #cc4444;
        }
    )");
    m_acknowledgeButton->setEnabled(false);
    alarmLayout->addWidget(m_acknowledgeButton);
    
    layout->addWidget(m_alarmFrame);
    
    layout->addStretch();
    
    // 操作按钮
    m_connectButton = new QPushButton(QStringLiteral("🔗 连接MQTT"));
    m_connectButton->setObjectName("connectButton");
    layout->addWidget(m_connectButton);
    
    m_simulateButton = new QPushButton(QStringLiteral("🔄 模拟数据"));
    m_simulateButton->setObjectName("simulateButton");
    layout->addWidget(m_simulateButton);
    
    m_historyButton = new QPushButton(QStringLiteral("📋 历史查询"));
    layout->addWidget(m_historyButton);
    
    m_settingsButton = new QPushButton(QStringLiteral("⚙️ 设置"));
    layout->addWidget(m_settingsButton);
    
    return sidebar;
}

QWidget* MainWindow::createDashboard()
{
    QWidget* dashboard = new QWidget();
    dashboard->setStyleSheet("background-color: #050d1a;");
    
    QVBoxLayout* layout = new QVBoxLayout(dashboard);
    layout->setSpacing(20);
    layout->setContentsMargins(30, 30, 30, 30);
    
    // 顶部生命体征卡片区域
    QHBoxLayout* vitalsLayout = new QHBoxLayout();
    vitalsLayout->setSpacing(20);
    
    // 体温卡片
    QWidget* tempCard = createVitalCard(
        QStringLiteral("体温"), "🌡️", "--.-", "°C",
        QColor("#ff6b6b"), &m_tempValueLabel);
    vitalsLayout->addWidget(tempCard);
    
    // 心率卡片
    QWidget* hrCard = createVitalCard(
        QStringLiteral("心率"), "❤️", "---", "bpm",
        QColor("#4ecdc4"), &m_hrValueLabel);
    vitalsLayout->addWidget(hrCard);
    
    // 血氧卡片
    QWidget* spo2Card = createVitalCard(
        QStringLiteral("血氧"), "💧", "---", "%",
        QColor("#45b7d1"), &m_spo2ValueLabel);
    vitalsLayout->addWidget(spo2Card);
    
    layout->addLayout(vitalsLayout);
    
    // 图表区域
    QSplitter* chartSplitter = new QSplitter(Qt::Vertical);
    chartSplitter->setStyleSheet(R"(
        QSplitter::handle {
            background-color: #1a3a5c;
            height: 2px;
        }
    )");
    
    // 心电图
    QGroupBox* ecgGroup = new QGroupBox(QStringLiteral("💓 实时心电图 (ECG)"));
    QVBoxLayout* ecgLayout = new QVBoxLayout(ecgGroup);
    m_ecgChart = new EcgChartWidget();
    m_ecgChart->setMinimumHeight(250);
    ecgLayout->addWidget(m_ecgChart);
    chartSplitter->addWidget(ecgGroup);
    
    // 趋势图
    QGroupBox* vitalsGroup = new QGroupBox(QStringLiteral("📈 生命体征趋势"));
    QVBoxLayout* vitalsChartLayout = new QVBoxLayout(vitalsGroup);
    m_vitalsChart = new VitalsChartWidget(VitalsChartWidget::Combined);
    m_vitalsChart->setMinimumHeight(200);
    vitalsChartLayout->addWidget(m_vitalsChart);
    chartSplitter->addWidget(vitalsGroup);
    
    chartSplitter->setSizes({350, 250});
    layout->addWidget(chartSplitter, 1);
    
    return dashboard;
}

QWidget* MainWindow::createVitalCard(const QString& title, const QString& icon,
                                      const QString& value, const QString& unit,
                                      const QColor& color, QLabel** valueLabel)
{
    QFrame* card = new QFrame();
    card->setStyleSheet(QString(R"(
        QFrame {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #0d1f35, stop:1 #0a1628);
            border: 1px solid %1;
            border-radius: 15px;
            padding: 20px;
        }
    )").arg(color.darker(150).name()));
    
    // 添加阴影效果
    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(20);
    shadow->setColor(color);
    shadow->setOffset(0, 0);
    card->setGraphicsEffect(shadow);
    
    QVBoxLayout* layout = new QVBoxLayout(card);
    layout->setSpacing(10);
    
    // 标题行
    QHBoxLayout* titleLayout = new QHBoxLayout();
    QLabel* iconLabel = new QLabel(icon);
    iconLabel->setStyleSheet("font-size: 24px;");
    titleLayout->addWidget(iconLabel);
    
    QLabel* titleLabel = new QLabel(title);
    titleLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 16px;").arg(color.name()));
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    
    layout->addLayout(titleLayout);
    
    // 数值
    *valueLabel = new QLabel(value);
    (*valueLabel)->setStyleSheet(QString(R"(
        color: %1;
        font-size: 48px;
        font-weight: bold;
        font-family: "Consolas", "Monaco", monospace;
    )").arg(color.name()));
    (*valueLabel)->setAlignment(Qt::AlignCenter);
    layout->addWidget(*valueLabel);
    
    // 单位
    QLabel* unitLabel = new QLabel(unit);
    unitLabel->setStyleSheet(QString("color: %1; font-size: 18px;").arg(color.darker(120).name()));
    unitLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(unitLabel);
    
    return card;
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
    
    // 云同步状态
    connect(m_cloudSyncer, &CloudSyncer::statusChanged, [this](const QString& status) {
        m_cloudStatusLabel->setText(QStringLiteral("☁️ %1").arg(status));
    });
    
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
    
    // 窗口几何
    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
    }
    
    // 报警阈值
    AlarmThresholds thresholds;
    thresholds.tempHigh = settings.value("alarm/tempHigh", 37.5).toDouble();
    thresholds.tempLow = settings.value("alarm/tempLow", 35.0).toDouble();
    thresholds.hrHigh = settings.value("alarm/hrHigh", 100).toInt();
    thresholds.hrLow = settings.value("alarm/hrLow", 50).toInt();
    thresholds.spo2Low = settings.value("alarm/spo2Low", 90).toInt();
    m_alarmManager->setThresholds(thresholds);
    m_alarmManager->setSoundEnabled(settings.value("alarm/soundEnabled", true).toBool());
    
    // 云同步设置
    m_cloudSyncer->setEnabled(settings.value("cloud/enabled", false).toBool());
    m_cloudSyncer->setServerUrl(settings.value("cloud/url", "").toString());
    m_cloudSyncer->setApiKey(settings.value("cloud/apiKey", "").toString());
    m_cloudSyncer->setDeviceId(settings.value("cloud/deviceId", "device_001").toString());
    
    if (m_cloudSyncer->isEnabled()) {
        m_cloudSyncer->startAutoSync(settings.value("cloud/syncInterval", 60).toInt());
    }
    
    // 显示设置
    m_ecgChart->setDisplayDuration(settings.value("display/ecgDuration", 5).toInt());
    m_vitalsChart->setTimeRange(settings.value("display/vitalsRange", 60).toInt());
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
    m_mqttStatusLabel->setText(QStringLiteral("错误: %1").arg(error));
    QMessageBox::warning(this, QStringLiteral("连接错误"), error);
}

void MainWindow::onMqttStatusChanged(const QString& status)
{
    m_mqttStatusLabel->setText(status);
}

void MainWindow::updateConnectionStatus(bool connected)
{
    if (connected) {
        m_connectionStatusLabel->setText(QStringLiteral("● 已连接"));
        m_connectionStatusLabel->setStyleSheet("color: #4ecdc4; font-size: 16px; font-weight: bold;");
        m_connectButton->setText(QStringLiteral("🔌 断开连接"));
    } else {
        m_connectionStatusLabel->setText(QStringLiteral("● 未连接"));
        m_connectionStatusLabel->setStyleSheet("color: #ff6b6b; font-size: 16px; font-weight: bold;");
        m_connectButton->setText(QStringLiteral("🔗 连接MQTT"));
    }
}

void MainWindow::onTemperatureReceived(double temp)
{
    m_currentTemp = temp;
    m_tempValueLabel->setText(QString::number(temp, 'f', 1));
    
    // 保存到数据库
    m_dataManager->saveTemperature(temp);
    
    // 更新趋势图
    m_vitalsChart->addTemperaturePoint(temp);
    
    // 检查报警
    m_alarmManager->checkTemperature(temp);
    
    // 更新最后时间
    m_lastUpdateLabel->setText(QStringLiteral("最后更新: %1")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
}

void MainWindow::onHeartRateReceived(int hr)
{
    m_currentHr = hr;
    m_hrValueLabel->setText(QString::number(hr));
    
    m_dataManager->saveHeartRate(hr);
    m_vitalsChart->addHeartRatePoint(hr);
    m_alarmManager->checkHeartRate(hr);
    
    m_lastUpdateLabel->setText(QStringLiteral("最后更新: %1")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
}

void MainWindow::onBloodOxygenReceived(int spo2)
{
    m_currentSpo2 = spo2;
    m_spo2ValueLabel->setText(QString::number(spo2));
    
    m_dataManager->saveBloodOxygen(spo2);
    m_vitalsChart->addBloodOxygenPoint(spo2);
    m_alarmManager->checkBloodOxygen(spo2);
    
    m_lastUpdateLabel->setText(QStringLiteral("最后更新: %1")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
}

void MainWindow::onEcgDataReceived(const QVector<double>& data)
{
    m_ecgChart->addDataPoints(data);
    m_dataManager->saveEcgData(data);
    
    m_lastUpdateLabel->setText(QStringLiteral("最后更新: %1")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
}

void MainWindow::onAlarmTriggered(const AlarmInfo& alarm)
{
    showAlarmIndicator(true);
    m_alarmLabel->setText(QStringLiteral("⚠️ %1").arg(alarm.message));
    m_alarmCountLabel->setText(QStringLiteral("活动报警: %1").arg(m_alarmManager->getActiveAlarmCount()));
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
        QString host = settings.value("mqtt/host", "localhost").toString();
        quint16 port = settings.value("mqtt/port", 1883).toInt();
        QString username = settings.value("mqtt/username", "").toString();
        QString password = settings.value("mqtt/password", "").toString();
        
        // 设置主题
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
        // 应用新设置
        loadSettings();
        
        // 更新MQTT主题
        m_mqttClient->setTopics(
            dialog.getTempTopic(),
            dialog.getHrTopic(),
            dialog.getSpo2Topic(),
            dialog.getEcgTopic()
        );
        
        // 更新报警阈值
        m_alarmManager->setThresholds(dialog.getAlarmThresholds());
        m_alarmManager->setSoundEnabled(dialog.isAlarmSoundEnabled());
        
        // 更新云同步
        m_cloudSyncer->setEnabled(dialog.isCloudEnabled());
        m_cloudSyncer->setServerUrl(dialog.getCloudServerUrl());
        m_cloudSyncer->setApiKey(dialog.getCloudApiKey());
        m_cloudSyncer->setDeviceId(dialog.getDeviceId());
        
        if (dialog.isCloudEnabled()) {
            m_cloudSyncer->startAutoSync(dialog.getAutoSyncInterval());
        } else {
            m_cloudSyncer->stopAutoSync();
        }
        
        // 更新图表设置
        m_ecgChart->setDisplayDuration(dialog.getEcgDisplayDuration());
        m_vitalsChart->setTimeRange(dialog.getVitalsTimeRange());
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
        m_simulateButton->setText(QStringLiteral("⏹️ 停止模拟"));
        m_simulationTimer->start(40);  // 25 Hz
    } else {
        m_simulateButton->setText(QStringLiteral("🔄 模拟数据"));
        m_simulationTimer->stop();
    }
}

void MainWindow::onUpdateTimer()
{
    // 更新记录数
    m_recordCountLabel->setText(QStringLiteral("记录数: %1").arg(m_dataManager->getRecordCount()));
}

void MainWindow::onSimulationTimer()
{
    // 模拟ECG数据 - 生成类似真实心电图的波形
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
        
        // 添加一点噪声
        value += (QRandomGenerator::global()->bounded(100) - 50) / 500.0;
        
        ecgData.append(value);
        m_simPhase += 1.0 / 250.0;  // 250 Hz 采样率
    }
    
    m_ecgChart->addDataPoints(ecgData);
    
    // 每秒更新一次生命体征
    ecgCounter++;
    if (ecgCounter >= 25) {
        ecgCounter = 0;
        
        // 模拟体温 (36.0 - 37.5)
        double temp = 36.5 + QRandomGenerator::global()->bounded(100) / 100.0;
        onTemperatureReceived(temp);
        
        // 模拟心率 (60 - 100)
        int hr = 70 + QRandomGenerator::global()->bounded(30);
        onHeartRateReceived(hr);
        
        // 模拟血氧 (95 - 100)
        int spo2 = 96 + QRandomGenerator::global()->bounded(4);
        onBloodOxygenReceived(spo2);
    }
}
