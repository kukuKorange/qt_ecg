#include "cloudsyncer.h"
#include "datamanager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QDebug>

CloudSyncer::CloudSyncer(DataManager* dataManager, QObject* parent)
    : QObject(parent)
    , m_dataManager(dataManager)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_autoSyncTimer(new QTimer(this))
    , m_serverUrl("https://api.healthmonitor.example.com/v1/data")
    , m_deviceId("default_device")
{
    connect(m_autoSyncTimer, &QTimer::timeout, this, &CloudSyncer::onAutoSyncTimer);
    connect(m_networkManager, &QNetworkAccessManager::finished, 
            this, &CloudSyncer::onReplyFinished);
    
    m_syncStatus = QStringLiteral("未同步");
}

CloudSyncer::~CloudSyncer()
{
    stopAutoSync();
}

void CloudSyncer::setServerUrl(const QString& url)
{
    m_serverUrl = url;
}

void CloudSyncer::setApiKey(const QString& apiKey)
{
    m_apiKey = apiKey;
}

void CloudSyncer::setDeviceId(const QString& deviceId)
{
    m_deviceId = deviceId;
}

void CloudSyncer::startAutoSync(int intervalSeconds)
{
    m_autoSyncTimer->start(intervalSeconds * 1000);
    m_syncStatus = QStringLiteral("自动同步已启用");
    emit statusChanged(m_syncStatus);
}

void CloudSyncer::stopAutoSync()
{
    m_autoSyncTimer->stop();
    m_syncStatus = QStringLiteral("自动同步已停止");
    emit statusChanged(m_syncStatus);
}

void CloudSyncer::syncNow()
{
    if (!m_enabled) {
        emit error(QStringLiteral("云同步未启用"));
        return;
    }
    
    if (m_syncing) {
        return;
    }
    
    m_syncing = true;
    emit syncStarted();
    m_syncStatus = QStringLiteral("正在同步...");
    emit statusChanged(m_syncStatus);
    
    // 获取未同步的数据
    QDateTime lastSync = m_lastSyncTime.isValid() ? m_lastSyncTime : QDateTime::currentDateTime().addDays(-1);
    m_pendingData = m_dataManager->getVitalDataRange(lastSync, QDateTime::currentDateTime());
    
    if (m_pendingData.isEmpty()) {
        m_syncing = false;
        m_syncStatus = QStringLiteral("无数据需要同步");
        emit statusChanged(m_syncStatus);
        emit syncCompleted(true);
        return;
    }
    
    m_currentSyncIndex = 0;
    
    // 批量发送数据
    QJsonObject payload;
    payload["deviceId"] = m_deviceId;
    payload["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonArray dataArray;
    for (const VitalData& data : m_pendingData) {
        dataArray.append(data.toJson());
    }
    payload["data"] = dataArray;
    
    sendData(payload);
}

void CloudSyncer::uploadData(const VitalData& data)
{
    if (!m_enabled) return;
    
    QJsonObject payload;
    payload["deviceId"] = m_deviceId;
    payload["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    payload["data"] = data.toJson();
    
    sendData(payload);
}

void CloudSyncer::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (enabled) {
        m_syncStatus = QStringLiteral("云同步已启用");
    } else {
        m_syncStatus = QStringLiteral("云同步已禁用");
        stopAutoSync();
    }
    emit statusChanged(m_syncStatus);
}

QString CloudSyncer::getLastSyncTime() const
{
    if (!m_lastSyncTime.isValid()) {
        return QStringLiteral("从未同步");
    }
    return m_lastSyncTime.toString("yyyy-MM-dd HH:mm:ss");
}

void CloudSyncer::onAutoSyncTimer()
{
    syncNow();
}

void CloudSyncer::onReplyFinished(QNetworkReply* reply)
{
    m_syncing = false;
    
    if (reply->error() == QNetworkReply::NoError) {
        m_lastSyncTime = QDateTime::currentDateTime();
        m_syncStatus = QStringLiteral("同步成功 - %1").arg(m_lastSyncTime.toString("HH:mm:ss"));
        emit syncCompleted(true);
    } else {
        m_syncStatus = QStringLiteral("同步失败: %1").arg(reply->errorString());
        emit error(m_syncStatus);
        emit syncCompleted(false);
    }
    
    emit statusChanged(m_syncStatus);
    reply->deleteLater();
}

void CloudSyncer::sendData(const QJsonObject& data)
{
    QUrl url(m_serverUrl);
    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    if (!m_apiKey.isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());
    }
    
    QJsonDocument doc(data);
    m_networkManager->post(request, doc.toJson());
}
