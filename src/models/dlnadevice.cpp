#include "dlnadevice.h"
#include <QDebug>

DLNADevice::DLNADevice()
    : m_port(0)
{
}

bool DLNADevice::hasService(const QString& serviceType) const
{
    for (const auto& service : m_services) {
        if (service.serviceType == serviceType) {
            return true;
        }
    }
    return false;
}

DLNAService DLNADevice::getService(const QString& serviceType) const
{
    for (const auto& service : m_services) {
        if (service.serviceType == serviceType) {
            return service;
        }
    }
    return DLNAService();
}

QString DLNADevice::getBaseUrl() const
{
    return m_baseUrl;
}

QString DLNADevice::getFullUrl(const QString& relativeUrl) const
{
    if (relativeUrl.isEmpty()) {
        return QString();
    }

    if (relativeUrl.startsWith("http://") || relativeUrl.startsWith("https://")) {
        return relativeUrl;
    }

    QString baseUrl = getBaseUrl();
    if (baseUrl.isEmpty()) {
        return QString();
    }

    if (!baseUrl.endsWith('/') && !relativeUrl.startsWith('/')) {
        baseUrl += '/';
    }

    return baseUrl + relativeUrl;
}

bool DLNADevice::operator==(const DLNADevice& other) const
{
    return m_id == other.m_id;
}

bool DLNADevice::operator!=(const DLNADevice& other) const
{
    return !(*this == other);
}

DLNADevice DLNADevice::fromXml(const QDomDocument& doc, const QHostAddress& addr, quint16 devicePort)
{
    DLNADevice device;
    device.setAddress(addr);
    device.setPort(devicePort);
    device.parseFromXml(doc);
    return device;
}

bool DLNADevice::parseFromXml(const QDomDocument& doc)
{
    QDomElement root = doc.documentElement();
    if (root.isNull()) {
        qDebug() << "Invalid XML document: root element is null";
        return false;
    }

    // 解析设备信息
    QDomElement device = root.firstChildElement("device");
    if (!device.isNull()) {
        parseDeviceInfo(device);
        
        // 解析服务列表
        QDomElement serviceList = device.firstChildElement("serviceList");
        if (!serviceList.isNull()) {
            parseServiceList(serviceList);
        }
    }

    return true;
}

void DLNADevice::parseSpecVersion(const QDomElement& element)
{
    // 暂时不需要实现
}

void DLNADevice::parseDeviceInfo(const QDomElement& element)
{
    m_deviceType = element.firstChildElement("deviceType").text();
    m_id = element.firstChildElement("UDN").text();
    m_name = element.firstChildElement("friendlyName").text();
    m_baseUrl = element.firstChildElement("URLBase").text();
}

void DLNADevice::parseServiceList(const QDomElement& element)
{
    QDomElement serviceElement = element.firstChildElement("service");
    while (!serviceElement.isNull()) {
        DLNAService service = DLNAService::fromXml(serviceElement);
        if (!service.serviceType.isEmpty()) {
            m_services.append(service);
        }
        serviceElement = serviceElement.nextSiblingElement("service");
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