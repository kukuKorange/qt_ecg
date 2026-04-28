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

    QWidget* spacer1 = new QWidget();
    spacer1->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer1);

    // MQTT 状态居中显示
    m_mqttStatusBadge = new QLabel(QStringLiteral("● 未连接"));
    m_mqttStatusBadge->setAlignment(Qt::AlignCenter);
    m_mqttStatusBadge->setStyleSheet(R"(
        color: #e74c3c;
        font-size: 13px;
        font-weight: bold;
        padding: 4px 14px;
        background-color: #16213e;
        border-radius: 8px;
        border: 1px solid #2a4a6a;
    )");
    toolbar->addWidget(m_mqttStatusBadge);

    QWidget* spacer2 = new QWidget();
    spacer2->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer2);

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
    
    // 生命体征卡片区域 — 一行显示
    QWidget* vitalsWidget = new QWidget();
    QHBoxLayout* vitalsLayout = new QHBoxLayout(vitalsWidget);
    vitalsLayout->setSpacing(10);
    vitalsLayout->setContentsMargins(0, 0, 0, 0);

    // 体温卡片
    m_tempCard = createVitalCard(
        QStringLiteral("体温"), "--.-", QStringLiteral("°C"),
        QColor("#e74c3c"), &m_tempValueLabel);
    vitalsLayout->addWidget(m_tempCard, 1);

    // 心率卡片
    m_hrCard = createVitalCard(
        QStringLiteral("心率"), "---", QStringLiteral("bpm"),
        QColor("#3498db"), &m_hrValueLabel);
    vitalsLayout->addWidget(m_hrCard, 1);

    // 血氧卡片
    m_spo2Card = createVitalCard(
        QStringLiteral("血氧"), "---", QStringLiteral("%"),
        QColor("#2ecc71"), &m_spo2ValueLabel);
    vitalsLayout->addWidget(m_spo2Card, 1);

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
    layout->setContentsMargins(10, 12, 10, 12);
    layout->setSpacing(6);
    
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
        font-size: 28px;
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
    
    // ECG R波检测心率
    connect(m_ecgChart, &EcgChartWidget::heartRateFromEcg, this, [this](int bpm) {
        m_currentHr = bpm;
        m_hrValueLabel->setText(QString::number(bpm));
        m_alarmManager->checkHeartRate(bpm);
    });

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
    
    // ECG滤波设置
    m_ecgChart->setFilterEnabled(settings.value("ecg/filterEnabled", true).toBool());
    m_ecgChart->setFilterCoefficient(settings.value("ecg/filterCoefficient", 0.25).toDouble());

    applyDisplaySettings();
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
    if (m_mqttStatusBadge) {
        m_mqttStatusBadge->setText(QStringLiteral("● ") + status);
    }
}

void MainWindow::updateConnectionStatus(bool connected)
{
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
        if (m_mqttStatusBadge) {
            m_mqttStatusBadge->setText(QStringLiteral("● MQTT 已连接"));
            m_mqttStatusBadge->setStyleSheet(R"(
                color: #2ecc71;
                font-size: 13px;
                font-weight: bold;
                padding: 4px 14px;
                background-color: #16213e;
                border-radius: 8px;
                border: 1px solid #2ecc71;
            )");
        }
    } else {
        m_connectionStatusLabel->setText(QStringLiteral("未连接"));
        m_connectButton->setText(QStringLiteral("连接"));
        m_connectButton->setStyleSheet("");
        m_connectButton->setObjectName("connectBtn");
        if (m_mqttStatusBadge) {
            m_mqttStatusBadge->setText(QStringLiteral("● MQTT 未连接"));
            m_mqttStatusBadge->setStyleSheet(R"(
                color: #e74c3c;
                font-size: 13px;
                font-weight: bold;
                padding: 4px 14px;
                background-color: #16213e;
                border-radius: 8px;
                border: 1px solid #2a4a6a;
            )");
        }
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
        
        // 应用ECG滤波设置
        m_ecgChart->setFilterEnabled(dialog.isEcgFilterEnabled());
        m_ecgChart->setFilterCoefficient(dialog.getEcgFilterCoefficient());

        applyDisplaySettings();
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
        m_ecgChart->clear();
        m_simPhase = 0.0;
        m_simulateButton->setText(QStringLiteral("停止"));
        m_simulateButton->setStyleSheet("background-color: #e67e22;");
        m_simulationTimer->start(50);  // 50ms × 10点/次 = 200Hz
    } else {
        m_simulateButton->setText(QStringLiteral("模拟"));
        m_simulateButton->setStyleSheet("");
        m_simulationTimer->stop();
        showEcgAnalysisReport();
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

    // 每50ms产生10个点 = 200Hz, 与图表采样率一致
    for (int i = 0; i < 10; ++i) {
        double t = fmod(m_simPhase, 1.0);  // 一个心跳周期 (~60bpm)
        double value = 0;

        // P波 (0.00 - 0.10s): 心房去极化
        if (t >= 0.02 && t < 0.12) {
            value = 30.0 * qSin(M_PI * (t - 0.02) / 0.10);
        }
        // Q波 (0.15 - 0.17s): QRS起始向下偏转
        else if (t >= 0.15 && t < 0.17) {
            value = -40.0 * qSin(M_PI * (t - 0.15) / 0.02);
        }
        // R波 (0.17 - 0.21s): 主峰, 尖锐向上
        else if (t >= 0.17 && t < 0.21) {
            value = 250.0 * qSin(M_PI * (t - 0.17) / 0.04);
        }
        // S波 (0.21 - 0.24s): QRS末尾向下
        else if (t >= 0.21 && t < 0.24) {
            value = -60.0 * qSin(M_PI * (t - 0.21) / 0.03);
        }
        // T波 (0.30 - 0.50s): 心室复极化, 较宽
        else if (t >= 0.30 && t < 0.50) {
            value = 50.0 * qSin(M_PI * (t - 0.30) / 0.20);
        }

        // 加入少量噪声
        value += (QRandomGenerator::global()->bounded(100) - 50) / 10.0;

        ecgData.append(value);
        m_simPhase += 1.0 / 200.0;  // 200Hz采样率
    }

    m_ecgChart->addDataPoints(ecgData);

    ecgCounter++;
    if (ecgCounter >= 20) {  // 每秒更新一次体征数据
        ecgCounter = 0;

        double temp = 36.5 + QRandomGenerator::global()->bounded(100) / 100.0;
        onTemperatureReceived(temp);

        int spo2 = 96 + QRandomGenerator::global()->bounded(4);
        onBloodOxygenReceived(spo2);
    }
}

void MainWindow::applyDisplaySettings()
{
    QSettings settings("HealthMonitor", "QtECG");
    bool showTemp = settings.value("display/showTemp", true).toBool();
    bool showHr   = settings.value("display/showHr",   true).toBool();
    bool showSpo2 = settings.value("display/showSpo2", true).toBool();

    if (m_tempCard) m_tempCard->setVisible(showTemp);
    if (m_hrCard)   m_hrCard->setVisible(showHr);
    if (m_spo2Card) m_spo2Card->setVisible(showSpo2);
}

void MainWindow::updateVitalDisplay(const QString& type, double value, const QString& unit)
{
    Q_UNUSED(type)
    Q_UNUSED(value)
    Q_UNUSED(unit)
}

void MainWindow::showEcgAnalysisReport()
{
    RPeakDetector* detector = m_ecgChart->rPeakDetector();
    if (!detector || detector->detectedPeaks().size() < 2) {
        QMessageBox::information(this, QStringLiteral("ECG分析"),
                                 QStringLiteral("数据不足，无法生成分析报告。\n请至少采集5秒以上的数据。"));
        return;
    }

    auto report = detector->generateReport();

    QString html;
    html += QStringLiteral("<h2 style='color:#00d9ff;'>ECG R波分析报告</h2>");
    html += QStringLiteral("<hr>");

    // 基本信息
    html += QStringLiteral("<h3>基本信息</h3>");
    html += QStringLiteral("<table cellpadding='4'>");
    html += QStringLiteral("<tr><td><b>检测到R波数:</b></td><td>%1 个</td></tr>").arg(report.totalPeaks);
    html += QStringLiteral("<tr><td><b>分析时长:</b></td><td>%1 秒</td></tr>").arg(report.durationSeconds, 0, 'f', 1);
    html += QStringLiteral("</table>");

    // 心率统计
    html += QStringLiteral("<h3>心率统计</h3>");
    html += QStringLiteral("<table cellpadding='4'>");
    html += QStringLiteral("<tr><td><b>平均心率:</b></td><td>%1 bpm</td></tr>").arg(report.avgHR, 0, 'f', 1);
    html += QStringLiteral("<tr><td><b>最低心率:</b></td><td>%1 bpm</td></tr>").arg(report.minHR, 0, 'f', 1);
    html += QStringLiteral("<tr><td><b>最高心率:</b></td><td>%1 bpm</td></tr>").arg(report.maxHR, 0, 'f', 1);
    html += QStringLiteral("<tr><td><b>心率标准差:</b></td><td>%1 bpm</td></tr>").arg(report.stdHR, 0, 'f', 2);
    html += QStringLiteral("</table>");

    // R-R间期
    html += QStringLiteral("<h3>R-R间期分析</h3>");
    html += QStringLiteral("<table cellpadding='4'>");
    html += QStringLiteral("<tr><td><b>平均R-R间期:</b></td><td>%1 ms</td></tr>").arg(report.avgRR, 0, 'f', 1);
    html += QStringLiteral("<tr><td><b>最短R-R间期:</b></td><td>%1 ms</td></tr>").arg(report.minRR, 0, 'f', 1);
    html += QStringLiteral("<tr><td><b>最长R-R间期:</b></td><td>%1 ms</td></tr>").arg(report.maxRR, 0, 'f', 1);
    html += QStringLiteral("<tr><td><b>R-R间期标准差:</b></td><td>%1 ms</td></tr>").arg(report.stdRR, 0, 'f', 2);
    html += QStringLiteral("</table>");

    // HRV指标
    html += QStringLiteral("<h3>心率变异性 (HRV)</h3>");
    html += QStringLiteral("<table cellpadding='4'>");
    html += QStringLiteral("<tr><td><b>SDNN:</b></td><td>%1 ms</td><td style='color:#8892b0;'>R-R间期标准差</td></tr>").arg(report.sdnn, 0, 'f', 2);
    html += QStringLiteral("<tr><td><b>RMSSD:</b></td><td>%1 ms</td><td style='color:#8892b0;'>相邻R-R差值均方根</td></tr>").arg(report.rmssd, 0, 'f', 2);
    html += QStringLiteral("<tr><td><b>pNN50:</b></td><td>%1 %</td><td style='color:#8892b0;'>差值>50ms的百分比</td></tr>").arg(report.pnn50, 0, 'f', 1);
    html += QStringLiteral("</table>");

    // R波幅值
    html += QStringLiteral("<h3>R波幅值</h3>");
    html += QStringLiteral("<table cellpadding='4'>");
    html += QStringLiteral("<tr><td><b>平均幅值:</b></td><td>%1 mV</td></tr>").arg(report.avgAmplitude, 0, 'f', 1);
    html += QStringLiteral("<tr><td><b>最小幅值:</b></td><td>%1 mV</td></tr>").arg(report.minAmplitude, 0, 'f', 1);
    html += QStringLiteral("<tr><td><b>最大幅值:</b></td><td>%1 mV</td></tr>").arg(report.maxAmplitude, 0, 'f', 1);
    html += QStringLiteral("</table>");

    // 医学评估
    html += QStringLiteral("<h3 style='color:#f39c12;'>医学评估</h3>");
    for (const QString& finding : report.findings) {
        QString color = "#2ecc71";  // 绿色=正常
        if (finding.contains(QStringLiteral("过缓")) || finding.contains(QStringLiteral("过速")) ||
            finding.contains(QStringLiteral("早搏")) || finding.contains(QStringLiteral("不齐")) ||
            finding.contains(QStringLiteral("偏低")) || finding.contains(QStringLiteral("变异较大"))) {
            color = "#e74c3c";  // 红色=异常
        }
        html += QStringLiteral("<p style='color:%1;'>%2</p>").arg(color, finding);
    }

    // 建议
    html += QStringLiteral("<h3 style='color:#3498db;'>建议</h3>");
    for (const QString& suggestion : report.suggestions) {
        html += QStringLiteral("<p>%1</p>").arg(suggestion);
    }

    html += QStringLiteral("<hr><p style='color:#8892b0; font-size:11px;'>"
                           "注: 本分析仅供参考，不构成医学诊断。如有异常请咨询专业医生。</p>");

    // 使用QMessageBox显示报告
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(QStringLiteral("ECG R波分析报告"));
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText(html);
    msgBox.setStyleSheet(R"(
        QMessageBox {
            background-color: #1a1a2e;
        }
        QMessageBox QLabel {
            color: #eaeaea;
            min-width: 480px;
            min-height: 400px;
        }
        QPushButton {
            background-color: #1f4068;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 8px 24px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #2a5a8c;
        }
    )");
    msgBox.exec();
}
