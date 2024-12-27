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
    sendSSDPByebye();
    m_discoveryTimer->stop();
    m_timeoutTimer->stop();
}

QList<DLNADevice> DLNAManager::getAvailableDevices() const
{
    QList<DLNADevice> devices = m_devices.values();
    qDebug() << "\n当前可用设备列表:";
    qDebug() << "设备数量:" << devices.size();
    for (const auto& device : devices) {
        qDebug() << "  设备ID:" << device.UDN;
        qDebug() << "  设备名称:" << device.friendlyName;
        qDebug() << "  设备类型:" << device.deviceType;
        qDebug() << "  设备地址:" << device.address.toString() << ":" << device.port;
        qDebug() << "";
    }
    return devices;
}

bool DLNAManager::connectToDevice(const QString& deviceId)
{
    qDebug() << "尝试连接设备:" << deviceId;
    
    if (!m_devices.contains(deviceId)) {
        qDebug() << "设备未找到:" << deviceId;
        emit error(tr("Device not found"));
        return false;
    }

    if (m_connected && m_currentDeviceId == deviceId) {
        qDebug() << "设备已连接:" << deviceId;
        return true;
    }

    if (m_connected) {
        disconnectFromDevice();
    }

    const DLNADevice& device = m_devices[deviceId];
    qDebug() << "正在连接到设备:" << device.friendlyName;
    
    // 验证设备是否支持AVTransport服务
    if (!device.hasService(UPnP_AVTransport)) {
        qDebug() << "设备不支持AVTransport服务";
        emit error(tr("Device does not support media playback"));
        return false;
    }
    
    // 设置当前设备ID
    m_currentDeviceId = deviceId;
    
    // 尝试发送一个简单的UPnP动作来测试连接
    QMap<QString, QString> args;
    args["InstanceID"] = "0";
    
    bool success = sendUPnPAction(UPnP_AVTransport, "GetTransportInfo", args);
    
    if (success) {
        m_connected = true;
        emit connectionStateChanged(true);
        qDebug() << "设备连接成功:" << device.friendlyName;
        startPlaybackMonitoring();
        return true;
    }
    
    m_currentDeviceId.clear();
    qDebug() << "设备连接失败:" << deviceId;
    emit error(tr("Failed to connect to device"));
    return false;
}

void DLNAManager::disconnectFromDevice()
{
    if (m_connected) {
        qDebug() << "正在断开设备连接:" << m_currentDeviceId;
        stopMedia();
        QString oldDeviceId = m_currentDeviceId;
        m_currentDeviceId.clear();
        m_connected = false;
        emit connectionStateChanged(false);
        qDebug() << "设备已断开连接:" << oldDeviceId;
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
        "ST: " + DLNAManager::UPnP_MediaRenderer.toUtf8() + "\r\n"
        "\r\n";

    qDebug() << "发送 SSDP 发现请求 (MediaRenderer):" << QString::fromUtf8(ssdpRequest);
    m_ssdpSocket->writeDatagram(ssdpRequest, SSDP_MULTICAST_ADDR, SSDP_PORT);
}

void DLNAManager::sendSSDPByebye()
{
    // 发送byebye消息通知设备离线
    if (!m_currentDeviceId.isEmpty()) {
        const DLNADevice& device = m_devices[m_currentDeviceId];
        QByteArray byebyeMessage = 
            "NOTIFY * HTTP/1.1\r\n"
            "HOST: 239.255.255.250:1900\r\n"
            "NT: " + device.deviceType.toUtf8() + "\r\n"
            "NTS: ssdp:byebye\r\n"
            "USN: " + device.UDN.toUtf8() + "\r\n"
            "\r\n";
        
        m_ssdpSocket->writeDatagram(byebyeMessage, SSDP_MULTICAST_ADDR, SSDP_PORT);
    }
}

void DLNAManager::handleSSDPResponse()
{
    while (m_ssdpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_ssdpSocket->pendingDatagramSize());
        QHostAddress senderAddress;
        quint16 senderPort;

        m_ssdpSocket->readDatagram(datagram.data(), datagram.size(), &senderAddress, &senderPort);
        QString response = QString::fromUtf8(datagram);

        // 解析设备位置
        QString location = extractHeader(response, "LOCATION");
        if (location.isEmpty()) {
            continue;
        }

        // 解析设备ID
        QString usn = extractHeader(response, "USN");
        if (usn.isEmpty()) {
            continue;
        }

        // 获取设备描述
        fetchDeviceDescription(location);
    }
}

void DLNAManager::fetchDeviceDescription(const QString& location)
{
    QUrl url(location);
    if (!url.isValid()) {
        return;
    }

    QNetworkRequest request(url);
    QNetworkReply* reply = m_networkManager->get(request);
    
    connect(reply, &QNetworkReply::finished, [this, reply, location]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            parseDeviceDescription(data, location);
        }
        reply->deleteLater();
    });
}

void DLNAManager::parseDeviceDescription(const QByteArray& data, const QString& location)
{
    QDomDocument doc;
    if (!doc.setContent(data)) {
        qDebug() << "Failed to parse device description XML";
        return;
    }

    QUrl locationUrl(location);
    QHostAddress deviceAddress(locationUrl.host());
    DLNADevice device = DLNADevice::fromXml(doc, deviceAddress, locationUrl.port());
    
    // 检查设备是否有效
    if (device.UDN.isEmpty()) {
        qDebug() << "Invalid device description";
        return;
    }

    // 更新或添加设备
    bool isNewDevice = !m_devices.contains(device.UDN);
    m_devices[device.UDN] = device;
    
    if (isNewDevice) {
        emit deviceDiscovered(device.UDN, device.friendlyName);
    }
}

void DLNAManager::addDevice(const DLNADevice& device)
{
    QString deviceId = device.UDN;
    bool isNew = !m_devices.contains(deviceId);
    
    m_devices[deviceId] = device;
    
    if (isNew) {
        emit deviceDiscovered(deviceId, device.friendlyName);
    }
}

void DLNAManager::removeDevice(const QString& deviceId)
{
    if (m_devices.contains(deviceId)) {
        if (m_currentDeviceId == deviceId) {
            disconnectFromDevice();
        }
        m_devices.remove(deviceId);
        emit deviceLost(deviceId);
    }
}

void DLNAManager::checkDeviceTimeouts()
{
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    QList<QString> devicesToRemove;
    
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        if (currentTime - it.value().lastSeenTime > DEVICE_TIMEOUT) {
            devicesToRemove.append(it.key());
        }
    }
    
    for (const QString& deviceId : devicesToRemove) {
        removeDevice(deviceId);
    }
}

QString DLNAManager::extractHeader(const QString& response, const QString& header)
{
    QStringList lines = response.split("\r\n");
    QString headerPrefix = header + ":";
    
    for (const QString& line : lines) {
        if (line.startsWith(headerPrefix, Qt::CaseInsensitive)) {
            return line.mid(headerPrefix.length()).trimmed();
        }
    }
    
    return QString();
}

bool DLNAManager::sendUPnPAction(const QString& serviceType, const QString& action, 
                                const QMap<QString, QString>& arguments)
{
    if (!m_connected || !m_devices.contains(m_currentDeviceId)) {
        return false;
    }

    const DLNADevice& device = m_devices[m_currentDeviceId];
    if (!device.hasService(serviceType)) {
        return false;
    }

    DLNAService service = device.getService(serviceType);
    QString controlUrl = device.getFullUrl(service.controlURL);
    
    // 构建SOAP请求
    QString soapBody = QString(
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
        "<s:Body>\n"
        "<u:%1 xmlns:u=\"%2\">\n").arg(action, serviceType);
    
    for (auto it = arguments.begin(); it != arguments.end(); ++it) {
        soapBody += QString("<%1>%2</%1>\n").arg(it.key(), it.value());
    }
    
    soapBody += QString("</u:%1>\n"
                       "</s:Body>\n"
                       "</s:Envelope>\n").arg(action);

    QNetworkRequest request(controlUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml; charset=\"utf-8\"");
    request.setRawHeader("SOAPAction", QString("\"%1#%2\"").arg(serviceType, action).toUtf8());

    QNetworkReply* reply = m_networkManager->post(request, soapBody.toUtf8());
    
    // 等待响应
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    bool success = (reply->error() == QNetworkReply::NoError);
    reply->deleteLater();
    
    return success;
}

// 媒体控制方法实现
bool DLNAManager::playMedia(const QUrl& url)
{
    QMap<QString, QString> args;
    args["InstanceID"] = "0";
    args["CurrentURI"] = url.toString();
    args["CurrentURIMetaData"] = "";
    
    if (sendUPnPAction(UPnP_AVTransport, "SetAVTransportURI", args)) {
        args.clear();
        args["InstanceID"] = "0";
        args["Speed"] = "1";
        return sendUPnPAction(UPnP_AVTransport, "Play", args);
    }
    return false;
}

bool DLNAManager::pauseMedia()
{
    QMap<QString, QString> args;
    args["InstanceID"] = "0";
    return sendUPnPAction(UPnP_AVTransport, "Pause", args);
}

bool DLNAManager::stopMedia()
{
    QMap<QString, QString> args;
    args["InstanceID"] = "0";
    return sendUPnPAction(UPnP_AVTransport, "Stop", args);
}

bool DLNAManager::setVolume(int volume)
{
    QMap<QString, QString> args;
    args["InstanceID"] = "0";
    args["Channel"] = "Master";
    args["DesiredVolume"] = QString::number(volume);
    return sendUPnPAction(UPnP_RenderingControl, "SetVolume", args);
}

bool DLNAManager::seekTo(qint64 position)
{
    QMap<QString, QString> args;
    args["InstanceID"] = "0";
    args["Unit"] = "REL_TIME";
    args["Target"] = QString("%1:%2:%3")
        .arg(position / 3600000)
        .arg((position % 3600000) / 60000, 2, 10, QChar('0'))
        .arg((position % 60000) / 1000, 2, 10, QChar('0'));
    return sendUPnPAction(UPnP_AVTransport, "Seek", args);
}

void DLNAManager::clearDevices()
{
    m_devices.clear();
}

void DLNAManager::startPlaybackMonitoring()
{
    if (!m_monitoringTimer) {
        m_monitoringTimer = new QTimer(this);
        m_monitoringTimer->setInterval(1000);
        connect(m_monitoringTimer, &QTimer::timeout, this, &DLNAManager::checkPlaybackState);
    }
    m_monitoringTimer->start();
}

void DLNAManager::stopPlaybackMonitoring()
{
    if (m_monitoringTimer) {
        m_monitoringTimer->stop();
    }
}

void DLNAManager::checkPlaybackState()
{
    if (!m_connected) {
        return;
    }

    QMap<QString, QString> args;
    args["InstanceID"] = "0";
    
    if (sendUPnPAction(UPnP_AVTransport, "GetTransportInfo", args)) {
        // 获取播放状态
        // 注意：这里需要解析响应来获取实际状态
        // 但由于我们没有保存响应内容，这里只是示例
        QString currentState = "UNKNOWN";
        if (currentState != m_currentPlaybackState) {
            m_currentPlaybackState = currentState;
            emit playbackStateChanged(currentState);
        }
    }
} 
