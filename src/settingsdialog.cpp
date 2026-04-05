#include "settingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QSettings>
#include <QMessageBox>

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("设置"));
    setMinimumSize(600, 500);
    setupUI();
    loadSettings();
}

SettingsDialog::~SettingsDialog()
{
    saveSettings();
}

void SettingsDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // 设置深色样式
    setStyleSheet(R"(
        QDialog {
            background-color: #0a1628;
            color: white;
        }
        QTabWidget::pane {
            border: 1px solid #2a4a6a;
            border-radius: 5px;
            background-color: #0a1628;
        }
        QTabBar::tab {
            background-color: #1a3a5c;
            color: white;
            padding: 10px 25px;
            margin-right: 2px;
            border-top-left-radius: 5px;
            border-top-right-radius: 5px;
        }
        QTabBar::tab:selected {
            background-color: #2a5a8c;
        }
        QGroupBox {
            border: 1px solid #2a4a6a;
            border-radius: 8px;
            margin-top: 15px;
            padding-top: 15px;
            font-weight: bold;
            color: #4ecdc4;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 15px;
            padding: 0 5px;
        }
        QLabel {
            color: #b0c4de;
        }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background-color: #1a3a5c;
            border: 1px solid #2a5a8c;
            border-radius: 5px;
            padding: 8px;
            color: white;
            min-width: 200px;
        }
        QCheckBox {
            color: #b0c4de;
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border-radius: 3px;
            border: 1px solid #2a5a8c;
            background-color: #1a3a5c;
        }
        QCheckBox::indicator:checked {
            background-color: #4ecdc4;
        }
        QPushButton {
            background-color: #1a3a5c;
            border: 1px solid #2a5a8c;
            border-radius: 5px;
            padding: 8px 20px;
            color: white;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #2a5a8c;
        }
        QPushButton:pressed {
            background-color: #3a6a9c;
        }
        QDialogButtonBox QPushButton {
            min-width: 100px;
        }
    )");
    
    m_tabWidget = new QTabWidget();
    
    // ===== MQTT设置页 =====
    QWidget* mqttPage = new QWidget();
    QVBoxLayout* mqttLayout = new QVBoxLayout(mqttPage);
    
    QGroupBox* mqttConnGroup = new QGroupBox(QStringLiteral("连接设置"));
    QFormLayout* mqttConnLayout = new QFormLayout(mqttConnGroup);
    
    m_mqttHostEdit = new QLineEdit("47.115.148.200");
    m_mqttPortSpin = new QSpinBox();
    m_mqttPortSpin->setRange(1, 65535);
    m_mqttPortSpin->setValue(1883);
    m_mqttUserEdit = new QLineEdit();
    m_mqttUserEdit->setPlaceholderText(QStringLiteral("可选"));
    m_mqttPassEdit = new QLineEdit();
    m_mqttPassEdit->setEchoMode(QLineEdit::Password);
    m_mqttPassEdit->setPlaceholderText(QStringLiteral("可选"));
    
    mqttConnLayout->addRow(QStringLiteral("服务器地址:"), m_mqttHostEdit);
    mqttConnLayout->addRow(QStringLiteral("端口:"), m_mqttPortSpin);
    mqttConnLayout->addRow(QStringLiteral("用户名:"), m_mqttUserEdit);
    mqttConnLayout->addRow(QStringLiteral("密码:"), m_mqttPassEdit);
    
    mqttLayout->addWidget(mqttConnGroup);
    
    QGroupBox* mqttTopicGroup = new QGroupBox(QStringLiteral("主题设置"));
    QFormLayout* mqttTopicLayout = new QFormLayout(mqttTopicGroup);
    
    m_tempTopicEdit = new QLineEdit("health/temperature");
    m_hrTopicEdit = new QLineEdit("health/heartrate");
    m_spo2TopicEdit = new QLineEdit("health/spo2");
    m_ecgTopicEdit = new QLineEdit("health/ecg");
    
    mqttTopicLayout->addRow(QStringLiteral("体温主题:"), m_tempTopicEdit);
    mqttTopicLayout->addRow(QStringLiteral("心率主题:"), m_hrTopicEdit);
    mqttTopicLayout->addRow(QStringLiteral("血氧主题:"), m_spo2TopicEdit);
    mqttTopicLayout->addRow(QStringLiteral("心电主题:"), m_ecgTopicEdit);
    
    mqttLayout->addWidget(mqttTopicGroup);
    
    m_testMqttButton = new QPushButton(QStringLiteral("🔗 测试连接"));
    QHBoxLayout* testBtnLayout = new QHBoxLayout();
    testBtnLayout->addStretch();
    testBtnLayout->addWidget(m_testMqttButton);
    mqttLayout->addLayout(testBtnLayout);
    mqttLayout->addStretch();
    
    m_tabWidget->addTab(mqttPage, QStringLiteral("📡 MQTT连接"));
    
    // ===== 报警设置页 =====
    QWidget* alarmPage = new QWidget();
    QVBoxLayout* alarmLayout = new QVBoxLayout(alarmPage);
    
    QGroupBox* thresholdGroup = new QGroupBox(QStringLiteral("报警阈值"));
    QGridLayout* thresholdLayout = new QGridLayout(thresholdGroup);
    
    thresholdLayout->addWidget(new QLabel(QStringLiteral("体温上限 (°C):")), 0, 0);
    m_tempHighSpin = new QDoubleSpinBox();
    m_tempHighSpin->setRange(35.0, 45.0);
    m_tempHighSpin->setValue(37.5);
    m_tempHighSpin->setSingleStep(0.1);
    thresholdLayout->addWidget(m_tempHighSpin, 0, 1);
    
    thresholdLayout->addWidget(new QLabel(QStringLiteral("体温下限 (°C):")), 0, 2);
    m_tempLowSpin = new QDoubleSpinBox();
    m_tempLowSpin->setRange(30.0, 40.0);
    m_tempLowSpin->setValue(35.0);
    m_tempLowSpin->setSingleStep(0.1);
    thresholdLayout->addWidget(m_tempLowSpin, 0, 3);
    
    thresholdLayout->addWidget(new QLabel(QStringLiteral("心率上限 (bpm):")), 1, 0);
    m_hrHighSpin = new QSpinBox();
    m_hrHighSpin->setRange(60, 250);
    m_hrHighSpin->setValue(100);
    thresholdLayout->addWidget(m_hrHighSpin, 1, 1);
    
    thresholdLayout->addWidget(new QLabel(QStringLiteral("心率下限 (bpm):")), 1, 2);
    m_hrLowSpin = new QSpinBox();
    m_hrLowSpin->setRange(20, 100);
    m_hrLowSpin->setValue(50);
    thresholdLayout->addWidget(m_hrLowSpin, 1, 3);
    
    thresholdLayout->addWidget(new QLabel(QStringLiteral("血氧下限 (%):")), 2, 0);
    m_spo2LowSpin = new QSpinBox();
    m_spo2LowSpin->setRange(70, 99);
    m_spo2LowSpin->setValue(90);
    thresholdLayout->addWidget(m_spo2LowSpin, 2, 1);
    
    alarmLayout->addWidget(thresholdGroup);
    
    QGroupBox* alarmOptGroup = new QGroupBox(QStringLiteral("报警选项"));
    QVBoxLayout* alarmOptLayout = new QVBoxLayout(alarmOptGroup);
    
    m_alarmSoundCheck = new QCheckBox(QStringLiteral("启用报警声音"));
    m_alarmSoundCheck->setChecked(true);
    alarmOptLayout->addWidget(m_alarmSoundCheck);
    
    alarmLayout->addWidget(alarmOptGroup);
    alarmLayout->addStretch();
    
    m_tabWidget->addTab(alarmPage, QStringLiteral("🔔 报警设置"));
    
    // ===== 云同步设置页 =====
    QWidget* cloudPage = new QWidget();
    QVBoxLayout* cloudLayout = new QVBoxLayout(cloudPage);
    
    QGroupBox* cloudGroup = new QGroupBox(QStringLiteral("云服务器设置"));
    QFormLayout* cloudFormLayout = new QFormLayout(cloudGroup);
    
    m_cloudEnabledCheck = new QCheckBox(QStringLiteral("启用云同步"));
    cloudFormLayout->addRow(m_cloudEnabledCheck);
    
    m_cloudUrlEdit = new QLineEdit("https://api.healthmonitor.example.com/v1/data");
    m_cloudApiKeyEdit = new QLineEdit();
    m_cloudApiKeyEdit->setEchoMode(QLineEdit::Password);
    m_deviceIdEdit = new QLineEdit("device_001");
    
    m_syncIntervalSpin = new QSpinBox();
    m_syncIntervalSpin->setRange(10, 3600);
    m_syncIntervalSpin->setValue(60);
    m_syncIntervalSpin->setSuffix(QStringLiteral(" 秒"));
    
    cloudFormLayout->addRow(QStringLiteral("服务器URL:"), m_cloudUrlEdit);
    cloudFormLayout->addRow(QStringLiteral("API密钥:"), m_cloudApiKeyEdit);
    cloudFormLayout->addRow(QStringLiteral("设备ID:"), m_deviceIdEdit);
    cloudFormLayout->addRow(QStringLiteral("同步间隔:"), m_syncIntervalSpin);
    
    cloudLayout->addWidget(cloudGroup);
    
    m_testCloudButton = new QPushButton(QStringLiteral("☁️ 测试云连接"));
    QHBoxLayout* cloudTestLayout = new QHBoxLayout();
    cloudTestLayout->addStretch();
    cloudTestLayout->addWidget(m_testCloudButton);
    cloudLayout->addLayout(cloudTestLayout);
    cloudLayout->addStretch();
    
    m_tabWidget->addTab(cloudPage, QStringLiteral("☁️ 云同步"));
    
    // ===== 显示设置页 =====
    QWidget* displayPage = new QWidget();
    QVBoxLayout* displayLayout = new QVBoxLayout(displayPage);
    
    QGroupBox* displayGroup = new QGroupBox(QStringLiteral("显示设置"));
    QFormLayout* displayFormLayout = new QFormLayout(displayGroup);
    
    m_ecgDurationSpin = new QSpinBox();
    m_ecgDurationSpin->setRange(1, 30);
    m_ecgDurationSpin->setValue(5);
    m_ecgDurationSpin->setSuffix(QStringLiteral(" 秒"));
    
    m_vitalsRangeCombo = new QComboBox();
    m_vitalsRangeCombo->addItem(QStringLiteral("10分钟"), 10);
    m_vitalsRangeCombo->addItem(QStringLiteral("30分钟"), 30);
    m_vitalsRangeCombo->addItem(QStringLiteral("1小时"), 60);
    m_vitalsRangeCombo->addItem(QStringLiteral("6小时"), 360);
    m_vitalsRangeCombo->setCurrentIndex(2);
    
    displayFormLayout->addRow(QStringLiteral("心电图显示时长:"), m_ecgDurationSpin);
    displayFormLayout->addRow(QStringLiteral("趋势图时间范围:"), m_vitalsRangeCombo);

    displayLayout->addWidget(displayGroup);

    // 显示信息选择
    QGroupBox* infoSelectGroup = new QGroupBox(QStringLiteral("显示信息选择"));
    QVBoxLayout* infoSelectLayout = new QVBoxLayout(infoSelectGroup);
    infoSelectLayout->setSpacing(10);

    m_showTempCheck = new QCheckBox(QStringLiteral("显示体温"));
    m_showTempCheck->setChecked(true);
    infoSelectLayout->addWidget(m_showTempCheck);

    m_showHrCheck = new QCheckBox(QStringLiteral("显示心率"));
    m_showHrCheck->setChecked(true);
    infoSelectLayout->addWidget(m_showHrCheck);

    m_showSpo2Check = new QCheckBox(QStringLiteral("显示血氧"));
    m_showSpo2Check->setChecked(true);
    infoSelectLayout->addWidget(m_showSpo2Check);

    QLabel* infoHint = new QLabel(QStringLiteral("取消勾选后，主界面将隐藏对应指标卡片"));
    infoHint->setStyleSheet("color: #7a8899; font-size: 11px; padding-top: 4px;");
    infoSelectLayout->addWidget(infoHint);

    displayLayout->addWidget(infoSelectGroup);
    
    // ECG滤波设置组
    QGroupBox* filterGroup = new QGroupBox(QStringLiteral("ECG低通滤波"));
    QFormLayout* filterFormLayout = new QFormLayout(filterGroup);
    
    m_ecgFilterEnabledCheck = new QCheckBox(QStringLiteral("启用低通滤波"));
    m_ecgFilterEnabledCheck->setChecked(true);
    filterFormLayout->addRow(m_ecgFilterEnabledCheck);
    
    m_ecgFilterCoefficientSpin = new QDoubleSpinBox();
    m_ecgFilterCoefficientSpin->setRange(0.01, 1.0);
    m_ecgFilterCoefficientSpin->setValue(0.25);
    m_ecgFilterCoefficientSpin->setSingleStep(0.05);
    m_ecgFilterCoefficientSpin->setDecimals(2);
    m_ecgFilterCoefficientSpin->setToolTip(QStringLiteral("滤波系数 (0.01-1.0)\n值越小滤波效果越强，但响应越慢\n值越大响应越快，但滤波效果越弱\n建议值: 0.1-0.5"));
    
    filterFormLayout->addRow(QStringLiteral("滤波系数 (α):"), m_ecgFilterCoefficientSpin);
    
    // 添加说明标签
    QLabel* filterHintLabel = new QLabel(QStringLiteral(
        "滤波公式: filtered = last + (raw - last) × α\n"
        "• α=0.1: 强滤波，平滑但延迟较大\n"
        "• α=0.25: 推荐值，平衡滤波与响应\n"
        "• α=0.5: 弱滤波，响应快但噪声多"
    ));
    filterHintLabel->setStyleSheet("color: #7a8899; font-size: 11px; padding: 8px;");
    filterFormLayout->addRow(filterHintLabel);
    
    displayLayout->addWidget(filterGroup);
    displayLayout->addStretch();
    
    m_tabWidget->addTab(displayPage, QStringLiteral("🎨 显示设置"));
    
    mainLayout->addWidget(m_tabWidget);
    
    // 按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* resetButton = new QPushButton(QStringLiteral("恢复默认"));
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    
    buttonLayout->addWidget(resetButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(buttonBox);
    
    mainLayout->addLayout(buttonLayout);
    
    // 连接信号
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(resetButton, &QPushButton::clicked, this, &SettingsDialog::onResetDefaults);
    connect(m_testMqttButton, &QPushButton::clicked, this, &SettingsDialog::onTestMqttClicked);
    connect(m_testCloudButton, &QPushButton::clicked, this, &SettingsDialog::onTestCloudClicked);
}

void SettingsDialog::loadSettings()
{
    QSettings settings("HealthMonitor", "QtECG");
    
    // MQTT设置
    m_mqttHostEdit->setText(settings.value("mqtt/host", "47.115.148.200").toString());
    m_mqttPortSpin->setValue(settings.value("mqtt/port", 1883).toInt());
    m_mqttUserEdit->setText(settings.value("mqtt/username", "").toString());
    m_mqttPassEdit->setText(settings.value("mqtt/password", "").toString());
    
    m_tempTopicEdit->setText(settings.value("mqtt/tempTopic", "health/temperature").toString());
    m_hrTopicEdit->setText(settings.value("mqtt/hrTopic", "health/heartrate").toString());
    m_spo2TopicEdit->setText(settings.value("mqtt/spo2Topic", "health/spo2").toString());
    m_ecgTopicEdit->setText(settings.value("mqtt/ecgTopic", "health/ecg").toString());
    
    // 报警阈值
    m_tempHighSpin->setValue(settings.value("alarm/tempHigh", 37.5).toDouble());
    m_tempLowSpin->setValue(settings.value("alarm/tempLow", 35.0).toDouble());
    m_hrHighSpin->setValue(settings.value("alarm/hrHigh", 100).toInt());
    m_hrLowSpin->setValue(settings.value("alarm/hrLow", 50).toInt());
    m_spo2LowSpin->setValue(settings.value("alarm/spo2Low", 90).toInt());
    m_alarmSoundCheck->setChecked(settings.value("alarm/soundEnabled", true).toBool());
    
    // 云同步
    m_cloudEnabledCheck->setChecked(settings.value("cloud/enabled", false).toBool());
    m_cloudUrlEdit->setText(settings.value("cloud/url", "").toString());
    m_cloudApiKeyEdit->setText(settings.value("cloud/apiKey", "").toString());
    m_deviceIdEdit->setText(settings.value("cloud/deviceId", "device_001").toString());
    m_syncIntervalSpin->setValue(settings.value("cloud/syncInterval", 60).toInt());
    
    // 显示设置
    m_ecgDurationSpin->setValue(settings.value("display/ecgDuration", 5).toInt());
    int vitalsIndex = m_vitalsRangeCombo->findData(settings.value("display/vitalsRange", 60).toInt());
    if (vitalsIndex >= 0) m_vitalsRangeCombo->setCurrentIndex(vitalsIndex);
    
    // ECG滤波设置
    m_ecgFilterEnabledCheck->setChecked(settings.value("ecg/filterEnabled", true).toBool());
    m_ecgFilterCoefficientSpin->setValue(settings.value("ecg/filterCoefficient", 0.25).toDouble());

    // 显示信息选择
    m_showTempCheck->setChecked(settings.value("display/showTemp", true).toBool());
    m_showHrCheck->setChecked(settings.value("display/showHr",   true).toBool());
    m_showSpo2Check->setChecked(settings.value("display/showSpo2", true).toBool());
}

void SettingsDialog::saveSettings()
{
    QSettings settings("HealthMonitor", "QtECG");
    
    // MQTT设置
    settings.setValue("mqtt/host", m_mqttHostEdit->text());
    settings.setValue("mqtt/port", m_mqttPortSpin->value());
    settings.setValue("mqtt/username", m_mqttUserEdit->text());
    settings.setValue("mqtt/password", m_mqttPassEdit->text());
    
    settings.setValue("mqtt/tempTopic", m_tempTopicEdit->text());
    settings.setValue("mqtt/hrTopic", m_hrTopicEdit->text());
    settings.setValue("mqtt/spo2Topic", m_spo2TopicEdit->text());
    settings.setValue("mqtt/ecgTopic", m_ecgTopicEdit->text());
    
    // 报警阈值
    settings.setValue("alarm/tempHigh", m_tempHighSpin->value());
    settings.setValue("alarm/tempLow", m_tempLowSpin->value());
    settings.setValue("alarm/hrHigh", m_hrHighSpin->value());
    settings.setValue("alarm/hrLow", m_hrLowSpin->value());
    settings.setValue("alarm/spo2Low", m_spo2LowSpin->value());
    settings.setValue("alarm/soundEnabled", m_alarmSoundCheck->isChecked());
    
    // 云同步
    settings.setValue("cloud/enabled", m_cloudEnabledCheck->isChecked());
    settings.setValue("cloud/url", m_cloudUrlEdit->text());
    settings.setValue("cloud/apiKey", m_cloudApiKeyEdit->text());
    settings.setValue("cloud/deviceId", m_deviceIdEdit->text());
    settings.setValue("cloud/syncInterval", m_syncIntervalSpin->value());
    
    // 显示设置
    settings.setValue("display/ecgDuration", m_ecgDurationSpin->value());
    settings.setValue("display/vitalsRange", m_vitalsRangeCombo->currentData().toInt());
    
    // ECG滤波设置
    settings.setValue("ecg/filterEnabled", m_ecgFilterEnabledCheck->isChecked());
    settings.setValue("ecg/filterCoefficient", m_ecgFilterCoefficientSpin->value());

    // 显示信息选择
    settings.setValue("display/showTemp", m_showTempCheck->isChecked());
    settings.setValue("display/showHr",   m_showHrCheck->isChecked());
    settings.setValue("display/showSpo2", m_showSpo2Check->isChecked());
}

QString SettingsDialog::getMqttHost() const { return m_mqttHostEdit->text(); }
quint16 SettingsDialog::getMqttPort() const { return m_mqttPortSpin->value(); }
QString SettingsDialog::getMqttUsername() const { return m_mqttUserEdit->text(); }
QString SettingsDialog::getMqttPassword() const { return m_mqttPassEdit->text(); }
QString SettingsDialog::getTempTopic() const { return m_tempTopicEdit->text(); }
QString SettingsDialog::getHrTopic() const { return m_hrTopicEdit->text(); }
QString SettingsDialog::getSpo2Topic() const { return m_spo2TopicEdit->text(); }
QString SettingsDialog::getEcgTopic() const { return m_ecgTopicEdit->text(); }

void SettingsDialog::setMqttSettings(const QString& host, quint16 port,
                                      const QString& username, const QString& password)
{
    m_mqttHostEdit->setText(host);
    m_mqttPortSpin->setValue(port);
    m_mqttUserEdit->setText(username);
    m_mqttPassEdit->setText(password);
}

void SettingsDialog::setMqttTopics(const QString& temp, const QString& hr,
                                    const QString& spo2, const QString& ecg)
{
    m_tempTopicEdit->setText(temp);
    m_hrTopicEdit->setText(hr);
    m_spo2TopicEdit->setText(spo2);
    m_ecgTopicEdit->setText(ecg);
}

AlarmThresholds SettingsDialog::getAlarmThresholds() const
{
    AlarmThresholds t;
    t.tempHigh = m_tempHighSpin->value();
    t.tempLow = m_tempLowSpin->value();
    t.hrHigh = m_hrHighSpin->value();
    t.hrLow = m_hrLowSpin->value();
    t.spo2Low = m_spo2LowSpin->value();
    return t;
}

void SettingsDialog::setAlarmThresholds(const AlarmThresholds& thresholds)
{
    m_tempHighSpin->setValue(thresholds.tempHigh);
    m_tempLowSpin->setValue(thresholds.tempLow);
    m_hrHighSpin->setValue(thresholds.hrHigh);
    m_hrLowSpin->setValue(thresholds.hrLow);
    m_spo2LowSpin->setValue(thresholds.spo2Low);
}

QString SettingsDialog::getCloudServerUrl() const { return m_cloudUrlEdit->text(); }
QString SettingsDialog::getCloudApiKey() const { return m_cloudApiKeyEdit->text(); }
QString SettingsDialog::getDeviceId() const { return m_deviceIdEdit->text(); }
bool SettingsDialog::isCloudEnabled() const { return m_cloudEnabledCheck->isChecked(); }
int SettingsDialog::getAutoSyncInterval() const { return m_syncIntervalSpin->value(); }

void SettingsDialog::setCloudSettings(const QString& url, const QString& apiKey,
                                       const QString& deviceId, bool enabled, int interval)
{
    m_cloudUrlEdit->setText(url);
    m_cloudApiKeyEdit->setText(apiKey);
    m_deviceIdEdit->setText(deviceId);
    m_cloudEnabledCheck->setChecked(enabled);
    m_syncIntervalSpin->setValue(interval);
}

int SettingsDialog::getEcgDisplayDuration() const { return m_ecgDurationSpin->value(); }
int SettingsDialog::getVitalsTimeRange() const { return m_vitalsRangeCombo->currentData().toInt(); }
bool SettingsDialog::isAlarmSoundEnabled() const { return m_alarmSoundCheck->isChecked(); }

void SettingsDialog::setDisplaySettings(int ecgDuration, int vitalsRange, bool alarmSound)
{
    m_ecgDurationSpin->setValue(ecgDuration);
    int index = m_vitalsRangeCombo->findData(vitalsRange);
    if (index >= 0) m_vitalsRangeCombo->setCurrentIndex(index);
    m_alarmSoundCheck->setChecked(alarmSound);
}

bool SettingsDialog::isEcgFilterEnabled() const
{
    return m_ecgFilterEnabledCheck->isChecked();
}

double SettingsDialog::getEcgFilterCoefficient() const
{
    return m_ecgFilterCoefficientSpin->value();
}

void SettingsDialog::setEcgFilterSettings(bool enabled, double coefficient)
{
    m_ecgFilterEnabledCheck->setChecked(enabled);
    m_ecgFilterCoefficientSpin->setValue(coefficient);
}

bool SettingsDialog::isShowTemperature() const { return m_showTempCheck->isChecked(); }
bool SettingsDialog::isShowHeartRate() const    { return m_showHrCheck->isChecked(); }
bool SettingsDialog::isShowBloodOxygen() const  { return m_showSpo2Check->isChecked(); }

void SettingsDialog::onTestMqttClicked()
{
    emit mqttTestRequested(m_mqttHostEdit->text(), m_mqttPortSpin->value(),
                           m_mqttUserEdit->text(), m_mqttPassEdit->text());
}

void SettingsDialog::onTestCloudClicked()
{
    QMessageBox::information(this, QStringLiteral("测试云连接"),
        QStringLiteral("云连接测试功能将在连接后可用。"));
}

void SettingsDialog::onAccepted()
{
    saveSettings();
    emit settingsChanged();
    accept();
}

void SettingsDialog::onResetDefaults()
{
    if (QMessageBox::question(this, QStringLiteral("恢复默认"),
            QStringLiteral("确定要恢复所有设置为默认值吗？"),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        
        m_mqttHostEdit->setText("47.115.148.200");
        m_mqttPortSpin->setValue(1883);
        m_mqttUserEdit->clear();
        m_mqttPassEdit->clear();
        
        m_tempTopicEdit->setText("health/temperature");
        m_hrTopicEdit->setText("health/heartrate");
        m_spo2TopicEdit->setText("health/spo2");
        m_ecgTopicEdit->setText("health/ecg");
        
        m_tempHighSpin->setValue(37.5);
        m_tempLowSpin->setValue(35.0);
        m_hrHighSpin->setValue(100);
        m_hrLowSpin->setValue(50);
        m_spo2LowSpin->setValue(90);
        m_alarmSoundCheck->setChecked(true);
        
        m_cloudEnabledCheck->setChecked(false);
        m_syncIntervalSpin->setValue(60);
        
        m_ecgDurationSpin->setValue(5);
        m_vitalsRangeCombo->setCurrentIndex(2);
        
        m_ecgFilterEnabledCheck->setChecked(true);
        m_ecgFilterCoefficientSpin->setValue(0.25);

        m_showTempCheck->setChecked(true);
        m_showHrCheck->setChecked(true);
        m_showSpo2Check->setChecked(true);
    }
}
