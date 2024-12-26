#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QAction>
#include "core/musicplayer.h"
#include "gui/dlnadevicedialog.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void showDLNADialog();
    void onDLNAConnectionStateChanged(bool connected);

private:
    void setupUI();
    void createActions();
    void createMenus();
    void createToolBars();
    void updateDLNAStatus();

    MusicPlayer* m_player;
    DLNADeviceDialog* m_dlnaDialog;
    
    // DLNA 相关控件
    QAction* m_dlnaAction;
    QLabel* m_dlnaStatusLabel;
}; 