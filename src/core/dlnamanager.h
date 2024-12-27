#ifndef DLNAMANAGER_H
#define DLNAMANAGER_H

#include <QObject>
#include <QUrl>
#include <QTimer>
#include <QMutex>
#include <QHostAddress>
#include <memory>
#include "models/dlnadevice.h"

class DLNAManager : public QObject
{
    Q_OBJECT

public:
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

    // SSDP 多播地址
    static const QHostAddress SSDP_MULTICAST_ADDR;

    explicit DLNAManager(QObject *parent = nullptr);
    ~DLNAManager();

    // 设备发现
    void startDiscovery();
    void stopDiscovery();
    QList<DLNADevice> getAvailableDevices() const;

    // 设备连接
    bool connectToDevice(const QString& deviceId);
    void disconnectFromDevice();
    QString getCurrentDeviceId() const;

    // 媒体控制
    void playMedia(const QUrl& url);
    void pauseMedia();
    void stopMedia();
    void seekTo(qint64 position);
    void setVolume(int volume);

signals:
    // 设备相关信号
    void deviceDiscovered(const QString& deviceId, const QString& deviceName);
    void deviceLost(const QString& deviceId);
    void connectionStateChanged(bool connected);

    // 播放状态信号
    void playbackStateChanged(const QString& state);
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void volumeChanged(int volume);
    void error(const QString& message);

    // 本地播放器控制信号
    void requestLocalPlay(const QUrl& url);
    void requestLocalPause();
    void requestLocalStop();
    void requestLocalSeek(qint64 position);
    void requestLocalVolume(int volume);

private slots:
    void handleDeviceDiscovered(const QString& deviceId, const QString& deviceName);
    void handleDeviceLost(const QString& deviceId);
    void handlePlaybackStateChanged(const QString& state);
    void handlePositionChanged(qint64 position);
    void handleDurationChanged(qint64 duration);
    void handleVolumeChanged(int volume);
    void handleError(const QString& message);

private:
    void startPlaybackMonitoring();
    void stopPlaybackMonitoring();
    void cleanupDevice();

    struct Private;
    std::unique_ptr<Private> d;
    QMutex m_mutex;
    bool m_isConnected;
    bool m_isMonitoring;
    QString m_currentDeviceId;
    QTimer* m_monitorTimer;

    // 防止并发访问的辅助方法
    void lockDevice() { m_mutex.lock(); }
    void unlockDevice() { m_mutex.unlock(); }
    bool tryLockDevice() { return m_mutex.tryLock(); }
};

#endif // DLNAMANAGER_H 