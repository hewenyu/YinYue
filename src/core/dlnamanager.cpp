#include "dlnamanager.h"
#include <QNetworkReply>
#include <QDateTime>
#include <QXmlStreamReader>
#include <QDebug>
#include <QEventLoop>

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
    // 打印发现的设备列表
    QList<DLNAManager::DLNADevice> devices = m_devices.values();
    qDebug() << "发现的设备数量:" << devices.size();
    for (const auto& device : devices) {
        qDebug() << "设备:" << device;
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
    qDebug() << "正在连接到设备:" << device.name;
    
    // 验证设备是否有有效的控制URL
    if (device.location.isEmpty()) {
        qDebug() << "设备控制URL为空";
        emit error(tr("Invalid device control URL"));
        return false;
    }
    
    // 尝试发送一个简单的UPnP动作来测试连接
    QMap<QString, QString> args;
    bool success = sendUPnPAction("urn:schemas-upnp-org:service:AVTransport:1", "GetTransportInfo", args);
    
    if (success) {
        m_currentDeviceId = deviceId;
        m_connected = true;
        emit connectionStateChanged(true);
        qDebug() << "设备连接成功:" << device.name;
        return true;
    } else {
        qDebug() << "设备连接失败:" << device.name;
        emit error(tr("Failed to connect to device"));
        return false;
    }
}

void DLNAManager::disconnectFromDevice()
{
    if (m_connected) {
        qDebug() << "正在断开设备连接:" << m_currentDeviceId;
        
        // 停止当前播放
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
    // 发送通用 MediaRenderer 搜索
    QByteArray ssdpRequest = 
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 3\r\n"
        "ST: urn:schemas-upnp-org:device:MediaRenderer:1\r\n"
        "\r\n";

    qDebug() << "发送 SSDP 发现请求 (MediaRenderer):" << QString::fromUtf8(ssdpRequest);
    m_ssdpSocket->writeDatagram(ssdpRequest, SSDP_MULTICAST_ADDR, SSDP_PORT);

    // 发送特定于小爱音箱的搜索
    ssdpRequest = 
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 3\r\n"
        "ST: urn:schemas-upnp-org:device:Basic:1\r\n"
        "\r\n";

    qDebug() << "发送 SSDP 发现请求 (Basic):" << QString::fromUtf8(ssdpRequest);
    m_ssdpSocket->writeDatagram(ssdpRequest, SSDP_MULTICAST_ADDR, SSDP_PORT);

    // 发送 AVTransport 服务搜索
    ssdpRequest = 
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 3\r\n"
        "ST: urn:schemas-upnp-org:service:AVTransport:1\r\n"
        "\r\n";

    qDebug() << "发送 SSDP 发现请求 (AVTransport):" << QString::fromUtf8(ssdpRequest);
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
        
        // 解析SSDP响应
        QString response = QString::fromUtf8(datagram);
        QStringList lines = response.split("\r\n");
        
        QString location;
        QString usn;
        QString friendlyName = "Unknown Device";
        QString deviceType;
        
        qDebug() << "收到SSDP响应:";
        qDebug() << response;
        
        for (const QString& line : lines) {
            if (line.startsWith("LOCATION:", Qt::CaseInsensitive)) {
                location = line.mid(9).trimmed();
                qDebug() << "发现设备位置:" << location;
            }
            else if (line.startsWith("USN:", Qt::CaseInsensitive)) {
                usn = line.mid(4).trimmed();
                qDebug() << "发现设备USN:" << usn;
            }
            else if (line.startsWith("NT:", Qt::CaseInsensitive) || line.startsWith("ST:", Qt::CaseInsensitive)) {
                deviceType = line.mid(3).trimmed();
                qDebug() << "发现设备类型:" << deviceType;
            }
        }
        
        if (!location.isEmpty() && !usn.isEmpty()) {
            QString deviceId = usn;
            if (deviceId.contains("::")) {
                deviceId = deviceId.split("::").first();
            }
            
            qDebug() << "处理设备:" << deviceId;
            qDebug() << "设备地址:" << sender.toString() << ":" << senderPort;
            
            DLNADevice device(deviceId, friendlyName, location);
            device.address = sender;
            device.port = senderPort;
            device.type = deviceType;
            
            // 添加或更新设备
            addDevice(device);
            m_deviceTimeouts[deviceId] = QDateTime::currentDateTime();
            
            // 获取设备描述
            fetchDeviceDescription(location);
        }
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
    QString deviceId;
    QString friendlyName;
    QString deviceType;
    QString controlURL;
    QString baseURL;

    while (!xml.atEnd() && !xml.hasError()) {
        if (xml.readNextStartElement()) {
            if (xml.name() == "URLBase") {
                baseURL = xml.readElementText();
                if (!baseURL.endsWith('/')) {
                    baseURL += '/';
                }
            } else if (xml.name() == "device") {
                // 解析设备信息
                while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "device")) {
                    if (xml.tokenType() == QXmlStreamReader::StartElement) {
                        if (xml.name() == "UDN") {
                            deviceId = xml.readElementText();
                        } else if (xml.name() == "friendlyName") {
                            friendlyName = xml.readElementText();
                        } else if (xml.name() == "deviceType") {
                            deviceType = xml.readElementText();
                        } else if (xml.name() == "serviceList") {
                            // ���析服务列表
                            while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "serviceList")) {
                                if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "service") {
                                    QString serviceType;
                                    QString serviceId;
                                    QString scpdUrl;
                                    QString control;
                                    QString eventSub;
                                    
                                    // 解析服务信息
                                    while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "service")) {
                                        if (xml.tokenType() == QXmlStreamReader::StartElement) {
                                            if (xml.name() == "serviceType") {
                                                serviceType = xml.readElementText();
                                            } else if (xml.name() == "serviceId") {
                                                serviceId = xml.readElementText();
                                            } else if (xml.name() == "SCPDURL") {
                                                scpdUrl = xml.readElementText();
                                            } else if (xml.name() == "controlURL") {
                                                control = xml.readElementText();
                                                qDebug() << "找到控制URL:" << control << "服务类型:" << serviceType;
                                            } else if (xml.name() == "eventSubURL") {
                                                eventSub = xml.readElementText();
                                            }
                                        }
                                        xml.readNext();
                                    }
                                    
                                    // 保存 AVTransport 服务的控制 URL
                                    if (serviceType.contains("AVTransport")) {
                                        controlURL = control;
                                        qDebug() << "找到 AVTransport 服务控制 URL:" << controlURL;
                                    }
                                }
                                xml.readNext();
                            }
                        }
                    }
                    xml.readNext();
                }
            }
        }
    }

    if (!deviceId.isEmpty() && !friendlyName.isEmpty()) {
        // 构建完整的控制URL
        QString location;
        if (controlURL.startsWith("http://") || controlURL.startsWith("https://")) {
            location = controlURL;
        } else {
            if (baseURL.isEmpty()) {
                QUrl deviceUrl(m_devices[deviceId].location);
                baseURL = deviceUrl.scheme() + "://" + deviceUrl.host() + ":" + QString::number(deviceUrl.port(80)) + "/";
            }
            location = baseURL + (controlURL.startsWith('/') ? controlURL.mid(1) : controlURL);
        }
        
        qDebug() << "设备信息:" << deviceId << friendlyName << location;
        DLNADevice device(deviceId, friendlyName, location);
        device.type = deviceType;
        addDevice(device);
        qDebug() << "设备描述已更新:" << device;
    } else {
        qDebug() << "设备描述解析失败: deviceId=" << deviceId << "friendlyName=" << friendlyName;
    }
}

void DLNAManager::checkDeviceTimeouts()
{
    QDateTime now = QDateTime::currentDateTime();
    QList<QString> expiredDevices;

    for (auto it = m_deviceTimeouts.begin(); it != m_deviceTimeouts.end(); ++it) {
        if (it.value().msecsTo(now) > DEVICE_TIMEOUT) {
            qDebug() << "设备超时:" << it.key();
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
    qDebug() << "添加/更新设备:" << device.id;
    if (!m_devices.contains(device.id)) {
        m_devices[device.id] = device;
        emit deviceDiscovered(device.id, device.name);
        qDebug() << "新设备已发现:" << device.name;
    } else {
        m_devices[device.id] = device;
    }
}

void DLNAManager::removeDevice(const QString& deviceId)
{
    qDebug() << "移除设备:" << deviceId;
    if (m_devices.remove(deviceId) > 0) {
        emit deviceLost(deviceId);
        qDebug() << "设备已移除:" << deviceId;
        
        if (m_currentDeviceId == deviceId) {
            qDebug() << "当前连接的设备已移除，断开连接";
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
    if (!m_connected) {
        qDebug() << "未连接到设备，无法播放媒体";
        return false;
    }
    
    qDebug() << "正在播放媒体:" << url.toString();
    QMap<QString, QString> args;
    args["CurrentURI"] = url.toString();
    args["CurrentURIMetaData"] = "";
    
    if (sendUPnPAction("urn:schemas-upnp-org:service:AVTransport:1", "SetAVTransportURI", args)) {
        bool success = sendUPnPAction("urn:schemas-upnp-org:service:AVTransport:1", "Play", {{"Speed", "1"}});
        if (success) {
            emit playbackStateChanged("PLAYING");
        }
        return success;
    }
    return false;
}

bool DLNAManager::pauseMedia()
{
    if (!m_connected) {
        qDebug() << "未连接到设备，无法暂停媒体";
        return false;
    }
    
    qDebug() << "正在暂停媒体";
    bool success = sendUPnPAction("urn:schemas-upnp-org:service:AVTransport:1", "Pause", {});
    if (success) {
        emit playbackStateChanged("PAUSED");
    }
    return success;
}

bool DLNAManager::stopMedia()
{
    if (!m_connected) {
        qDebug() << "未连接到设备，无法停止媒体";
        return false;
    }
    
    qDebug() << "正在停止媒体";
    bool success = sendUPnPAction("urn:schemas-upnp-org:service:AVTransport:1", "Stop", {});
    if (success) {
        emit playbackStateChanged("STOPPED");
    }
    return success;
}

bool DLNAManager::setVolume(int volume)
{
    if (!m_connected) {
        qDebug() << "未连接到设备，无法设置音量";
        return false;
    }
    
    qDebug() << "正在设置音量:" << volume;
    QMap<QString, QString> args;
    args["DesiredVolume"] = QString::number(qBound(0, volume, 100));
    return sendUPnPAction("urn:schemas-upnp-org:service:RenderingControl:1", "SetVolume", args);
}

bool DLNAManager::seekTo(qint64 position)
{
    if (!m_connected) {
        qDebug() << "未连接到设备，无法定位";
        return false;
    }
    
    qDebug() << "正在定位到:" << position;
    QMap<QString, QString> args;
    args["Target"] = QString::number(position);
    return sendUPnPAction("urn:schemas-upnp-org:service:AVTransport:1", "Seek", args);
}

bool DLNAManager::sendUPnPAction(const QString& serviceType, const QString& actionName, const QMap<QString, QString>& arguments)
{
    if (!m_connected || !m_devices.contains(m_currentDeviceId)) {
        qDebug() << "未连接到设备或设备无效";
        return false;
    }

    const DLNADevice& device = m_devices[m_currentDeviceId];
    if (device.location.isEmpty()) {
        qDebug() << "设备位置URL为空";
        return false;
    }
    
    // 构建完���的控制URL
    QUrl baseUrl(device.location);
    QString controlPath = device.location;
    QUrl controlUrl;
    
    // 如果控制路径是相对路径，需要与基础URL组合
    if (!controlPath.startsWith("http://") && !controlPath.startsWith("https://")) {
        QString basePath = baseUrl.path();
        if (!basePath.endsWith('/')) {
            basePath += '/';
        }
        if (controlPath.startsWith('/')) {
            controlPath = controlPath.mid(1);
        }
        controlUrl = QUrl(baseUrl.scheme() + "://" + baseUrl.host() + ":" + 
                         QString::number(baseUrl.port(80)) + "/" + controlPath);
    } else {
        controlUrl = QUrl(controlPath);
    }
    
    qDebug() << "发送UPnP动作到:" << controlUrl.toString();
    
    // 构建SOAP消息
    QString soapBody = QString(
        "<?xml version=\"1.0\"?>\n"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
        "<s:Body>\n"
        "<u:%1 xmlns:u=\"%2\">\n"
        "<InstanceID>0</InstanceID>\n").arg(actionName, serviceType);
    
    for (auto it = arguments.constBegin(); it != arguments.constEnd(); ++it) {
        soapBody += QString("<%1>%2</%1>\n").arg(it.key(), it.value());
    }
    
    soapBody += QString("</u:%1>\n"
                       "</s:Body>\n"
                       "</s:Envelope>\n").arg(actionName);

    QNetworkRequest request(controlUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml; charset=\"utf-8\"");
    request.setRawHeader("SOAPAction", QString("\"%1#%2\"").arg(serviceType, actionName).toUtf8());

    qDebug() << "发送SOAP消息:" << soapBody;
    QNetworkReply* reply = m_networkManager->post(request, soapBody.toUtf8());
    
    // 等待响应
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    bool success = (reply->error() == QNetworkReply::NoError);
    if (!success) {
        qDebug() << "UPnP动作失败:" << reply->errorString();
        emit error(reply->errorString());
    } else {
        qDebug() << "UPnP动作成功:" << actionName;
        QByteArray response = reply->readAll();
        qDebug() << "响应内容:" << response;
    }

    reply->deleteLater();
    return success;
}

void DLNAManager::handleUPnPResponse(QNetworkReply* reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "UPnP响应错误:" << reply->errorString();
        emit error(reply->errorString());
    } else {
        QByteArray data = reply->readAll();
        qDebug() << "收到UPnP响应:" << data;
        // 解析响应并更新状态
    }
    reply->deleteLater();
}

QDebug operator<<(QDebug debug, const DLNAManager::DLNADevice& device)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "DLNADevice(id: " << device.id
                    << ", name: " << device.name
                    << ", location: " << device.location << ")";
    return debug;
} 
