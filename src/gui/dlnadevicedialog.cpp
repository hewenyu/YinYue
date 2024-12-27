#include "dlnadevicedialog.h"
#include <QMessageBox>
#include <QLabel>
#include <QHBoxLayout>

DLNADeviceDialog::DLNADeviceDialog(MusicPlayer* player, QWidget* parent)
    : QDialog(parent)
    , m_player(player)
    , m_statusLabel(nullptr)
{
    setupUI();
    setupConnections();
    
    // 初始化状态
    updateButtons();
    
    // 开始搜索设备
    m_player->startDeviceDiscovery();
}

DLNADeviceDialog::~DLNADeviceDialog()
{
    m_player->stopDeviceDiscovery();
}

void DLNADeviceDialog::setupUI()
{
    setWindowTitle(tr("DLNA Devices"));
    setMinimumSize(400, 300);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // 设备列表
    m_deviceList = new QListWidget(this);
    mainLayout->addWidget(m_deviceList);
    
    // 状态标签
    m_statusLabel = new QLabel(tr("Searching for DLNA devices..."), this);
    mainLayout->addWidget(m_statusLabel);
    
    // 按钮布局
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    
    m_refreshButton = new QPushButton(tr("Refresh"), this);
    m_connectButton = new QPushButton(tr("Connect"), this);
    m_disconnectButton = new QPushButton(tr("Disconnect"), this);
    
    buttonLayout->addWidget(m_refreshButton);
    buttonLayout->addWidget(m_connectButton);
    buttonLayout->addWidget(m_disconnectButton);
    
    mainLayout->addLayout(buttonLayout);
}

void DLNADeviceDialog::setupConnections()
{
    connect(m_player, &MusicPlayer::deviceDiscovered, this, &DLNADeviceDialog::onDeviceDiscovered);
    connect(m_player, &MusicPlayer::deviceLost, this, &DLNADeviceDialog::onDeviceLost);
    connect(m_player, &MusicPlayer::deviceConnectionChanged, this, &DLNADeviceDialog::onConnectionStateChanged);
    connect(m_player, &MusicPlayer::error, this, &DLNADeviceDialog::onError);
    
    connect(m_deviceList, &QListWidget::itemClicked, this, &DLNADeviceDialog::onDeviceItemClicked);
    connect(m_refreshButton, &QPushButton::clicked, this, &DLNADeviceDialog::onRefreshButtonClicked);
    connect(m_connectButton, &QPushButton::clicked, this, &DLNADeviceDialog::onConnectButtonClicked);
    connect(m_disconnectButton, &QPushButton::clicked, this, &DLNADeviceDialog::onDisconnectButtonClicked);
}

void DLNADeviceDialog::updateButtons()
{
    bool hasSelection = !m_deviceList->selectedItems().isEmpty();
    bool isConnected = m_player->isDeviceConnected();
    
    m_connectButton->setEnabled(hasSelection && !isConnected);
    m_disconnectButton->setEnabled(isConnected);
    
    if (isConnected) {
        m_statusLabel->setText(tr("Connected to: %1").arg(m_player->getCurrentDeviceId()));
    } else {
        m_statusLabel->setText(tr("Not connected"));
    }
}

void DLNADeviceDialog::onDeviceDiscovered(const QString& deviceId, const QString& deviceName)
{
    QListWidgetItem* item = new QListWidgetItem(deviceName);
    item->setData(Qt::UserRole, deviceId);
    m_deviceList->addItem(item);
    m_deviceIds[deviceName] = deviceId;
    updateButtons();
}

void DLNADeviceDialog::onDeviceLost(const QString& deviceId)
{
    for (int i = 0; i < m_deviceList->count(); ++i) {
        QListWidgetItem* item = m_deviceList->item(i);
        if (item->data(Qt::UserRole).toString() == deviceId) {
            m_deviceIds.remove(item->text());
            delete m_deviceList->takeItem(i);
            break;
        }
    }
    updateButtons();
}

void DLNADeviceDialog::onConnectionStateChanged(bool connected)
{
    updateButtons();
}

void DLNADeviceDialog::onRefreshButtonClicked()
{
    m_deviceList->clear();
    m_deviceIds.clear();
    m_player->stopDeviceDiscovery();
    m_player->startDeviceDiscovery();
    m_statusLabel->setText(tr("Searching for DLNA devices..."));
}

void DLNADeviceDialog::onConnectButtonClicked()
{
    QListWidgetItem* selectedItem = m_deviceList->currentItem();
    if (!selectedItem) return;
    
    QString deviceId = selectedItem->data(Qt::UserRole).toString();
    m_player->connectToDevice(deviceId);
}

void DLNADeviceDialog::onDisconnectButtonClicked()
{
    m_player->disconnectFromDevice();
}

void DLNADeviceDialog::onDeviceItemClicked(QListWidgetItem* item)
{
    updateButtons();
}

void DLNADeviceDialog::onError(const QString& error)
{
    QMessageBox::warning(this, tr("DLNA Error"), error);
} 