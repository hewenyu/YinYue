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
    
    // 验证设备是否支持AVTransport服务
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
        qDebug() << "  设备ID:" << deviceId;
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
    qDebug() << "当前设备数量:" << m_devices.size();
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

void DLNAManager::handleUPnPResponse(QNetworkReply* reply)
{
    if (!reply) {
        qDebug() << "错误: 收到空的网络响应";
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "\nUPnP响应错误:";
        qDebug() << "  错误类型:" << reply->error();
        qDebug() << "  错误描述:" << reply->errorString();
        qDebug() << "  HTTP状态码:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug() << "  请求URL:" << reply->url().toString();
        
        QByteArray errorData = reply->readAll();
        if (!errorData.isEmpty()) {
            qDebug() << "  错误响应内容:" << QString::fromUtf8(errorData);
        }
        
        reply->deleteLater();
        return;
    }

    QByteArray response = reply->readAll();
    if (response.isEmpty()) {
        qDebug() << "警告: 收到空的响应内容";
        reply->deleteLater();
        return;
    }

    QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    qDebug() << "\nUPnP响应:";
    qDebug() << "  内容类型:" << contentType;
    qDebug() << "  响应内容:" << QString::fromUtf8(response);

    // 解析SOAP响应
    if (contentType.contains("xml", Qt::CaseInsensitive) && response.contains("<?xml")) {
        QDomDocument doc;
        QString errorMsg;
        int errorLine = 0, errorColumn = 0;
        
        if (!doc.setContent(response, &errorMsg, &errorLine, &errorColumn)) {
            qDebug() << "XML解析错误:";
            qDebug() << "  错误信息:" << errorMsg;
            qDebug() << "  错误位置:" << errorLine << "行" << errorColumn << "列";
            reply->deleteLater();
            return;
        }

        QDomElement envelope = doc.documentElement();
        if (envelope.isNull()) {
            qDebug() << "错误: XML根元素为空";
            reply->deleteLater();
            return;
        }

        QDomElement body = envelope.firstChildElement("s:Body");
        if (body.isNull()) {
            body = envelope.firstChildElement("Body");
        }

        if (!body.isNull()) {
            // 检查是否有错误响应
            QDomElement fault = body.firstChildElement("s:Fault");
            if (fault.isNull()) {
                fault = body.firstChildElement("Fault");
            }

            if (!fault.isNull()) {
                QString faultString;
                QDomElement faultStringElem = fault.firstChildElement("faultstring");
                if (!faultStringElem.isNull()) {
                    faultString = faultStringElem.text();
                } else {
                    faultString = fault.text();
                }
                qDebug() << "  SOAP错误:" << faultString;
                emit error(tr("Device error: %1").arg(faultString));
            } else {
                // 处理成功响应
                // 查找任何可能的响应元素
                QDomElement responseElem = body.firstChildElement();
                while (!responseElem.isNull()) {
                    QString tagName = responseElem.tagName();
                    if (tagName.contains("Response", Qt::CaseInsensitive)) {
                        qDebug() << "  找到响应元素:" << tagName;
                        // ��理所有子元素
                        QDomElement child = responseElem.firstChildElement();
                        while (!child.isNull()) {
                            QString childName = child.tagName();
                            QString childValue = child.text();
                            qDebug() << "    " << childName << ":" << childValue;
                            
                            // 特别处理一些常见的状态
                            if (childName == "CurrentTransportState") {
                                emit playbackStateChanged(childValue);
                            }
                            child = child.nextSiblingElement();
                        }
                        break;
                    }
                    responseElem = responseElem.nextSiblingElement();
                }
            }
        } else {
            qDebug() << "警告: 未找到SOAP Body元素";
        }
    } else {
        qDebug() << "警告: 响应不是有效的XML格式";
    }

    reply->deleteLater();
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
        qDebug() << "发送UPnP动作失败: 设备未连接";
        return false;
    }

    const DLNADevice& device = m_devices[m_currentDeviceId];
    if (!device.hasService(serviceType)) {
        qDebug() << "发送UPnP动作失败: 设备不支持服务" << serviceType;
        return false;
    }

    DLNAService service = device.getService(serviceType);
    QString controlUrl = device.getFullUrl(service.controlURL);
    
    qDebug() << "\n发送UPnP动作:";
    qDebug() << "  动作:" << action;
    qDebug() << "  服务:" << serviceType;
    qDebug() << "  控制URL:" << controlUrl;
    
    // 构建SOAP请求
    QString soapBody = QString(
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
        "<s:Body>\n"
        "<u:%1 xmlns:u=\"%2\">\n").arg(action, serviceType);
    
    for (auto it = arguments.begin(); it != arguments.end(); ++it) {
        soapBody += QString("<%1>%2</%1>\n").arg(it.key(), it.value());
        qDebug() << "  参数:" << it.key() << "=" << it.value();
    }
    
    soapBody += QString("</u:%1>\n"
                       "</s:Body>\n"
                       "</s:Envelope>\n").arg(action);

    QNetworkRequest request(controlUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml; charset=\"utf-8\"");
    request.setHeader(QNetworkRequest::UserAgentHeader, "YinYue/1.0");
    request.setRawHeader("SOAPAction", QString("\"%1#%2\"").arg(serviceType, action).toUtf8());
    request.setRawHeader("Connection", "close");
    request.setRawHeader("Cache-Control", "no-cache");
    request.setRawHeader("Pragma", "no-cache");
    request.setRawHeader("Accept", "text/xml");
    request.setRawHeader("Accept-Encoding", "identity");
    request.setRawHeader("Content-Length", QByteArray::number(soapBody.toUtf8().length()));

    qDebug() << "  请求头:";
    qDebug() << "    Content-Type:" << request.header(QNetworkRequest::ContentTypeHeader).toString();
    qDebug() << "    SOAPAction:" << request.rawHeader("SOAPAction");
    qDebug() << "    Content-Length:" << request.rawHeader("Content-Length");
    qDebug() << "  请求体:";
    qDebug() << soapBody;

    QNetworkReply* reply = m_networkManager->post(request, soapBody.toUtf8());
    
    // 等待响应
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    bool success = (reply->error() == QNetworkReply::NoError);
    if (!success) {
        qDebug() << "UPnP动作失败:";
        qDebug() << "  错误:" << reply->errorString();
        qDebug() << "  状态���:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray responseData = reply->readAll();
        qDebug() << "  响应:" << QString::fromUtf8(responseData);
        
        // 尝试解析错误响应
        if (responseData.contains("<?xml")) {
            QDomDocument doc;
            if (doc.setContent(responseData)) {
                QDomElement fault = doc.documentElement().firstChildElement("s:Body").firstChildElement("s:Fault");
                if (!fault.isNull()) {
                    QString faultString = fault.firstChildElement("faultstring").text();
                    qDebug() << "  SOAP错误:" << faultString;
                }
            }
        }
    } else {
        qDebug() << "UPnP动作成功";
        QByteArray responseData = reply->readAll();
        qDebug() << "  响应:" << QString::fromUtf8(responseData);
    }

    reply->deleteLater();
    return success;
}

// 媒体控制方法实现
bool DLNAManager::playMedia(const QUrl& url)
{
    qDebug() << "\n尝试播放媒体:" << url.toString();
    
    if (!m_connected || !m_devices.contains(m_currentDeviceId)) {
        qDebug() << "播放失败: 设备未连接";
        emit error(tr("Device not connected"));
        return false;
    }

    const DLNADevice& device = m_devices[m_currentDeviceId];
    qDebug() << "当前设备:" << device.friendlyName;
    
    // 检查AVTransport服务
    if (!device.hasService(UPnP_AVTransport)) {
        qDebug() << "播放失败: 设备不支持AVTransport服务";
        emit error(tr("Device does not support media playback"));
        return false;
    }

    // 设置媒体URI
    QMap<QString, QString> args;
    args["InstanceID"] = "0";
    args["CurrentURI"] = url.toString();
    args["CurrentURIMetaData"] = "";
    
    qDebug() << "设置媒体URI:" << url.toString();
    if (!sendUPnPAction(UPnP_AVTransport, "SetAVTransportURI", args)) {
        qDebug() << "播放失败: 无法设置媒体URI";
        emit error(tr("Failed to set media URI"));
        return false;
    }

    // 等待一段时间让设备加载媒体
    QThread::msleep(500);

    // 发送播放请求到本地播放器
    emit requestLocalPlay(url);

    // 开始播放DLNA设备
    args.clear();
    args["InstanceID"] = "0";
    args["Speed"] = "1";
    
    qDebug() << "开始播放";
    if (!sendUPnPAction(UPnP_AVTransport, "Play", args)) {
        qDebug() << "播放失败: 无法开始播放";
        emit error(tr("Failed to start playback"));
        return false;
    }

    qDebug() << "播放命令发送成功";
    return true;
}

bool DLNAManager::pauseMedia()
{
    emit requestLocalPause();
    
    QMap<QString, QString> args;
    args["InstanceID"] = "0";
    return sendUPnPAction(UPnP_AVTransport, "Pause", args);
}

bool DLNAManager::stopMedia()
{
    emit requestLocalStop();
    
    QMap<QString, QString> args;
    args["InstanceID"] = "0";
    return sendUPnPAction(UPnP_AVTransport, "Stop", args);
}

bool DLNAManager::setVolume(int volume)
{
    emit requestLocalVolume(volume);
    
    QMap<QString, QString> args;
    args["InstanceID"] = "0";
    args["Channel"] = "Master";
    args["DesiredVolume"] = QString::number(volume);
    return sendUPnPAction(UPnP_RenderingControl, "SetVolume", args);
}

bool DLNAManager::seekTo(qint64 position)
{
    emit requestLocalSeek(position);
    
    QMap<QString, QString> args;
    args["InstanceID"] = "0";
    args["Unit"] = "REL_TIME";
    args["Target"] = QString("%1:%2:%3")
        .arg(position / 3600000)
        .arg((position % 3600000) / 60000, 2, 10, QChar('0'))
        .arg((position % 60000) / 1000, 2, 10, QChar('0'));
    return sendUPnPAction(UPnP_AVTransport, "Seek", args);
}

void DLNAManager::checkDeviceTimeouts()
{
    // 不再使用超时检查逻辑
}

void DLNAManager::onLocalPlaybackStateChanged(const QString& state)
{
    m_localPlaybackState = state;
    
    if (m_connected) {
        // 同步本地播放状态到DLNA设备
        if (state == "Playing") {
            QMap<QString, QString> args;
            args["InstanceID"] = "0";
            args["Speed"] = "1";
            sendUPnPAction(UPnP_AVTransport, "Play", args);
        } else if (state == "Paused") {
            QMap<QString, QString> args;
            args["InstanceID"] = "0";
            sendUPnPAction(UPnP_AVTransport, "Pause", args);
        } else if (state == "Stopped") {
            QMap<QString, QString> args;
            args["InstanceID"] = "0";
            sendUPnPAction(UPnP_AVTransport, "Stop", args);
        }
    }
}

void DLNAManager::onLocalPositionChanged(qint64 position)
{
    m_localPosition = position;
    
    if (m_connected) {
        // 同步播放位置到DLNA设备
        seekTo(position);
    }
}

void DLNAManager::onLocalDurationChanged(qint64 duration)
{
    m_localDuration = duration;
}

void DLNAManager::onLocalVolumeChanged(int volume)
{
    m_localVolume = volume;
    
    if (m_connected) {
        // 同步音量到DLNA设备
        setVolume(volume);
    }
} 
