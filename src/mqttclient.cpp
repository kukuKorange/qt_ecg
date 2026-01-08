#include "mqttclient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

MqttClient::MqttClient(QObject* parent)
    : QObject(parent)
    , m_client(new QMqttClient(this))
    , m_reconnectTimer(new QTimer(this))
    , m_port(1883)
    , m_tempTopic("health/temperature")
    , m_hrTopic("health/heartrate")
    , m_spo2Topic("health/spo2")
    , m_ecgTopic("health/ecg")
{
    connect(m_client, &QMqttClient::connected, this, &MqttClient::onConnected);
    connect(m_client, &QMqttClient::disconnected, this, &MqttClient::onDisconnected);
    connect(m_client, &QMqttClient::messageReceived, this, &MqttClient::onMessageReceived);
    connect(m_client, &QMqttClient::stateChanged, this, &MqttClient::onStateChanged);
    connect(m_client, &QMqttClient::errorChanged, this, &MqttClient::onErrorChanged);
    
    connect(m_reconnectTimer, &QTimer::timeout, this, &MqttClient::onReconnectTimer);
    m_reconnectTimer->setInterval(m_reconnectInterval);
}

MqttClient::~MqttClient()
{
    disconnectFromHost();
}

void MqttClient::connectToHost(const QString& host, quint16 port,
                                const QString& username, const QString& password)
{
    m_host = host;
    m_port = port;
    m_username = username;
    m_password = password;
    
    m_client->setHostname(host);
    m_client->setPort(port);
    
    if (!username.isEmpty()) {
        m_client->setUsername(username);
        m_client->setPassword(password);
    }
    
    m_client->setClientId(QString("QtECGMonitor_%1").arg(QDateTime::currentMSecsSinceEpoch()));
    m_client->setKeepAlive(60);
    
    emit statusChanged(QStringLiteral("正在连接到 %1:%2...").arg(host).arg(port));
    m_client->connectToHost();
}

void MqttClient::disconnectFromHost()
{
    m_autoReconnect = false;
    m_reconnectTimer->stop();
    
    if (m_client->state() != QMqttClient::Disconnected) {
        m_client->disconnectFromHost();
    }
}

bool MqttClient::isConnected() const
{
    return m_client->state() == QMqttClient::Connected;
}

void MqttClient::setTopics(const QString& tempTopic, const QString& hrTopic,
                           const QString& spo2Topic, const QString& ecgTopic)
{
    m_tempTopic = tempTopic;
    m_hrTopic = hrTopic;
    m_spo2Topic = spo2Topic;
    m_ecgTopic = ecgTopic;
}

QString MqttClient::getStatusText() const
{
    switch (m_client->state()) {
        case QMqttClient::Disconnected:
            return QStringLiteral("已断开");
        case QMqttClient::Connecting:
            return QStringLiteral("连接中...");
        case QMqttClient::Connected:
            return QStringLiteral("已连接");
        default:
            return QStringLiteral("未知状态");
    }
}

void MqttClient::onConnected()
{
    emit statusChanged(QStringLiteral("已连接到MQTT服务器"));
    m_reconnectTimer->stop();
    m_autoReconnect = true;
    subscribeToTopics();
    emit connected();
}

void MqttClient::onDisconnected()
{
    emit statusChanged(QStringLiteral("已断开连接"));
    emit disconnected();
    
    if (m_autoReconnect && !m_reconnectTimer->isActive()) {
        m_reconnectTimer->start();
    }
}

void MqttClient::onMessageReceived(const QByteArray& message, const QMqttTopicName& topic)
{
    QString topicName = topic.name();
    
    if (topicName == m_tempTopic) {
        parseTemperature(message);
    } else if (topicName == m_hrTopic) {
        parseHeartRate(message);
    } else if (topicName == m_spo2Topic) {
        parseBloodOxygen(message);
    } else if (topicName == m_ecgTopic) {
        parseEcgData(message);
    } else if (topicName.startsWith("health/vitals")) {
        // 综合数据包
        QJsonDocument doc = QJsonDocument::fromJson(message);
        if (doc.isObject()) {
            VitalData data = VitalData::fromJson(doc.object());
            data.timestamp = QDateTime::currentDateTime();
            emit vitalDataReceived(data);
        }
    }
}

void MqttClient::onStateChanged(QMqttClient::ClientState state)
{
    QString stateText;
    switch (state) {
        case QMqttClient::Disconnected:
            stateText = QStringLiteral("已断开");
            break;
        case QMqttClient::Connecting:
            stateText = QStringLiteral("连接中");
            break;
        case QMqttClient::Connected:
            stateText = QStringLiteral("已连接");
            break;
    }
    emit statusChanged(stateText);
}

void MqttClient::onErrorChanged(QMqttClient::ClientError error)
{
    QString errorText;
    switch (error) {
        case QMqttClient::NoError:
            return;
        case QMqttClient::InvalidProtocolVersion:
            errorText = QStringLiteral("协议版本无效");
            break;
        case QMqttClient::IdRejected:
            errorText = QStringLiteral("客户端ID被拒绝");
            break;
        case QMqttClient::ServerUnavailable:
            errorText = QStringLiteral("服务器不可用");
            break;
        case QMqttClient::BadUsernameOrPassword:
            errorText = QStringLiteral("用户名或密码错误");
            break;
        case QMqttClient::NotAuthorized:
            errorText = QStringLiteral("未授权");
            break;
        case QMqttClient::TransportInvalid:
            errorText = QStringLiteral("传输层无效");
            break;
        case QMqttClient::ProtocolViolation:
            errorText = QStringLiteral("协议违规");
            break;
        case QMqttClient::UnknownError:
        default:
            errorText = QStringLiteral("未知错误");
            break;
    }
    emit connectionError(errorText);
    emit statusChanged(QStringLiteral("错误: %1").arg(errorText));
}

void MqttClient::onReconnectTimer()
{
    if (m_client->state() == QMqttClient::Disconnected) {
        emit statusChanged(QStringLiteral("正在重连..."));
        m_client->connectToHost();
    }
}

void MqttClient::subscribeToTopics()
{
    m_client->subscribe(QMqttTopicFilter(m_tempTopic), 1);
    m_client->subscribe(QMqttTopicFilter(m_hrTopic), 1);
    m_client->subscribe(QMqttTopicFilter(m_spo2Topic), 1);
    m_client->subscribe(QMqttTopicFilter(m_ecgTopic), 1);
    m_client->subscribe(QMqttTopicFilter("health/vitals"), 1);
    m_client->subscribe(QMqttTopicFilter("health/#"), 1);
    
    qDebug() << "Subscribed to topics:" << m_tempTopic << m_hrTopic << m_spo2Topic << m_ecgTopic;
}

void MqttClient::parseTemperature(const QByteArray& data)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    double temp = 0.0;
    
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        temp = obj["value"].toDouble();
        if (temp == 0.0) temp = obj["temperature"].toDouble();
    } else {
        bool ok;
        temp = QString::fromUtf8(data).toDouble(&ok);
        if (!ok) return;
    }
    
    if (temp > 0) {
        emit temperatureReceived(temp);
    }
}

void MqttClient::parseHeartRate(const QByteArray& data)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    int hr = 0;
    
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        hr = obj["value"].toInt();
        if (hr == 0) hr = obj["heartRate"].toInt();
    } else {
        bool ok;
        hr = QString::fromUtf8(data).toInt(&ok);
        if (!ok) return;
    }
    
    if (hr > 0) {
        emit heartRateReceived(hr);
    }
}

void MqttClient::parseBloodOxygen(const QByteArray& data)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    int spo2 = 0;
    
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        spo2 = obj["value"].toInt();
        if (spo2 == 0) spo2 = obj["spo2"].toInt();
        if (spo2 == 0) spo2 = obj["bloodOxygen"].toInt();
    } else {
        bool ok;
        spo2 = QString::fromUtf8(data).toInt(&ok);
        if (!ok) return;
    }
    
    if (spo2 > 0) {
        emit bloodOxygenReceived(spo2);
    }
}

void MqttClient::parseEcgData(const QByteArray& data)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QVector<double> ecgData;
    
    if (doc.isArray()) {
        QJsonArray arr = doc.array();
        for (const QJsonValue& val : arr) {
            ecgData.append(val.toDouble());
        }
    } else if (doc.isObject()) {
        QJsonObject obj = doc.object();
        QJsonArray arr = obj["data"].toArray();
        if (arr.isEmpty()) arr = obj["ecg"].toArray();
        if (arr.isEmpty()) arr = obj["values"].toArray();
        
        for (const QJsonValue& val : arr) {
            ecgData.append(val.toDouble());
        }
    }
    
    if (!ecgData.isEmpty()) {
        emit ecgDataReceived(ecgData);
    }
}
