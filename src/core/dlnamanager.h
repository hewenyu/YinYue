#ifndef DLNAMANAGER_H
#define DLNAMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QUdpSocket>
#include <QTimer>
#include <QHostAddress>

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
        int port;
    };

    explicit DLNAManager(QObject *parent = nullptr);
    ~DLNAManager();

    // 设备发现
    void startDiscovery();
    void stopDiscovery();
    QList<DLNADevice> getAvailableDevices() const;

    // 设备连接
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
    void error(const QString& errorMessage);

private slots:
    void handleSSDPResponse();
    void checkDeviceTimeouts();
    void handleUPnPResponse(QNetworkReply* reply);

private:
    // SSDP 相关
    void sendSSDPDiscover();
    void processSSDPMessage(const QByteArray& data, const QHostAddress& sender);

    // UPnP 相关
    void fetchDeviceDescription(const QString& location);
    void parseDeviceDescription(const QByteArray& data);
    bool sendUPnPAction(const QString& serviceType, const QString& actionName, const QMap<QString, QString>& arguments);

    // 设备管理
    void addDevice(const DLNADevice& device);
    void removeDevice(const QString& deviceId);
    void clearDevices();

    QUdpSocket* m_ssdpSocket;
    QNetworkAccessManager* m_networkManager;
    QTimer* m_discoveryTimer;
    QTimer* m_timeoutTimer;
    
    QMap<QString, DLNADevice> m_devices;
    QMap<QString, QDateTime> m_deviceTimeouts;
    
    QString m_currentDeviceId;
    bool m_connected;
    
    // SSDP 常量
    const int SSDP_PORT = 1900;
    const QHostAddress SSDP_MULTICAST_ADDR = QHostAddress("239.255.255.250");
    const int DISCOVERY_INTERVAL = 5000;  // 5秒
    const int DEVICE_TIMEOUT = 30000;     // 30秒
};

#endif // DLNAMANAGER_H 