#include "dlnamanager.h"
#include <QNetworkReply>
#include <QDateTime>
#include <QXmlStreamReader>
#include <QDebug>
#include <QEventLoop>


/*
SSDP 设备类型
UPnP_RootDevice	upnp:rootdevice
UPnP_InternetGatewayDevice1	urn:schemas-upnp-org:device:InternetGatewayDevice:1
UPnP_WANConnectionDevice1	urn:schemas-upnp-org:device:WANConnectionDevice:1
UPnP_WANDevice1	urn:schemas-upnp-org:device:WANConnectionDevice:1
UPnP_WANCommonInterfaceConfig1	urn:schemas-upnp-org:service:WANCommonInterfaceConfig:1
UPnP_WANIPConnection1	urn:schemas-upnp-org:device:WANConnectionDevice:1
UPnP_Layer3Forwarding1	urn:schemas-upnp-org:service:WANIPConnection:1
UPnP_WANConnectionDevice1	urn:schemas-upnp-org:service:Layer3Forwarding:1
*/ 

// 设备类型
const QString DLNAManager::UPnP_RootDevice = "upnp:rootdevice";
const QString DLNAManager::UPnP_InternetGatewayDevice = "urn:schemas-upnp-org:device:InternetGatewayDevice:1";
const QString DLNAManager::UPnP_WANConnectionDevice = "urn:schemas-upnp-org:device:WANConnectionDevice:1";
const QString DLNAManager::UPnP_WANCommonInterfaceConfig = "urn:schemas-upnp-org:service:WANCommonInterfaceConfig:1";
const QString DLNAManager::UPnP_WANIPConnection = "urn:schemas-upnp-org:device:WANConnectionDevice:1";
const QString DLNAManager::UPnP_Layer3Forwarding = "urn:schemas-upnp-org:service:WANIPConnection:1";

/*
SSDP 服务类型
UPnP_MediaServer1	urn:schemas-upnp-org:device:MediaServer:1
UPnP_MediaRenderer1	urn:schemas-upnp-org:device:MediaRenderer:1
UPnP_ContentDirectory1	urn:schemas-upnp-org:service:ContentDirectory:1
UPnP_RenderingControl1	urn:schemas-upnp-org:service:RenderingControl:1
UPnP_ConnectionManager1	urn:schemas-upnp-org:service:ConnectionManager:1
UPnP_AVTransport1	urn:schemas-upnp-org:service:AVTransport:1
 */
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
    // 启动设备发现
    qDebug() << "Starting DLNA device discovery";
    clearDevices();
    sendSSDPDiscover();
    m_discoveryTimer->start(); // 启动发现定时器
    m_timeoutTimer->start();    // 启动超时检查定时器
}

void DLNAManager::stopDiscovery()
{
    qDebug() << "Stopping DLNA device discovery";
    // 发送byebye消息
    sendSSDPByebye();
    m_discoveryTimer->stop();  // 停止发现定时器
    m_timeoutTimer->stop();   // 停止超时检查定时器
}

QList<DLNAManager::DLNADevice> DLNAManager::getAvailableDevices() const
{
    QList<DLNAManager::DLNADevice> devices = m_devices.values();
    qDebug() << "\n当前可用设备列表:";
    qDebug() << "设备数量:" << devices.size();
    for (const auto& device : devices) {
        qDebug() << "  设备ID:" << device.id;
        qDebug() << "  设备名称:" << device.name;
        qDebug() << "  设备位置:" << device.location;
        qDebug() << "  设备类型:" << device.type;
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
    qDebug() << "正在连接到设备:" << device.name;
    
    // 验证设备是否有有效的控制URL
    if (device.location.isEmpty()) {
        qDebug() << "设备控制URL为空";
        emit error(tr("Invalid device control URL"));
        return false;
    }
    
    // 设当前设备ID（在发送UPnP动作之前）
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
    // 当设备添加到网络后，定期向（239.255.255.250:1900）发送SSDP通知消息宣告自己的设备和服务。
    /*
    ssdp:alive 消息格式

    NOTIFY * HTTP/1.1           // 消息头
    NT:                         // 在此消息中，NT必须为服务的服务类型。（如：upnp:rootdevice）
    HOST:                       // 设置为协议保留多播地址和端口，必须是：239.255.255.250:1900（IPv4）或FF0x::C(IPv6
    NTS:                        // 表示通知消息的子类型，必须为ssdp:alive
    LOCATION:                   // 包含根设备描述得URL地址  device 的webservice路径（如：http://127.0.0.1:2351/1.xml)
    CACHE-CONTROL:              // max-age指定通知消息存活时间，如果超过此时间间隔，控制点可以认为设备不存在 （如：max-age=1800）
    SERVER:                     // 包含操作系统名，版本，产品名和产品版本信息( 如：Windows NT/5.0, UPnP/1.0)
    USN:                        // 表示不同服务的统一服务名，它提供了一种标识出相同类型服务的能力。如：
                                // 根/启动设备 uuid:f7001351-cf4f-4edd-b3df-4b04792d0e8a::upnp:rootdevice
                                // 连接管理器  uuid:f7001351-cf4f-4edd-b3df-4b04792d0e8a::urn:schemas-upnp-org:service:ConnectionManager:1
                                // 内容管理器 uuid:f7001351-cf4f-4edd-b3df-4b04792d0e8a::urn:schemas-upnp-org:service:ContentDirectory:1

   
    */


   /*
    一般情况我们使用多播搜索消息来搜索有设备即可。多播搜索消息如下：
    M-SEARCH * HTTP/1.1             // 请求头 不可改变
    MAN: "ssdp:discover"            // 设置协议查询的类型，必须是：ssdp:discover
    MX: 5                           // 设置设备响应最长等待时间，设备响应在0和这个值之间随机选择响应延迟的值。这样可以为控制点响应平衡网络负载。
    HOST: 239.255.255.250:1900      // 设置为协议保留多播地址和端口，必须是：239.255.255.250:1900（IPv4）或FF0x::C(IPv6
    ST: upnp:rootdevice             // 设置服务查询的目标，它必须是下面的类型：
                                    // ssdp:all  搜索所有设备和服务
                                    // upnp:rootdevice  仅搜索网络中的��设备
                                    // uuid:device-UUID  查询UUID标识的设备
                                    // urn:schemas-upnp-org:device:device-Type:version  查询device-Type字段指定的设备类型，设备类型和版本由UPNP组织定义。
                                    // urn:schemas-upnp-org:service:service-Type:version  查询service-Type字段指定的服务类型，服务类型和版本由UPNP组织定义。

    */

    // 如果需要实现投屏，则设备类型 ST 为 urn:schemas-upnp-org:service:AVTransport:1

    // 搜索视频设备
    // 发送通用 MediaRenderer 搜索
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

// / sendSSDPByebye
void DLNAManager::sendSSDPByebye()
{
    /*
    ssdp:byebye 消息格式
    NOTIFY * HTTP/1.1       // 消息头
    HOST:                   // 设置为协议保留多播地址和端口，必须是：239.255.255.250:1900（IPv4）或FF0x::C(IPv6
    NTS:                    // 表示通知消息的子类型，必须为ssdp:byebye
    USN:                    // 表示不同服务的统一服务名，它提供了一种标识出相同类型服务的能力。如
                            // 根/启动设备 uuid:f7001351-cf4f-4edd-b3df-4b04792d0e8a::upnp:rootdevice
                            // 连接管理器  uuid:f7001351-cf4f-4edd-b3df-4b04792d0e8a::urn:schemas-upnp-org:service:ConnectionManager:1
                            // 内容管理器 uuid:f7001351-cf4f-4edd-b3df-4b04792d0e8a::urn:schemas-upnp-org:service:ContentDirectory:1
    */
    QByteArray ssdpRequest = 
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "NTS: ssdp:byebye\r\n"
        "USN: " + DLNAManager::UPnP_MediaRenderer.toUtf8() + "\r\n"
        "\r\n";
    qDebug() << "发送 SSDP 再见请求 (MediaRenderer):" << QString::fromUtf8(ssdpRequest);
    m_ssdpSocket->writeDatagram(ssdpRequest, SSDP_MULTICAST_ADDR, SSDP_PORT);   // 发送byebye消息
}

// 处理SSDP响应
/*
    QDEBUG : DLNATest::testDeviceDiscovery() 收到SSDP响应:

QDEBUG : DLNATest::testDeviceDiscovery() 发现设备USN: " uuid:a3fc6529-5b62-d12e-cfe1-f4546e07a920::urn:schemas-upnp-org:device:MediaRenderer:1"
QDEBUG : DLNATest::testDeviceDiscovery() 处理设备: " uuid:a3fc6529-5b62-d12e-cfe1-f4546e07a920"
QDEBUG : DLNATest::testDeviceDiscovery() 设备地址: "192.168.199.124" : 20549
QDEBUG : DLNATest::testDeviceDiscovery() 添加/更新设备: " uuid:a3fc6529-5b62-d12e-cfe1-f4546e07a920"
QDEBUG : DLNATest::testDeviceDiscovery() 新设备已发现: "Unknown Device"
QDEBUG : DLNATest::testDeviceDiscovery() 收到SSDP响应:
 "HTTP/1.1 200 OK\r\nLocation: http://192.168.199.106:9999/507b4406-58e3-4463-95bf-6211f55f12a4.xml\r\nExt:\r\nUSN: uuid:507b4406-58e3-4463-95bf-6211f55f12a4::urn:schemas-upnp-org:device:MediaRenderer:1\r\nServer: Linux/4.9.61 UPnP/1.0 GUPnP/1.0.2\r\nCache-Control: max-age=1800\r\nST: urn:schemas-upnp-org:device:MediaRenderer:1\r\nDate: Fri, 27 Dec 2024 05:41:49 GMT\r\nContent-Length: 0\r\n\r\n"
QDEBUG : DLNATest::testDeviceDiscovery() 发现设备位置: " http://192.168.199.106:9999/507b4406-58e3-4463-95bf-6211f55f12a4.xml"
QDEBUG : DLNATest::testDeviceDiscovery() 发现设备类型: " urn:schemas-upnp-org:device:MediaRenderer:1"
QDEBUG : DLNATest::testDeviceDiscovery() 发现设备USN: " uuid:507b4406-58e3-4463-95bf-6211f55f12a4::urn:schemas-upnp-org:device:MediaRenderer:1"
QDEBUG : DLNATest::testDeviceDiscovery() 处理设备: " uuid:507b4406-58e3-4463-95bf-6211f55f12a4"
QDEBUG : DLNATest::testDeviceDiscovery() 设备地址: "192.168.199.106" : 1900
QDEBUG : DLNATest::testDeviceDiscovery() 添加/更新设备: " uuid:507b4406-58e3-4463-95bf-6211f55f12a4"
QDEBUG : DLNATest::testDeviceDiscovery() 新设备已发现: "Unknown Device"
QDEBUG : DLNATest::testDeviceDiscovery() 信号等待结果: 成功
QDEBUG : DLNATest::testDeviceDiscovery() 发现的设备数量: 2
QDEBUG : DLNATest::testDeviceDiscovery() 设备: DLNADevice(id: " uuid:507b4406-58e3-4463-95bf-6211f55f12a4", name: "Unknown Device", location: " http://192.168.199.106:9999/507b4406-58e3-4463-95bf-6211f55f12a4.xml")
QDEBUG : DLNATest::testDeviceDiscovery() 设备: DLNADevice(id: " uuid:a3fc6529-5b62-d12e-cfe1-f4546e07a920", name: "Unknown Device", location: " http://192.168.199.124:49152/description.xml")
 */
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
        QString location = extractHeader(response, "LOCATION").simplified();
        QString st = extractHeader(response, "ST").simplified();
        QString usn = extractHeader(response, "USN").simplified();

        if (!location.isEmpty()) {
            qDebug() << "发现设备位置:" << location;
            qDebug() << "发现设备类型:" << st;
            qDebug() << "发现设备USN:" << usn;

            // 提取设备ID
            QString deviceId;
            if (usn.contains("::")) {
                deviceId = usn.split("::").first().simplified();
            } else {
                deviceId = usn.simplified();
            }
            qDebug() << "处理设备:" << deviceId;

            // 记录设备地址和端口
            qDebug() << "设备地址:" << senderAddress.toString() << ":" << senderPort;

            // 检查是否已存在相同ID的设备
            if (m_devices.contains(deviceId)) {
                qDebug() << "更新现有设备:" << deviceId;
                DLNADevice& existingDevice = m_devices[deviceId];
                existingDevice.address = senderAddress;
                existingDevice.port = senderPort;
                existingDevice.lastSeen = QDateTime::currentDateTime();
                
                // 即使位置未变也重新获取设备描述，以确保名称正确
                qDebug() << "重新获取设备描述";
                existingDevice.location = location;
                fetchDeviceDescription(location);
            } else {
                qDebug() << "添加新设备:" << deviceId;
                DLNADevice newDevice;
                newDevice.id = deviceId;
                newDevice.name = "Unknown Device";
                newDevice.location = location;
                newDevice.type = st;
                newDevice.address = senderAddress;
                newDevice.port = senderPort;
                newDevice.lastSeen = QDateTime::currentDateTime();
                
                addDevice(newDevice);
                fetchDeviceDescription(location);
            }
            
            // 更新设备超时时间
            m_deviceTimeouts[deviceId] = QDateTime::currentDateTime();
        }
    }
}

// 获取设备描述
/*
    描述文件有两种类型：设备描述文档(DDD)和服务描述文档(SDD)
    设备描述文档(DDD)：描述设备的基本信息，包括设备类型、制造商、型号、网络接口等。
    服务描述文档(SDD)：描述设备提供的服务，包括服务类型、控制URL、事件URL等。
    设备描述采用XML格式，可以通过HTTP GET请求获取。其链为设备发现消息中的Location。如上述设备的描述文件获取请求为
    http://192.168.199.106:9999/507b4406-58e3-4463-95bf-6211f55f12a4.xml
 */
void DLNAManager::fetchDeviceDescription(const QString& location)
{
    qDebug() << "\n正在获取设备描述文件:" << location;
    QUrl url(location);
    
    if (!url.isValid()) {
        qDebug() << "设备描述URL无效:" << location;
        return;
    }
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "YinYue/1.0");
    
    // 设置超时和缓存策略
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    
    QNetworkReply* reply = m_networkManager->get(request);
    
    // 设置超时定时器
    QTimer* timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(10000);  // 10秒总超时
    
    connect(timeoutTimer, &QTimer::timeout, this, [reply, location]() {
        if (reply->isRunning()) {
            qDebug() << "获取设备描述超时:" << location;
            reply->abort();
        }
    });
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, location]() {
        QTimer* timer = qobject_cast<QTimer*>(sender()->parent());
        if (timer) {
            timer->stop();
            timer->deleteLater();
        }
        
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            qDebug() << "\n成功获取设备描述文件:";
            qDebug() << "URL:" << location;
            qDebug() << "内容长度:" << data.length() << "字节";
            qDebug() << "内容:" << QString(data);
            parseDeviceDescription(data, location);
        } else {
            qDebug() << "\n获取设备描述失败:";
            qDebug() << "URL:" << location;
            qDebug() << "错误:" << reply->errorString();
            qDebug() << "错误代码:" << reply->error();
            qDebug() << "HTTP状态码:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        }
        reply->deleteLater();
    });
    
    timeoutTimer->start();
}

void DLNAManager::parseDeviceDescription(const QByteArray& data, const QString& location)
{
    qDebug() << "\n开始解析设备描述文件";
    
    if (data.isEmpty()) {
        qDebug() << "设备描述文件为空";
        return;
    }
    
    QXmlStreamReader xml(data);
    DLNADeviceInfo info;
    QString deviceId;
    bool foundDevice = false;
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement) {
            QString elementName = xml.name().toString();
            qDebug() << "解析XML元素:" << elementName;
            
            if (elementName == "device") {
                foundDevice = true;
                // 解析设备基本信息
                while (!xml.atEnd()) {
                    xml.readNext();
                    
                    if (xml.isStartElement()) {
                        QString childName = xml.name().toString();
                        QString value = xml.readElementText().trimmed();
                        qDebug() << "  " << childName << "=" << value;
                        
                        if (childName == "deviceType")
                            info.deviceType = value;
                        else if (childName == "friendlyName") {
                            info.friendlyName = value;
                            qDebug() << "找到设备名称:" << value;
                        }
                        else if (childName == "manufacturer")
                            info.manufacturer = value;
                        else if (childName == "manufacturerURL")
                            info.manufacturerUrl = value;
                        else if (childName == "modelDescription")
                            info.modelDescription = value;
                        else if (childName == "modelName")
                            info.modelName = value;
                        else if (childName == "modelNumber")
                            info.modelNumber = value;
                        else if (childName == "modelURL")
                            info.modelUrl = value;
                        else if (childName == "presentationURL")
                            info.presentationUrl = value;
                        else if (childName == "UDN") {
                            info.udn = value;
                            deviceId = value;
                            qDebug() << "找到设备ID:" << value;
                        }
                        else if (childName == "UPC")
                            info.upc = value;
                    }
                }
            }
        }
    }

    if (xml.hasError()) {
        qDebug() << "\nXML解析错误:";
        qDebug() << "错误信息:" << xml.errorString();
        qDebug() << "错误位置:" << xml.lineNumber() << "行," << xml.columnNumber() << "列";
        return;
    }

    if (!foundDevice) {
        qDebug() << "未找到device元素";
        return;
    }

    // 更新设备信息
    if (!deviceId.isEmpty() && m_devices.contains(deviceId)) {
        DLNADevice& device = m_devices[deviceId];
        device.info = info;
        
        // 只有在有效的friendlyName时才更新设备名称
        if (!info.friendlyName.isEmpty()) {
            device.name = info.friendlyName;
            qDebug() << "\n设备信息更新成功:";
            qDebug() << "  设备ID:" << deviceId;
            qDebug() << "  友好名称:" << device.name;
            qDebug() << "  设备类型:" << device.info.deviceType;
            qDebug() << "  制造商:" << device.info.manufacturer;
            qDebug() << "  型号:" << device.info.modelName;
            
            // 发送设备更新信号
            emit deviceDiscovered(device.id, device.name);
        } else {
            qDebug() << "设备描述中未找到有效的friendlyName";
        }
    } else {
        qDebug() << "\n无法更新设备信息:";
        qDebug() << "  设备ID是否为空:" << deviceId.isEmpty();
        qDebug() << "  设备是否存在:" << m_devices.contains(deviceId);
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
    qDebug() << "  名称:" << device.name;
    qDebug() << "  位置:" << device.location;
    qDebug() << "  类型:" << device.type;
    
    if (!m_devices.contains(device.id)) {
        m_devices[device.id] = device;
        qDebug() << "新设备已添加到设备列表";
    } else {
        m_devices[device.id] = device;
        qDebug() << "设备信息已更新";
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
    if (!device.services.contains(serviceType)) {
        qDebug() << "服务不存在:" << serviceType;
        return false;
    }

    const DLNAService& service = device.services[serviceType];
    if (service.controlUrl.isEmpty()) {
        qDebug() << "控制URL为空";
        return false;
    }

    // 构建完整的控制URL
    QUrl baseUrl(device.location);
    QString basePath = baseUrl.path();
    basePath = basePath.left(basePath.lastIndexOf('/') + 1);
    
    QUrl controlUrl = baseUrl;
    controlUrl.setPath(basePath + service.controlUrl.mid(service.controlUrl.startsWith('/') ? 1 : 0));
    
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
    request.setRawHeader("Connection", "keep-alive");
    request.setRawHeader("Content-Length", QString::number(soapBody.toUtf8().length()).toUtf8());
    request.setRawHeader("User-Agent", "YinYue/1.0");

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
