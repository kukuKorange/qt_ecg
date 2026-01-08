#pragma once
#include <QObject>
#include <QtMqtt/QtMqtt>
#include <QTimer>
#include "vitaldata.h"

class MqttClient : public QObject {
    Q_OBJECT

public:
    explicit MqttClient(QObject* parent = nullptr);
    ~MqttClient();

    void connectToHost(const QString& host, quint16 port, 
                       const QString& username = QString(), 
                       const QString& password = QString());
    void disconnectFromHost();
    bool isConnected() const;
    
    void setTopics(const QString& tempTopic, const QString& hrTopic,
                   const QString& spo2Topic, const QString& ecgTopic);
    
    QString getStatusText() const;

signals:
    void connected();
    void disconnected();
    void connectionError(const QString& error);
    void temperatureReceived(double temperature);
    void heartRateReceived(int heartRate);
    void bloodOxygenReceived(int spo2);
    void ecgDataReceived(const QVector<double>& ecgData);
    void vitalDataReceived(const VitalData& data);
    void statusChanged(const QString& status);

private slots:
    void onConnected();
    void onDisconnected();
    void onMessageReceived(const QByteArray& message, const QMqttTopicName& topic);
    void onStateChanged(QMqttClient::ClientState state);
    void onErrorChanged(QMqttClient::ClientError error);
    void onReconnectTimer();

private:
    void subscribeToTopics();
    void parseTemperature(const QByteArray& data);
    void parseHeartRate(const QByteArray& data);
    void parseBloodOxygen(const QByteArray& data);
    void parseEcgData(const QByteArray& data);

    QMqttClient* m_client;
    QTimer* m_reconnectTimer;
    
    QString m_host;
    quint16 m_port;
    QString m_username;
    QString m_password;
    
    QString m_tempTopic;
    QString m_hrTopic;
    QString m_spo2Topic;
    QString m_ecgTopic;
    
    bool m_autoReconnect = true;
    int m_reconnectInterval = 5000;
};
