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
#include "models/dlnadevice.h"

class QNetworkReply;

class DLNAManager : public QObject
{
    Q_OBJECT

public:
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
    void requestLocalPlay(const QUrl& url);
    void requestLocalPause();
    void requestLocalStop();
    void requestLocalSeek(qint64 position);
    void requestLocalVolume(int volume);

public slots:
    void onLocalPlaybackStateChanged(const QString& state);
    void onLocalPositionChanged(qint64 position);
    void onLocalDurationChanged(qint64 duration);
    void onLocalVolumeChanged(int volume);

private slots:
    void handleSSDPResponse();
    void sendSSDPDiscover(); // 发送发现消息
    void sendSSDPByebye(); // 发送byebye消息
    void checkDeviceTimeouts();
    void checkPlaybackState();
    void handleUPnPResponse(QNetworkReply* reply);

private:
    // 定义静态成员变量
    // 设备类型
    static const QString UPnP_RootDevice;
    static const QString UPnP_InternetGatewayDevice;
    static const QString UPnP_WANConnectionDevice;
    static const QString UPnP_WANCommonInterfaceConfig;
    static const QString UPnP_WANIPConnection;
    static const QString UPnP_Layer3Forwarding;
    // 服务类型
    static const QString UPnP_MediaServer;
    static const QString UPnP_MediaRenderer;
    static const QString UPnP_ContentDirectory;
    static const QString UPnP_RenderingControl;
    static const QString UPnP_ConnectionManager;
    static const QString UPnP_AVTransport;

    static const QHostAddress SSDP_MULTICAST_ADDR;
    static const quint16 SSDP_PORT = 1900;
    static const int DISCOVERY_INTERVAL = 10000;  // 10秒
    static const int DEVICE_TIMEOUT = 30000;     // 30秒

    QUdpSocket* m_ssdpSocket;
    QNetworkAccessManager* m_networkManager;
    QTimer* m_discoveryTimer;
    QTimer* m_timeoutTimer;
    QTimer* m_monitoringTimer;
    QMap<QString, DLNADevice> m_devices;
    QString m_currentDeviceId;
    bool m_connected;
    QString m_currentPlaybackState;
    QString m_localPlaybackState;
    qint64 m_localPosition;
    qint64 m_localDuration;
    int m_localVolume;

    void clearDevices();
    bool sendUPnPAction(const QString& serviceType, const QString& action, const QMap<QString, QString>& arguments);
    void startPlaybackMonitoring();
    void stopPlaybackMonitoring();
    void parseDeviceDescription(const QByteArray& data, const QString& location);
    void fetchDeviceDescription(const QString& location);
    void addDevice(const DLNADevice& device);
    void removeDevice(const QString& deviceId);
    QString extractHeader(const QString& response, const QString& header);
};

#endif // DLNAMANAGER_H 