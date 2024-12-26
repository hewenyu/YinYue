#include "dlnamanager.h"
#include <QNetworkReply>
#include <QDateTime>
#include <QXmlStreamReader>
#include <QDebug>

DLNAManager::DLNAManager(QObject *parent)
    : QObject(parent)
    , m_ssdpSocket(new QUdpSocket(this))
    , m_networkManager(new QNetworkAccessManager(this))
    , m_discoveryTimer(new QTimer(this))
    , m_timeoutTimer(new QTimer(this))
    , m_connected(false)
{
    // 设置SSDP套接字
    m_ssdpSocket->bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress);
    connect(m_ssdpSocket, &QUdpSocket::readyRead, this, &DLNAManager::handleSSDPResponse);

    // 设置发现定时器
    m_discoveryTimer->setInterval(DISCOVERY_INTERVAL);
    connect(m_discoveryTimer, &QTimer::timeout, this, &DLNAManager::sendSSDPDiscover);

    // 设置超时检查定时器
    m_timeoutTimer->setInterval(1000);  // 每秒检查一次
    connect(m_timeoutTimer, &QTimer::timeout, this, &DLNAManager::checkDeviceTimeouts);
}

DLNAManager::~DLNAManager()
{
    stopDiscovery();
}

void DLNAManager::startDiscovery()
{
    qDebug() << "Starting DLNA device discovery";
    clearDevices();
    sendSSDPDiscover();
    m_discoveryTimer->start();
    m_timeoutTimer->start();
}

void DLNAManager::stopDiscovery()
{
    qDebug() << "Stopping DLNA device discovery";
    m_discoveryTimer->stop();
    m_timeoutTimer->stop();
}

QList<DLNAManager::DLNADevice> DLNAManager::getAvailableDevices() const
{
    return m_devices.values();
}

bool DLNAManager::connectToDevice(const QString& deviceId)
{
    if (!m_devices.contains(deviceId)) {
        emit error(tr("Device not found"));
        return false;
    }

    if (m_connected) {
        disconnectFromDevice();
    }

    const DLNADevice& device = m_devices[deviceId];
    // TODO: 实现设备连接逻辑
    // 1. 获取设备的服务描述
    // 2. 建立控制连接
    // 3. 订阅事件通知

    m_currentDeviceId = deviceId;
    m_connected = true;
    emit connectionStateChanged(true);
    return true;
}

void DLNAManager::disconnectFromDevice()
{
    if (m_connected) {
        // TODO: 实现设备断开连接逻辑
        // 1. 取消事件订阅
        // 2. 关闭控制连接

        m_currentDeviceId.clear();
        m_connected = false;
        emit connectionStateChanged(false);
    }
}

bool DLNAManager::isConnected() const
{
    return m_connected;
}

QString DLNAManager::getCurrentDeviceId() const
{
    return m_currentDeviceId;
}

void DLNAManager::sendSSDPDiscover()
{
    QByteArray ssdpRequest = 
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 3\r\n"
        "ST: urn:schemas-upnp-org:device:MediaRenderer:1\r\n"
        "\r\n";

    m_ssdpSocket->writeDatagram(ssdpRequest, SSDP_MULTICAST_ADDR, SSDP_PORT);
}

void DLNAManager::handleSSDPResponse()
{
    while (m_ssdpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_ssdpSocket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        m_ssdpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        processSSDPMessage(datagram, sender);
    }
}

void DLNAManager::processSSDPMessage(const QByteArray& data, const QHostAddress& sender)
{
    QString message = QString::fromUtf8(data);
    QStringList lines = message.split("\r\n", Qt::SkipEmptyParts);

    QString location;
    QString usn;

    for (const QString& line : lines) {
        if (line.startsWith("LOCATION:", Qt::CaseInsensitive)) {
            location = line.mid(9).trimmed();
        } else if (line.startsWith("USN:", Qt::CaseInsensitive)) {
            usn = line.mid(4).trimmed();
        }
    }

    if (!location.isEmpty() && !usn.isEmpty()) {
        fetchDeviceDescription(location);
        m_deviceTimeouts[usn] = QDateTime::currentDateTime();
    }
}

void DLNAManager::fetchDeviceDescription(const QString& location)
{
    QNetworkRequest request(location);
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            parseDeviceDescription(reply->readAll());
        }
        reply->deleteLater();
    });
}

void DLNAManager::parseDeviceDescription(const QByteArray& data)
{
    QXmlStreamReader xml(data);
    DLNADevice device;

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name() == "device") {
                // 解析设备信息
                while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "device")) {
                    if (xml.tokenType() == QXmlStreamReader::StartElement) {
                        if (xml.name() == "UDN") {
                            device.id = xml.readElementText();
                        } else if (xml.name() == "friendlyName") {
                            device.name = xml.readElementText();
                        } else if (xml.name() == "deviceType") {
                            device.type = xml.readElementText();
                        }
                    }
                    xml.readNext();
                }
            }
        }
    }

    if (!xml.hasError() && !device.id.isEmpty()) {
        addDevice(device);
    }
}

void DLNAManager::checkDeviceTimeouts()
{
    QDateTime now = QDateTime::currentDateTime();
    QList<QString> expiredDevices;

    for (auto it = m_deviceTimeouts.begin(); it != m_deviceTimeouts.end(); ++it) {
        if (it.value().msecsTo(now) > DEVICE_TIMEOUT) {
            expiredDevices.append(it.key());
        }
    }

    for (const QString& deviceId : expiredDevices) {
        removeDevice(deviceId);
        m_deviceTimeouts.remove(deviceId);
    }
}

void DLNAManager::addDevice(const DLNADevice& device)
{
    if (!m_devices.contains(device.id)) {
        m_devices[device.id] = device;
        emit deviceDiscovered(device.id, device.name);
    }
}

void DLNAManager::removeDevice(const QString& deviceId)
{
    if (m_devices.remove(deviceId) > 0) {
        emit deviceLost(deviceId);
        if (m_currentDeviceId == deviceId) {
            disconnectFromDevice();
        }
    }
}

void DLNAManager::clearDevices()
{
    QStringList deviceIds = m_devices.keys();
    m_devices.clear();
    m_deviceTimeouts.clear();
    
    for (const QString& deviceId : deviceIds) {
        emit deviceLost(deviceId);
    }
    
    if (!m_currentDeviceId.isEmpty()) {
        disconnectFromDevice();
    }
}

bool DLNAManager::playMedia(const QUrl& url)
{
    if (!m_connected) return false;
    
    QMap<QString, QString> args;
    args["CurrentURI"] = url.toString();
    args["CurrentURIMetaData"] = "";  // TODO: 添加媒体元数据
    
    return sendUPnPAction("urn:schemas-upnp-org:service:AVTransport:1", "SetAVTransportURI", args) &&
           sendUPnPAction("urn:schemas-upnp-org:service:AVTransport:1", "Play", {{"Speed", "1"}});
}

bool DLNAManager::pauseMedia()
{
    if (!m_connected) return false;
    return sendUPnPAction("urn:schemas-upnp-org:service:AVTransport:1", "Pause", {});
}

bool DLNAManager::stopMedia()
{
    if (!m_connected) return false;
    return sendUPnPAction("urn:schemas-upnp-org:service:AVTransport:1", "Stop", {});
}

bool DLNAManager::setVolume(int volume)
{
    if (!m_connected) return false;
    QMap<QString, QString> args;
    args["DesiredVolume"] = QString::number(volume);
    return sendUPnPAction("urn:schemas-upnp-org:service:RenderingControl:1", "SetVolume", args);
}

bool DLNAManager::seekTo(qint64 position)
{
    if (!m_connected) return false;
    QMap<QString, QString> args;
    args["Target"] = QString::number(position);
    return sendUPnPAction("urn:schemas-upnp-org:service:AVTransport:1", "Seek", args);
}

bool DLNAManager::sendUPnPAction(const QString& serviceType, const QString& actionName, const QMap<QString, QString>& arguments)
{
    // TODO: 实现UPnP动作发送
    // 1. 构建SOAP消息
    // 2. 发送HTTP POST请求
    // 3. 处理响应
    return false;  // 临时返回值
}

void DLNAManager::handleUPnPResponse(QNetworkReply* reply)
{
    // TODO: 实现UPnP响应处理
    reply->deleteLater();
} 