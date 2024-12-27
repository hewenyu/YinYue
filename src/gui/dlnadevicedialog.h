#ifndef DLNADEVICEDIALOG_H
#define DLNADEVICEDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QLabel>
#include "core/musicplayer.h"

class DLNADeviceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DLNADeviceDialog(MusicPlayer* player, QWidget* parent = nullptr);
    ~DLNADeviceDialog();

private slots:
    void onDeviceDiscovered(const QString& deviceId, const QString& deviceName);
    void onDeviceLost(const QString& deviceId);
    void onConnectionStateChanged(bool connected);
    void onError(const QString& error);
    
    void onRefreshButtonClicked();
    void onConnectButtonClicked();
    void onDisconnectButtonClicked();
    void onDeviceItemClicked(QListWidgetItem* item);

private:
    void setupUI();
    void setupConnections();
    void updateButtons();

    MusicPlayer* m_player;
    QListWidget* m_deviceList;
    QPushButton* m_refreshButton;
    QPushButton* m_connectButton;
    QPushButton* m_disconnectButton;
    QLabel* m_statusLabel;
    QMap<QString, QString> m_deviceIds;  // 设备名称到ID的映射
};

#endif // DLNADEVICEDIALOG_H 