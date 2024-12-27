#include "dlnadevice.h"
#include <QDateTime>

DLNAService DLNAService::fromXml(const QDomElement& element) {
    DLNAService service;
    service.serviceType = element.firstChildElement("serviceType").text();
    service.serviceId = element.firstChildElement("serviceId").text();
    service.controlURL = element.firstChildElement("controlURL").text();
    service.eventSubURL = element.firstChildElement("eventSubURL").text();
    service.SCPDURL = element.firstChildElement("SCPDURL").text();
    return service;
}

DLNADevice::DLNADevice() 
    : majorVersion(1)
    , minorVersion(0)
    , port(0)
    , isOnline(false)
    , lastSeenTime(0) {
}

bool DLNADevice::hasService(const QString& serviceType) const {
    for (const DLNAService& service : services) {
        if (service.serviceType == serviceType) {
            return true;
        }
    }
    return false;
}

DLNAService DLNADevice::getService(const QString& serviceType) const {
    for (const DLNAService& service : services) {
        if (service.serviceType == serviceType) {
            return service;
        }
    }
    return DLNAService();
}

QString DLNADevice::getBaseUrl() const {
    if (!URLBase.isEmpty()) {
        return URLBase;
    }
    if (address.isNull() || port == 0) {
        return QString();
    }
    return QString("http://%1:%2").arg(address.toString()).arg(port);
}

QString DLNADevice::getFullUrl(const QString& relativeUrl) const {
    QString baseUrl = getBaseUrl();
    if (baseUrl.isEmpty() || relativeUrl.isEmpty()) {
        return QString();
    }
    
    // 如果相对URL已经是完整URL，直接返回
    if (relativeUrl.startsWith("http://") || relativeUrl.startsWith("https://")) {
        return relativeUrl;
    }
    
    // 组合baseUrl和相对URL
    QString fullUrl = baseUrl;
    if (!fullUrl.endsWith('/') && !relativeUrl.startsWith('/')) {
        fullUrl += '/';
    }
    fullUrl += relativeUrl;
    return fullUrl;
}

DLNADevice DLNADevice::fromXml(const QDomDocument& doc, const QHostAddress& addr, quint16 devicePort) {
    DLNADevice device;
    device.address = addr;
    device.port = devicePort;
    device.parseFromXml(doc);
    return device;
}

bool DLNADevice::parseFromXml(const QDomDocument& doc) {
    QDomElement root = doc.documentElement();
    if (root.isNull()) {
        return false;
    }

    // 解析URLBase
    QDomElement urlBaseElement = root.firstChildElement("URLBase");
    if (!urlBaseElement.isNull()) {
        URLBase = urlBaseElement.text();
    }

    // 解析版本信息
    QDomElement specVersion = root.firstChildElement("specVersion");
    if (!specVersion.isNull()) {
        parseSpecVersion(specVersion);
    }

    // 解析设备信息
    QDomElement deviceElement = root.firstChildElement("device");
    if (!deviceElement.isNull()) {
        parseDeviceInfo(deviceElement);
        
        // 解析服务列表
        QDomElement serviceList = deviceElement.firstChildElement("serviceList");
        if (!serviceList.isNull()) {
            parseServiceList(serviceList);
        }
    }

    // 更新设备状态
    isOnline = true;
    lastSeenTime = QDateTime::currentMSecsSinceEpoch();
    
    return true;
}

void DLNADevice::parseSpecVersion(const QDomElement& element) {
    majorVersion = element.firstChildElement("major").text().toInt();
    minorVersion = element.firstChildElement("minor").text().toInt();
}

void DLNADevice::parseDeviceInfo(const QDomElement& element) {
    deviceType = element.firstChildElement("deviceType").text();
    friendlyName = element.firstChildElement("friendlyName").text();
    manufacturer = element.firstChildElement("manufacturer").text();
    modelName = element.firstChildElement("modelName").text();
    modelDescription = element.firstChildElement("modelDescription").text();
    UDN = element.firstChildElement("UDN").text();
    
    // 可选字段
    QDomElement elem;
    if (!(elem = element.firstChildElement("presentationURL")).isNull())
        presentationURL = elem.text();
    if (!(elem = element.firstChildElement("manufacturerURL")).isNull())
        manufacturerURL = elem.text();
    if (!(elem = element.firstChildElement("modelURL")).isNull())
        modelURL = elem.text();
    if (!(elem = element.firstChildElement("modelNumber")).isNull())
        modelNumber = elem.text();
}

void DLNADevice::parseServiceList(const QDomElement& element) {
    services.clear();
    QDomElement serviceElement = element.firstChildElement("service");
    while (!serviceElement.isNull()) {
        services.append(DLNAService::fromXml(serviceElement));
        serviceElement = serviceElement.nextSiblingElement("service");
    }
}

bool DLNADevice::operator==(const DLNADevice& other) const {
    return UDN == other.UDN;
}

bool DLNADevice::operator!=(const DLNADevice& other) const {
    return !(*this == other);
} 