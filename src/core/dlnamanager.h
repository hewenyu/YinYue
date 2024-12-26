#ifndef DLNAMANAGER_H
#define DLNAMANAGER_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QThread>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QMap>
#include <QDateTime>
#include <QHostAddress>

class QNetworkReply;

class DLNAManager : public QObject
{
    Q_OBJECT

public:
    struct DLNADevice {
        QString id;
        QString name;
        QString location;
        QString type;
        QHostAddress address;
        quint16 port;
        QDateTime lastSeen;

        DLNADevice() : port(0) {}
        DLNADevice(const QString& _id, const QString& _name, const QString& _location)
            : id(_id), name(_name), location(_location), port(0), lastSeen(QDateTime::currentDateTime()) {}
    };

    explicit DLNAManager(QObject *parent = nullptr);
    ~DLNAManager();

    void startDiscovery();
    void stopDiscovery();
    QList<DLNADevice> getAvailableDevices() const;
    bool connectToDevice(const QString& deviceId);
    void disconnectFromDevice();
    bool isConnected() const;
    QString getCurrentDeviceId() const;

    // 媒体控制
    bool playMedia(const QUrl& url);
    bool pauseMedia();
    bool stopMedia();
    bool setVolume(int volume);
    bool seekTo(qint64 position);

signals:
    void deviceDiscovered(const QString& deviceId, const QString& deviceName);
    void deviceLost(const QString& deviceId);
    void connectionStateChanged(bool connected);
    void playbackStateChanged(const QString& state);
    void error(const QString& message);

private slots:
    void handleSSDPResponse();
    void sendSSDPDiscover();
    void checkDeviceTimeouts();
    void checkPlaybackState();
    void handleUPnPResponse(QNetworkReply* reply);

private:
    static const QHostAddress SSDP_MULTICAST_ADDR;
    static const quint16 SSDP_PORT = 1900;
    static const int DISCOVERY_INTERVAL = 5000;  // 5秒
    static const int DEVICE_TIMEOUT = 30000;     // 30秒

    QUdpSocket* m_ssdpSocket;
    QNetworkAccessManager* m_networkManager;
    QTimer* m_discoveryTimer;
    QTimer* m_timeoutTimer;
    QTimer* m_monitoringTimer;
    QMap<QString, DLNADevice> m_devices;
    QMap<QString, QDateTime> m_deviceTimeouts;
    QString m_currentDeviceId;
    bool m_connected;
    QString m_currentPlaybackState;
    QMap<QString, QString> m_lastResponse;

    void clearDevices();
    bool sendUPnPAction(const QString& serviceType, const QString& action, const QMap<QString, QString>& arguments);
    void startPlaybackMonitoring();
    void stopPlaybackMonitoring();
    void parseDeviceDescription(const QByteArray& data);
    void fetchDeviceDescription(const QString& location);
    void addDevice(const DLNADevice& device);
    void removeDevice(const QString& deviceId);
    QString extractHeader(const QString& response, const QString& header);
};

QDebug operator<<(QDebug debug, const DLNAManager::DLNADevice& device);

#endif // DLNAMANAGER_H 