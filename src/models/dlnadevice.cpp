#include "dlnadevice.h"
#include <QDebug>

DLNADevice::DLNADevice()
    : port(0)
    , majorVersion(0)
    , minorVersion(0)
    , isOnline(false)
    , lastSeenTime(0)
{
}

bool DLNADevice::hasService(const QString& serviceType) const
{
    for (const auto& service : services) {
        if (service.serviceType == serviceType) {
            return true;
        }
    }
    return false;
}

DLNAService DLNADevice::getService(const QString& serviceType) const
{
    for (const auto& service : services) {
        if (service.serviceType == serviceType) {
            return service;
        }
    }
    return DLNAService();
}

QString DLNADevice::getBaseUrl() const
{
    if (!URLBase.isEmpty()) {
        return URLBase;
    }
    return QString("http://%1:%2").arg(address.toString()).arg(port);
}

QString DLNADevice::getFullUrl(const QString& relativeUrl) const
{
    QString baseUrl = getBaseUrl();
    if (baseUrl.endsWith("/") && relativeUrl.startsWith("/")) {
        baseUrl.chop(1);
    }
    return baseUrl + relativeUrl;
}

bool DLNADevice::operator==(const DLNADevice& other) const
{
    return UDN == other.UDN;
}

bool DLNADevice::operator!=(const DLNADevice& other) const
{
    return !(*this == other);
}

DLNADevice DLNADevice::fromXml(const QDomDocument& doc, const QHostAddress& addr, quint16 devicePort)
{
    DLNADevice device;
    device.address = addr;
    device.port = devicePort;
    
    if (device.parseFromXml(doc)) {
        return device;
    }
    return DLNADevice();
}

bool DLNADevice::parseFromXml(const QDomDocument& doc)
{
    QDomElement root = doc.documentElement();
    if (root.isNull()) {
        return false;
    }

    // 解析URLBase
    QDomElement urlBaseElement = root.firstChildElement("URLBase");
    if (!urlBaseElement.isNull()) {
        URLBase = urlBaseElement.text();
    }

    // 解析设备信息
    QDomElement deviceElement = root.firstChildElement("device");
    if (deviceElement.isNull()) {
        return false;
    }

    parseDeviceInfo(deviceElement);
    parseSpecVersion(root.firstChildElement("specVersion"));
    parseServiceList(deviceElement.firstChildElement("serviceList"));

    return !UDN.isEmpty();
}

void DLNADevice::parseSpecVersion(const QDomElement& element)
{
    if (!element.isNull()) {
        majorVersion = element.firstChildElement("major").text().toInt();
        minorVersion = element.firstChildElement("minor").text().toInt();
    }
}

void DLNADevice::parseDeviceInfo(const QDomElement& element)
{
    deviceType = element.firstChildElement("deviceType").text();
    friendlyName = element.firstChildElement("friendlyName").text();
    manufacturer = element.firstChildElement("manufacturer").text();
    modelName = element.firstChildElement("modelName").text();
    modelDescription = element.firstChildElement("modelDescription").text();
    UDN = element.firstChildElement("UDN").text();
    
    // 可选字段
    QDomElement presentationUrlElement = element.firstChildElement("presentationURL");
    if (!presentationUrlElement.isNull()) {
        presentationURL = presentationUrlElement.text();
    }
    
    QDomElement manufacturerUrlElement = element.firstChildElement("manufacturerURL");
    if (!manufacturerUrlElement.isNull()) {
        manufacturerURL = manufacturerUrlElement.text();
    }
    
    QDomElement modelUrlElement = element.firstChildElement("modelURL");
    if (!modelUrlElement.isNull()) {
        modelURL = modelUrlElement.text();
    }
    
    QDomElement modelNumberElement = element.firstChildElement("modelNumber");
    if (!modelNumberElement.isNull()) {
        modelNumber = modelNumberElement.text();
    }
}

void DLNADevice::parseServiceList(const QDomElement& element)
{
    services.clear();
    if (element.isNull()) {
        return;
    }

    QDomNodeList serviceNodes = element.elementsByTagName("service");
    for (int i = 0; i < serviceNodes.count(); ++i) {
        QDomElement serviceElement = serviceNodes.at(i).toElement();
        if (!serviceElement.isNull()) {
            services.append(DLNAService::fromXml(serviceElement));
        }
    }
}

DLNAService DLNAService::fromXml(const QDomElement& element)
{
    DLNAService service;
    service.serviceType = element.firstChildElement("serviceType").text();
    service.serviceId = element.firstChildElement("serviceId").text();
    service.controlURL = element.firstChildElement("controlURL").text();
    service.eventSubURL = element.firstChildElement("eventSubURL").text();
    service.SCPDURL = element.firstChildElement("SCPDURL").text();
    return service;
} 