#include "dlnamanager.h"
#include <QNetworkReply>
#include <QDateTime>
#include <QXmlStreamReader>
#include <QDebug>
#include <QEventLoop>

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
    
    // 验证设备是否有有��的控制URL
    if (device.location.isEmpty()) {
        qDebug() << "设备控制URL为空";
        emit error(tr("Invalid device control URL"));
        return false;
    }
    
    // 设置当前设备ID（在发送UPnP动作之前）
    m_currentDeviceId = deviceId;
    
    // 尝试发送一个简单的UPnP动作来测试连接
    QMap<QString, QString> args;
    args["InstanceID"] = "0";
    
    bool success = sendUPnPAction("urn:schemas-upnp-org:service:AVTransport:1", 
                                "GetTransportInfo", args);
    
    if (success) {
        m_connected = true;
        emit connectionStateChanged(true);
        qDebug() << "设备连接成功:" << device.name;
        
        // 启动状态监控
        startPlaybackMonitoring();
        
        return true;
    }
    
    // 如果连接失败，清除当前设备ID
    m_currentDeviceId.clear();
    qDebug() << "设备连接失败:" << deviceId;
    emit error(tr("Failed to connect to device"));
    return false;
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
        QHostAddress senderAddress;
        quint16 senderPort;

        m_ssdpSocket->readDatagram(datagram.data(), datagram.size(), &senderAddress, &senderPort);
        QString response = QString::fromUtf8(datagram);
        qDebug() << "收到SSDP响应:\n" << response;

        // 解析设备信息
        QString location = extractHeader(response, "LOCATION");
        QString st = extractHeader(response, "ST");
        QString usn = extractHeader(response, "USN");

        if (!location.isEmpty()) {
            qDebug() << "发现设备位置:" << location;
            qDebug() << "发现设备类型:" << st;
            qDebug() << "发现设备USN:" << usn;

            // 提取设备ID
            QString deviceId;
            if (usn.contains("::")) {
                deviceId = usn.split("::").first();
            } else {
                deviceId = usn;
            }
            qDebug() << "处理设备:" << deviceId;

            // 记录设备地址和端口
            qDebug() << "设备地址:" << senderAddress.toString() << ":" << senderPort;

            // 检查是否已存在相同ID的设备
            if (m_devices.contains(deviceId)) {
                // 更新现有设备的信息，但保留名称等已知信息
                DLNADevice& existingDevice = m_devices[deviceId];
                existingDevice.address = senderAddress;
                existingDevice.port = senderPort;
                existingDevice.lastSeen = QDateTime::currentDateTime();
                if (location != existingDevice.location) {
                    existingDevice.location = location;
                    fetchDeviceDescription(location);  // 获取更新的设备描述
                }
            } else {
                // 创建新设备
                DLNADevice newDevice;
                newDevice.id = deviceId;
                newDevice.name = "Unknown Device";  // 临时名称
                newDevice.location = location;
                newDevice.type = st;
                newDevice.address = senderAddress;
                newDevice.port = senderPort;
                newDevice.lastSeen = QDateTime::currentDateTime();
                
                addDevice(newDevice);
                fetchDeviceDescription(location);  // 获取设备描述
            }
        }
    }
}

void DLNAManager::fetchDeviceDescription(const QString& location)
{
    QUrl url(location);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "YinYue/1.0");
    
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, location]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            parseDeviceDescription(data);
        } else {
            qDebug() << "获取设备描述失败:" << reply->errorString();
        }
        reply->deleteLater();
    });
}

void DLNAManager::parseDeviceDescription(const QByteArray& data)
{
    QXmlStreamReader xml(data);
    QString currentDeviceId;
    QString deviceName;
    QString controlURL;
    QString serviceType;
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name() == "device") {
                // 重置设备信息
                currentDeviceId.clear();
                deviceName.clear();
            }
            else if (xml.name() == "UDN") {
                currentDeviceId = xml.readElementText();
            }
            else if (xml.name() == "friendlyName") {
                deviceName = xml.readElementText();
                if (!currentDeviceId.isEmpty() && m_devices.contains(currentDeviceId)) {
                    DLNADevice& device = m_devices[currentDeviceId];
                    device.name = deviceName;
                    qDebug() << "设备名称已更新:" << deviceName;
                    emit deviceDiscovered(device.id, device.name);
                }
            }
            else if (xml.name() == "serviceType") {
                serviceType = xml.readElementText();
            }
            else if (xml.name() == "controlURL") {
                controlURL = xml.readElementText();
                qDebug() << "找到控制URL:" << controlURL << "服务类型:" << serviceType;
                
                if (serviceType.contains("AVTransport")) {
                    qDebug() << "找到 AVTransport 服务控制 URL:" << controlURL;
                    if (!currentDeviceId.isEmpty() && m_devices.contains(currentDeviceId)) {
                        DLNADevice& device = m_devices[currentDeviceId];
                        // 更新设备的控制URL
                        if (!controlURL.startsWith("http")) {
                            QUrl baseUrl(device.location);
                            QString basePath = baseUrl.path();
                            basePath = basePath.left(basePath.lastIndexOf('/') + 1);
                            controlURL = QString("%1://%2:%3%4%5")
                                .arg(baseUrl.scheme())
                                .arg(baseUrl.host())
                                .arg(baseUrl.port())
                                .arg(basePath)
                                .arg(controlURL);
                        }
                        device.location = controlURL;
                        qDebug() << "设备信息:" << device.id << device.name << device.location;
                        emit deviceDiscovered(device.id, device.name);
                    }
                }
            }
        }
    }
    
    if (xml.hasError()) {
        qDebug() << "XML解析错误:" << xml.errorString();
    }
}

QString DLNAManager::extractHeader(const QString& response, const QString& header)
{
    QRegExp rx(header + ": *([^\r\n]+)", Qt::CaseInsensitive);
    if (rx.indexIn(response) != -1) {
        return rx.cap(1);
    }
    return QString();
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
    args["InstanceID"] = "0";
    args["CurrentURI"] = url.toString();
    args["CurrentURIMetaData"] = "";
    
    // 设置媒体
    bool success = sendUPnPAction("urn:schemas-upnp-org:service:AVTransport:1", "SetAVTransportURI", args);
    if (!success) {
        return false;
    }
    
    // 等待媒体加载
    QThread::msleep(500);
    
    // 开始播放
    args.clear();
    args["InstanceID"] = "0";
    args["Speed"] = "1";
    success = sendUPnPAction("urn:schemas-upnp-org:service:AVTransport:1", "Play", args);
    
    if (success) {
        // 启动状态监控
        startPlaybackMonitoring();
        emit playbackStateChanged("PLAYING");
    }
    
    return success;
}

void DLNAManager::startPlaybackMonitoring()
{
    if (!m_monitoringTimer) {
        m_monitoringTimer = new QTimer(this);
        m_monitoringTimer->setInterval(500);  // 减少检查间隔到500ms
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
        stopPlaybackMonitoring();
        return;
    }

    QMap<QString, QString> args;
    args["InstanceID"] = "0";
    
    bool success = sendUPnPAction("urn:schemas-upnp-org:service:AVTransport:1", "GetTransportInfo", args);
    if (success) {
        // 解析响应并更新状态
        QString currentState = m_lastResponse.value("CurrentTransportState");
        if (!currentState.isEmpty() && currentState != m_currentPlaybackState) {
            m_currentPlaybackState = currentState;
            emit playbackStateChanged(currentState);
            qDebug() << "播放状态已更新:" << currentState;
        }
    } else {
        qDebug() << "获取播放状态失败";
        // 如果获取状态失败，可能是连接断开
        if (m_connected) {
            m_connected = false;
            emit connectionStateChanged(false);
            qDebug() << "连接已断开";
        }
    }
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
    if (!m_devices.contains(m_currentDeviceId)) {
        qDebug() << "设备无效";
        return false;
    }

    const DLNADevice& device = m_devices[m_currentDeviceId];
    if (device.location.isEmpty()) {
        qDebug() << "设备位置URL为空";
        return false;
    }
    
    // 构建控制URL
    QUrl controlUrl(device.location);
    QString controlPath = controlUrl.path();
    
    // 确保控制路径包含 "control"
    if (!controlPath.contains("control", Qt::CaseInsensitive)) {
        if (!controlPath.endsWith('/')) {
            controlPath += '/';
        }
        controlPath += "control";
        controlUrl.setPath(controlPath);
    }
    
    qDebug() << "发送UPnP动作到:" << controlUrl.toString();
    
    // 构建SOAP消息
    QString soapBody = QString(
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
        "<s:Body>\n"
        "<u:%1 xmlns:u=\"%2\">\n").arg(actionName, serviceType);
    
    // 添加参数
    for (auto it = arguments.constBegin(); it != arguments.constEnd(); ++it) {
        soapBody += QString("<%1>%2</%1>\n").arg(it.key(), it.value());
    }
    
    soapBody += QString("</u:%1>\n"
                       "</s:Body>\n"
                       "</s:Envelope>\n").arg(actionName);

    QNetworkRequest request(controlUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml; charset=\"utf-8\"");
    request.setRawHeader("SOAPAction", QString("\"%1#%2\"").arg(serviceType, actionName).toUtf8());
    request.setRawHeader("Connection", "keep-alive");  // 修改为 keep-alive
    request.setRawHeader("Content-Length", QString::number(soapBody.toUtf8().length()).toUtf8());
    request.setRawHeader("User-Agent", "YinYue/1.0");  // 添加 User-Agent

    qDebug() << "发送SOAP消息:" << soapBody;
    QNetworkReply* reply = m_networkManager->post(request, soapBody.toUtf8());
    
    // 等待响应
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(5000);  // 5秒超时
    loop.exec();

    if (timer.isActive()) {
        timer.stop();
        bool success = (reply->error() == QNetworkReply::NoError);
        if (!success) {
            qDebug() << "UPnP动作失败:" << reply->errorString() 
                     << "\nHTTP状态码:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
                     << "\n错误类型:" << reply->error();
            emit error(reply->errorString());
        } else {
            qDebug() << "UPnP动作成功:" << actionName;
            QByteArray response = reply->readAll();
            qDebug() << "响应内容:" << response;
            
            // 解析响应
            if (response.contains("GetTransportInfoResponse")) {
                // 解析播放状态
                QString currentState;
                if (response.contains("<CurrentTransportState>")) {
                    int start = response.indexOf("<CurrentTransportState>") + 21;
                    int end = response.indexOf("</CurrentTransportState>");
                    if (start > 0 && end > start) {
                        currentState = response.mid(start, end - start);
                        m_lastResponse["CurrentTransportState"] = currentState;
                        emit playbackStateChanged(currentState);
                    }
                }
            }
            success = true;
        }
        reply->deleteLater();
        return success;
    } else {
        qDebug() << "UPnP动作超时";
        reply->abort();
        reply->deleteLater();
        emit error("Connection timeout");
        return false;
    }
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
