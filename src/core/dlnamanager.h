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

class QNetworkReply;

class DLNAManager : public QObject
{
    Q_OBJECT

public:
    // serviceList
    // DLNA 服务结构体
    struct DLNAService {
        QString serviceType;      // 服务类型 (如 "urn:schemas-upnp-org:service:AVTransport:1")
        QString serviceId;        // 服务ID (如 "urn:upnp-org:serviceId:AVTransport")
        QString scpdUrl;         // 服务描述文档URL
        QString controlUrl;      // 控制URL
        QString eventSubUrl;     // 事件订阅URL
    };

        // DLNA 设备信息结构体
    struct DLNADeviceInfo {
        // 基本设备信息
        QString deviceType;           // 设备类型 (如 "urn:schemas-upnp-org:device:MediaRenderer:1")
        QString friendlyName;         // 设备友好名称 (如 "小爱音箱-5204" 或 "月半的电视")
        QString manufacturer;         // 制造商 (如 "Mi, Inc." 或 "Xiaomi")
        QString manufacturerUrl;      // 制造商URL (如 "http://www.xiaomi.com/")
        QString modelDescription;     // 型号描述 (如 "The Mi AI SoundBox" 或 "Xiaomi MediaRenderer")
        QString modelName;           // 型号名称
        QString modelNumber;         // 型号编号
        QString modelUrl;            // 型号URL (如 "http://www.xiaomi.com/hezi")
        QString presentationUrl;     // 展示URL
        QString udn;                 // 唯一设备名称 (uuid)
        QString upc;                 // 通用产品代码

        // // 扩展信息
        // struct ExtendedInfo {
        //     QString qplayCapability;     // QPlay 能力 (小米设备特有)
        //     QString dlnaDoc;             // DLNA 文档版本 (如 "DMR-1.50")
        //     QString dlnaCap;             // DLNA 能力
            
        //     // RController 信息 (小米电视特有)
        //     struct RControllerInfo {
        //         QString version;         // 控制器版本
        //         struct Service {
        //             QString serviceType;     // 服务类型
        //             QString actionListUrl;   // 动作列表URL
        //         };
        //         QList<Service> services;     // 控制器服务列表
        //     } rController;
        // } extendedInfo;

        // 服务列表
        QList<DLNAService> services;    // 设备提供的服务列表
        QString urlBase;                // URL基础地址

        DLNADeviceInfo() {}
        DLNADeviceInfo(const QString& type, const QString& name, const QString& mfr,
                    const QString& desc, const QString& model, const QString& number,
                    const QString& id)
            : deviceType(type), friendlyName(name), manufacturer(mfr),
            modelDescription(desc), modelName(model), modelNumber(number),
            udn(id) {}
    };

    struct DLNADevice {
        QString id; // 设备ID (uuid)
        QString name; // 设备名称
        QString location; // 设备位置URL
        QString type; // 设备类型
        QHostAddress address; // 设备IP地址
        quint16 port; // 设备端口
        QDateTime lastSeen; // 最后发现时间
        DLNADeviceInfo info;     // 设备详细信息
        QMap<QString, DLNAService> services;  // 服务类型到服务的映射

        DLNADevice() : port(0) {}
        DLNADevice(const QString& _id, const QString& _name, const QString& _location)
        : id(_id), name(_name), location(_location), port(0), 
          lastSeen(QDateTime::currentDateTime()) {}

        // 重载输出运算符
        friend QDebug operator<<(QDebug debug, const DLNADevice& device);
    };

    explicit DLNAManager(QObject *parent = nullptr);
    ~DLNAManager();

    // 更新设备信息
    void updateDeviceInfo(const DLNADevice& info);

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
    QMap<QString, QDateTime> m_deviceTimeouts;
    QString m_currentDeviceId;
    bool m_connected;
    QString m_currentPlaybackState;
    QMap<QString, QString> m_lastResponse;

    void clearDevices();
    bool sendUPnPAction(const QString& serviceType, const QString& action, const QMap<QString, QString>& arguments);
    void startPlaybackMonitoring();
    void stopPlaybackMonitoring();
    void parseDeviceDescription(const QByteArray& data);
    void fetchDeviceDescription(const QString& location);
    void addDevice(const DLNADevice& device);
    void removeDevice(const QString& deviceId);
    QString extractHeader(const QString& response, const QString& header);
};

QDebug operator<<(QDebug debug, const DLNAManager::DLNADevice& device);

#endif // DLNAMANAGER_H 