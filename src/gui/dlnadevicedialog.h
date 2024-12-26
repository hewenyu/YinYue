#ifndef DLNADEVICEDIALOG_H
#define DLNADEVICEDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include "core/musicplayer.h"

class DLNADeviceDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DLNADeviceDialog(MusicPlayer* player, QWidget* parent = nullptr);

private slots:
    void onDeviceDiscovered(const QString& deviceId, const QString& deviceName);
    void onDeviceLost(const QString& deviceId);
    void onConnectionStateChanged(bool connected);
    void onRefreshClicked();
    void onConnectClicked();
    void onDisconnectClicked();

private:
    void setupUI();
    void updateButtons();

    MusicPlayer* m_player;
    QListWidget* m_deviceList;
    QPushButton* m_refreshButton;
    QPushButton* m_connectButton;
    QPushButton* m_disconnectButton;
    QLabel* m_statusLabel;
};

#endif // DLNADEVICEDIALOG_H 