#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include "vitaldata.h"

class DataManager;

class CloudSyncer : public QObject {
    Q_OBJECT

public:
    explicit CloudSyncer(DataManager* dataManager, QObject* parent = nullptr);
    ~CloudSyncer();

    void setServerUrl(const QString& url);
    void setApiKey(const QString& apiKey);
    void setDeviceId(const QString& deviceId);
    
    void startAutoSync(int intervalSeconds = 60);
    void stopAutoSync();
    
    void syncNow();
    void uploadData(const VitalData& data);
    
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    
    QString getLastSyncTime() const;
    QString getSyncStatus() const { return m_syncStatus; }

signals:
    void syncStarted();
    void syncCompleted(bool success);
    void syncProgress(int current, int total);
    void statusChanged(const QString& status);
    void error(const QString& message);

private slots:
    void onAutoSyncTimer();
    void onReplyFinished(QNetworkReply* reply);

private:
    void sendData(const QJsonObject& data);
    
    DataManager* m_dataManager;
    QNetworkAccessManager* m_networkManager;
    QTimer* m_autoSyncTimer;
    
    QString m_serverUrl;
    QString m_apiKey;
    QString m_deviceId;
    
    bool m_enabled = false;
    bool m_syncing = false;
    QDateTime m_lastSyncTime;
    QString m_syncStatus;
    
    QVector<VitalData> m_pendingData;
    int m_currentSyncIndex = 0;
};
