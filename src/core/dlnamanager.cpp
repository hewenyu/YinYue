#include "dlnamanager.h"
#include <QDebug>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>
#include <QtXml/QDomDocument>
#include <QHostAddress>
#include <QUdpSocket>

// 定义静态成员变量
// 设备类型
const QString DLNAManager::UPnP_RootDevice = "upnp:rootdevice";
const QString DLNAManager::UPnP_InternetGatewayDevice = "urn:schemas-upnp-org:device:InternetGatewayDevice:1";
const QString DLNAManager::UPnP_WANConnectionDevice = "urn:schemas-upnp-org:device:WANConnectionDevice:1";
const QString DLNAManager::UPnP_WANCommonInterfaceConfig = "urn:schemas-upnp-org:service:WANCommonInterfaceConfig:1";
const QString DLNAManager::UPnP_WANIPConnection = "urn:schemas-upnp-org:device:WANConnectionDevice:1";
const QString DLNAManager::UPnP_Layer3Forwarding = "urn:schemas-upnp-org:service:WANIPConnection:1";

// 服务类型
const QString DLNAManager::UPnP_MediaServer = "urn:schemas-upnp-org:device:MediaServer:1";
const QString DLNAManager::UPnP_MediaRenderer = "urn:schemas-upnp-org:device:MediaRenderer:1";
const QString DLNAManager::UPnP_ContentDirectory = "urn:schemas-upnp-org:service:ContentDirectory:1";
const QString DLNAManager::UPnP_RenderingControl = "urn:schemas-upnp-org:service:RenderingControl:1";
const QString DLNAManager::UPnP_ConnectionManager = "urn:schemas-upnp-org:service:ConnectionManager:1";
const QString DLNAManager::UPnP_AVTransport = "urn:schemas-upnp-org:service:AVTransport:1";

// 定义静态成员变量
const QHostAddress DLNAManager::SSDP_MULTICAST_ADDR = QHostAddress("239.255.255.250");

struct DLNAManager::Private {
    QList<DLNADevice> devices;
    // 其他私有成员...
};

DLNAManager::DLNAManager(QObject *parent)
    : QObject(parent)
    , d(new Private)
    , m_isConnected(false)
    , m_isMonitoring(false)
    , m_monitorTimer(new QTimer(this))
{
    m_monitorTimer->setInterval(1000);  // 1秒检查一次
    connect(m_monitorTimer, &QTimer::timeout, this, [this]() {
        if (m_isConnected && !m_currentDeviceId.isEmpty()) {
            // 检查设备状态...
            emit playbackStateChanged("Playing");  // 临时模拟播放状态
        }
    });
}

DLNAManager::~DLNAManager()
{
    stopPlaybackMonitoring();
    disconnectFromDevice();
}

void DLNAManager::startDiscovery()
{
    qDebug() << "Starting DLNA device discovery";
    
    // 清除现有设备列表
    d->devices.clear();
    
    // 创建测试设备
    DLNADevice testDevice;
    testDevice.setId("test_device_1");
    testDevice.setName("Test DLNA Device");
    testDevice.setDeviceType(UPnP_MediaRenderer);
    testDevice.setAddress(QHostAddress::LocalHost);
    testDevice.setPort(8000);
    
    // 添加测试设备到列表
    d->devices.append(testDevice);
    
    // 发出设备发现信号
    emit deviceDiscovered(testDevice.id(), testDevice.name());
    
    qDebug() << "Test device discovered:" << testDevice.name() 
             << "ID:" << testDevice.id()
             << "Type:" << testDevice.deviceType()
             << "Address:" << testDevice.address().toString()
             << "Port:" << testDevice.port();
}

void DLNAManager::stopDiscovery()
{
    qDebug() << "Stopping DLNA device discovery";
    // 在这里可以添加清理代码
}

QList<DLNADevice> DLNAManager::getAvailableDevices() const
{
    return d->devices;
}

bool DLNAManager::connectToDevice(const QString& deviceId)
{
    qDebug() << "Attempting to connect to device:" << deviceId;
    
    lockDevice();
    if (m_isConnected) {
        disconnectFromDevice();
    }

    // 查找设备
    bool found = false;
    for (const auto& device : d->devices) {
        if (device.id() == deviceId) {
            found = true;
            qDebug() << "Found device:" << device.name() 
                     << "Type:" << device.deviceType()
                     << "Address:" << device.address().toString()
                     << "Port:" << device.port();
            break;
        }
    }

    if (!found) {
        unlockDevice();
        qDebug() << "Device not found:" << deviceId;
        emit error("Device not found");
        return false;
    }

    qDebug() << "Connecting to DLNA device:" << deviceId;
    m_currentDeviceId = deviceId;
    m_isConnected = true;
    startPlaybackMonitoring();
    unlockDevice();

    emit connectionStateChanged(true);
    qDebug() << "Successfully connected to device:" << deviceId;
    return true;
}

void DLNAManager::disconnectFromDevice()
{
    qDebug() << "Disconnecting from device:" << m_currentDeviceId;
    lockDevice();
    if (m_isConnected) {
        stopPlaybackMonitoring();
        stopMedia();
        m_currentDeviceId.clear();
        m_isConnected = false;
        emit connectionStateChanged(false);
        qDebug() << "Device disconnected";
    }
    unlockDevice();
}

QString DLNAManager::getCurrentDeviceId() const
{
    return m_currentDeviceId;
}

void DLNAManager::playMedia(const QUrl& url)
{
    if (!m_isConnected) {
        emit error("Not connected to any device");
        return;
    }
    qDebug() << "Playing media on DLNA device:" << url.toString();
    emit playbackStateChanged("Playing");
}

void DLNAManager::pauseMedia()
{
    if (!m_isConnected) {
        emit error("Not connected to any device");
        return;
    }
    qDebug() << "Pausing media on DLNA device";
    emit playbackStateChanged("Paused");
}

void DLNAManager::stopMedia()
{
    if (!m_isConnected) {
        emit error("Not connected to any device");
        return;
    }
    qDebug() << "Stopping media on DLNA device";
    emit playbackStateChanged("Stopped");
}

void DLNAManager::seekTo(qint64 position)
{
    if (!m_isConnected) {
        emit error("Not connected to any device");
        return;
    }
    qDebug() << "Seeking to position:" << position;
    emit positionChanged(position);
}

void DLNAManager::setVolume(int volume)
{
    if (!m_isConnected) {
        emit error("Not connected to any device");
        return;
    }
    qDebug() << "Setting volume to:" << volume;
    emit volumeChanged(volume);
}

void DLNAManager::startPlaybackMonitoring()
{
    if (!m_isMonitoring) {
        m_isMonitoring = true;
        m_monitorTimer->start();
    }
}

void DLNAManager::stopPlaybackMonitoring()
{
    if (m_isMonitoring) {
        m_monitorTimer->stop();
        m_isMonitoring = false;
    }
}

void DLNAManager::cleanupDevice()
{
    stopPlaybackMonitoring();
    m_currentDeviceId.clear();
    m_isConnected = false;
}

// 处理设备事件
void DLNAManager::handleDeviceDiscovered(const QString& deviceId, const QString& deviceName)
{
    DLNADevice device;
    device.setId(deviceId);
    device.setName(deviceName);
    d->devices.append(device);
    emit deviceDiscovered(deviceId, deviceName);
}

void DLNAManager::handleDeviceLost(const QString& deviceId)
{
    for (int i = 0; i < d->devices.size(); ++i) {
        if (d->devices[i].id() == deviceId) {
            d->devices.removeAt(i);
            break;
        }
    }
    emit deviceLost(deviceId);
}

void DLNAManager::handlePlaybackStateChanged(const QString& state)
{
    emit playbackStateChanged(state);
}

void DLNAManager::handlePositionChanged(qint64 position)
{
    emit positionChanged(position);
}

void DLNAManager::handleDurationChanged(qint64 duration)
{
    emit durationChanged(duration);
}

void DLNAManager::handleVolumeChanged(int volume)
{
    emit volumeChanged(volume);
}

void DLNAManager::handleError(const QString& message)
{
    emit error(message);
} 
