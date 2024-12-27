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
    , m_connected(false)
{
    // 设置SSDP套接字
    m_ssdpSocket->bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress);
    connect(m_ssdpSocket, &QUdpSocket::readyRead, this, &DLNAManager::handleSSDPResponse);

    // 设置发现定时器
    m_discoveryTimer->setInterval(DISCOVERY_INTERVAL);
    connect(m_discoveryTimer, &QTimer::timeout, this, &DLNAManager::sendSSDPDiscover);
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
}

void DLNAManager::stopDiscovery()
{
    qDebug() << "Stopping DLNA device discovery";
    m_discoveryTimer->stop();
    clearDevices();
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
    qDebug() << "\n尝试连接设备:" << deviceId;
    
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
    
    // 验�����设备是否支持AVTransport服务
    if (!device.hasService(UPnP_AVTransport)) {
        qDebug() << "设备不支持AVTransport服务";
        emit error(tr("Device does not support media playback"));
        return false;
    }
    
    // 先设置连接状态
    m_currentDeviceId = deviceId;
    m_connected = true;
    
    // 尝试发送一个简单的UPnP动作来测试连接
    int retryCount = 0;
    const int maxRetries = 3;
    bool success = false;
    
    while (retryCount < maxRetries && !success) {
        if (retryCount > 0) {
            qDebug() << "重试连接 (" << retryCount + 1 << "/" << maxRetries << ")";
            QThread::msleep(1000); // 重试前等待1秒
        }
        
        QMap<QString, QString> args;
        args["InstanceID"] = "0";
        success = sendUPnPAction(UPnP_AVTransport, "GetTransportInfo", args);
        
        if (!success) {
            qDebug() << "连接测试失败，尝试重新连接...";
            retryCount++;
        }
    }
    
    if (success) {
        emit connectionStateChanged(true);
        qDebug() << "设备连接成功:" << device.friendlyName;
        startPlaybackMonitoring();
        return true;
    }
    
    // 如果所有重试都失败，恢复未连接状态
    m_connected = false;
    m_currentDeviceId.clear();
    qDebug() << "设备连接失败:" << deviceId;
    emit error(tr("Failed to connect to device after %1 attempts").arg(maxRetries));
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
    addDevice(device);
}

void DLNAManager::addDevice(const DLNADevice& device)
{
    QString deviceId = device.UDN;
    bool isNew = !m_devices.contains(deviceId);
    
    m_devices[deviceId] = device;
    
    if (isNew) {
        qDebug() << "\n处理DLNA设备发现:";
        qDebug() << "  设��ID:" << deviceId;
        qDebug() << "  设备名称:" << device.friendlyName;
        emit deviceDiscovered(deviceId, device.friendlyName);
    }

    // 打印当前设备列表
    qDebug() << "\n更新DLNA设备列表:";
    qDebug() << "当前设备数量:" << m_devices.size();
    for (const auto& dev : m_devices) {
        qDebug() << "  添加设备到列表:";
        qDebug() << "    ID:" << dev.UDN;
        qDebug() << "    名称:" << dev.friendlyName;
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
        
        qDebug() << "\n更新DLNA设备列表:";
        qDebug() << "当前设备数量:" << m_devices.size();
        for (const auto& dev : m_devices) {
            qDebug() << "  设备列表:";
            qDebug() << "    ID:" << dev.UDN;
            qDebug() << "    名称:" << dev.friendlyName;
        }
    }
}

void DLNAManager::clearDevices()
{
    qDebug() << "清空DLNA设备列表";
    if (m_connected) {
        disconnectFromDevice();
    }
    m_devices.clear();
    qDebug() << "\n更新DLNA设备列表:";
    qDebug() << "���前设���数量:" << m_devices.size();
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
    
    // 获取传输信息
    if (sendUPnPAction(UPnP_AVTransport, "GetTransportInfo", args)) {
        // 处理响应...
    }
    
    // 获取位置信息
    if (sendUPnPAction(UPnP_AVTransport, "GetPositionInfo", args)) {
        // 处理响应...
    }
}

void DLNAManager::handleUPnPResponse(QNetworkReply* reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "UPnP请求失败:" << reply->errorString();
        return;
    }

    QByteArray data = reply->readAll();
    // 解析响应...
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

bool DLNAManager::sendUPnPAction(const QString& serviceType, const QString& action, const QMap<QString, QString>& arguments)
{
    if (!m_connected || !m_devices.contains(m_currentDeviceId)) {
        return false;
    }

    const DLNADevice& device = m_devices[m_currentDeviceId];
    if (!device.hasService(serviceType)) {
        qDebug() << "服务未找到:" << serviceType;
        return false;
    }

    DLNAService service = device.getService(serviceType);
    
    // 构建SOAP请求
    QString soapBody = QString(
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
        "<s:Body>\n"
        "<u:%1 xmlns:u=\"%2\">\n")
        .arg(action)
        .arg(serviceType);

    // 添加参数
    for (auto it = arguments.constBegin(); it != arguments.constEnd(); ++it) {
        soapBody += QString("<%1>%2</%1>\n")
            .arg(it.key())
            .arg(it.value().toHtmlEscaped());
    }

    soapBody += QString("</u:%1>\n"
        "</s:Body>\n"
        "</s:Envelope>\n")
        .arg(action);

    // 创建HTTP请求
    QUrl controlUrl(service.controlURL);
    if (controlUrl.isRelative()) {
        controlUrl = QUrl(device.getBaseUrl()).resolved(controlUrl);
    }

    QNetworkRequest request(controlUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml; charset=\"utf-8\"");
    request.setRawHeader("SOAPAction", QString("\"%1#%2\"").arg(serviceType).arg(action).toUtf8());

    // 发送请求
    QNetworkReply* reply = m_networkManager->post(request, soapBody.toUtf8());
    
    // 等待响应
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    
    timer.start(5000); // 5秒超时
    loop.exec();
    
    if (timer.isActive()) {
        timer.stop();
        if (reply->error() == QNetworkReply::NoError) {
            reply->deleteLater();
            return true;
        }
        qDebug() << "UPnP动作失败:" << action << ", 错误:" << reply->errorString();
    } else {
        qDebug() << "UPnP动作超时:" << action;
        reply->abort();
    }
    
    reply->deleteLater();
    return false;
}

// 媒体控制方法实现
bool DLNAManager::playMedia(const QUrl& url)
{
    qDebug() << "\n开始播放媒体:" << url.toString();
    
    if (!m_connected) {
        qDebug() << "未连接到设备";
        emit error(tr("Not connected to any device"));
        return false;
    }

    // 设置 AVTransport URI
    QMap<QString, QString> setUriArgs;
    setUriArgs["InstanceID"] = "0";
    setUriArgs["CurrentURI"] = url.toString();
    setUriArgs["CurrentURIMetaData"] = "";

    if (!sendUPnPAction(UPnP_AVTransport, "SetAVTransportURI", setUriArgs)) {
        qDebug() << "设置媒体URI失败";
        emit error(tr("Failed to set media URI"));
        return false;
    }

    // 等待一段时间让设备处理URI
    QThread::msleep(500);

    // 开始播放
    QMap<QString, QString> playArgs;
    playArgs["InstanceID"] = "0";
    playArgs["Speed"] = "1";

    if (!sendUPnPAction(UPnP_AVTransport, "Play", playArgs)) {
        qDebug() << "开始播放失败";
        emit error(tr("Failed to start playback"));
        return false;
    }

    qDebug() << "媒体播放已开始";
    return true;
}

bool DLNAManager::pauseMedia()
{
    if (!m_connected) {
        emit error(tr("Not connected to any device"));
        return false;
    }

    QMap<QString, QString> args;
    args["InstanceID"] = "0";
    return sendUPnPAction(UPnP_AVTransport, "Pause", args);
}

bool DLNAManager::stopMedia()
{
    if (!m_connected) {
        emit error(tr("Not connected to any device"));
        return false;
    }

    QMap<QString, QString> args;
    args["InstanceID"] = "0";
    return sendUPnPAction(UPnP_AVTransport, "Stop", args);
}

bool DLNAManager::setVolume(int volume)
{
    if (!m_connected) {
        emit error(tr("Not connected to any device"));
        return false;
    }

    QMap<QString, QString> args;
    args["InstanceID"] = "0";
    args["Channel"] = "Master";
    args["DesiredVolume"] = QString::number(volume);
    return sendUPnPAction(UPnP_RenderingControl, "SetVolume", args);
}

bool DLNAManager::seekTo(qint64 position)
{
    if (!m_connected) {
        emit error(tr("Not connected to any device"));
        return false;
    }

    // 将毫秒转换为时:分:秒.毫秒格式
    int hours = position / 3600000;
    int minutes = (position % 3600000) / 60000;
    int seconds = (position % 60000) / 1000;
    int milliseconds = position % 1000;

    QString target = QString("%1:%2:%3.%4")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(milliseconds, 3, 10, QChar('0'));

    QMap<QString, QString> args;
    args["InstanceID"] = "0";
    args["Unit"] = "REL_TIME";
    args["Target"] = target;
    return sendUPnPAction(UPnP_AVTransport, "Seek", args);
}

void DLNAManager::checkDeviceTimeouts()
{
    // 不再使用超时检查逻辑
}

void DLNAManager::onLocalPlaybackStateChanged(const QString& state)
{
    m_localPlaybackState = state;
}

void DLNAManager::onLocalPositionChanged(qint64 position)
{
    m_localPosition = position;
}

void DLNAManager::onLocalDurationChanged(qint64 duration)
{
    m_localDuration = duration;
}

void DLNAManager::onLocalVolumeChanged(int volume)
{
    m_localVolume = volume;
} 
