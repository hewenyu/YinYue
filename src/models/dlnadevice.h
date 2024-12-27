#ifndef DLNADEVICE_H
#define DLNADEVICE_H

#include <QString>
#include <QList>
#include <QHostAddress>
#include <QtXml/QDomDocument>

// DLNA服务信息结构体
struct DLNAService {
    QString serviceType;    // 服务类型
    QString serviceId;      // 服务ID
    QString controlURL;     // 控制URL
    QString eventSubURL;    // 事件订阅URL
    QString SCPDURL;        // SCPD URL

    // 构造函数
    DLNAService() {}
    
    // 从XML节点解析服务信息
    static DLNAService fromXml(const QDomElement& element);
};

// DLNA设备信息结构体
class DLNADevice {
public:
    // 基本设备信息
    QString deviceType;         // 设备类型
    QString friendlyName;       // 设备友好名称
    QString manufacturer;       // 制造商
    QString modelName;          // 型号名称
    QString modelDescription;   // 型号描述
    QString UDN;               // 唯一设备标识符
    QString URLBase;           // 基础URL
    
    // 网络信息
    QHostAddress address;      // 设备IP地址
    quint16 port;             // 设备端口
    
    // 版本信息
    int majorVersion;
    int minorVersion;
    
    // 服务列表
    QList<DLNAService> services;
    
    // 扩展信息(可选)
    QString presentationURL;    // 展示URL
    QString manufacturerURL;    // 制造商URL
    QString modelURL;          // 型号URL
    QString modelNumber;       // 型号编号
    
    // 设备状态
    bool isOnline;            // 设备是否在线
    qint64 lastSeenTime;      // 最后一次发现设备的时间
    
    // 构造函数
    DLNADevice();
    
    // 辅助方法
    bool hasService(const QString& serviceType) const;
    DLNAService getService(const QString& serviceType) const;
    QString getBaseUrl() const;
    
    // XML解析方法
    static DLNADevice fromXml(const QDomDocument& doc, const QHostAddress& addr = QHostAddress(), quint16 devicePort = 0);
    bool parseFromXml(const QDomDocument& doc);
    
    // 设备比较
    bool operator==(const DLNADevice& other) const;
    bool operator!=(const DLNADevice& other) const;
    
    // 获取完整URL
    QString getFullUrl(const QString& relativeUrl) const;
    
private:
    void parseSpecVersion(const QDomElement& element);
    void parseDeviceInfo(const QDomElement& element);
    void parseServiceList(const QDomElement& element);
};

#endif // DLNADEVICE_H 