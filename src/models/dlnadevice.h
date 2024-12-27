#ifndef DLNADEVICE_H
#define DLNADEVICE_H

#include <QString>
#include <QHostAddress>
#include <QList>
#include <QtXml/QDomDocument>

// DLNA服务信息结构体
struct DLNAService {
    QString serviceType;    // 服务类型
    QString serviceId;      // 服务ID
    QString controlURL;     // 控制URL
    QString eventSubURL;    // 事件订阅URL
    QString SCPDURL;        // SCPD URL

    static DLNAService fromXml(const QDomElement& element);
};

class DLNADevice
{
public:
    DLNADevice();
    ~DLNADevice() = default;

    // 基本属性访问方法
    QString id() const { return m_id; }
    void setId(const QString& id) { m_id = id; }
    
    QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }
    
    QString deviceType() const { return m_deviceType; }
    void setDeviceType(const QString& type) { m_deviceType = type; }
    
    QHostAddress address() const { return m_address; }
    void setAddress(const QHostAddress& addr) { m_address = addr; }
    
    quint16 port() const { return m_port; }
    void setPort(quint16 port) { m_port = port; }

    // 服务相关方法
    bool hasService(const QString& serviceType) const;
    DLNAService getService(const QString& serviceType) const;
    QString getBaseUrl() const;
    QString getFullUrl(const QString& relativeUrl) const;

    // 比较运算符
    bool operator==(const DLNADevice& other) const;
    bool operator!=(const DLNADevice& other) const;

    // XML解析方法
    static DLNADevice fromXml(const QDomDocument& doc, const QHostAddress& addr = QHostAddress(), quint16 devicePort = 0);
    bool parseFromXml(const QDomDocument& doc);

private:
    QString m_id;
    QString m_name;
    QString m_deviceType;
    QHostAddress m_address;
    quint16 m_port = 0;
    QString m_baseUrl;
    QList<DLNAService> m_services;

    // XML解析辅助方法
    void parseSpecVersion(const QDomElement& element);
    void parseDeviceInfo(const QDomElement& element);
    void parseServiceList(const QDomElement& element);
};

#endif // DLNADEVICE_H 