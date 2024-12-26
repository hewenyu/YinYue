#include "mainwindow.h"
#include <QLabel>
#include <QStyle>
#include <QApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_player(new MusicPlayer(this))
    , m_dlnaDialog(nullptr)
{
    setupUI();
    createActions();
    createMenus();
    createToolBars();
    
    // 连接DLNA相关信号
    connect(m_player, &MusicPlayer::dlnaConnectionStateChanged,
            this, &MainWindow::onDLNAConnectionStateChanged);
    
    updateDLNAStatus();
}

void MainWindow::setupUI()
{
    // ... 其他UI设置代码 ...
    
    // 添加DLNA状态标签到状态栏
    m_dlnaStatusLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_dlnaStatusLabel);
}

void MainWindow::createActions()
{
    // ... 其他动作创建代码 ...
    
    // 创建DLNA动作
    m_dlnaAction = new QAction(tr("DLNA Devices"), this);
    m_dlnaAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
    m_dlnaAction->setStatusTip(tr("Connect to DLNA devices"));
    connect(m_dlnaAction, &QAction::triggered, this, &MainWindow::showDLNADialog);
}

void MainWindow::createMenus()
{
    // ... 其他菜单创建代码 ...
    
    // 添加DLNA菜单项
    QMenu* toolsMenu = menuBar()->addMenu(tr("Tools"));
    toolsMenu->addAction(m_dlnaAction);
}

void MainWindow::createToolBars()
{
    // ... 其他工具栏创建代码 ...
    
    // 添加DLNA工具栏按钮
    QToolBar* toolBar = addToolBar(tr("Tools"));
    toolBar->addAction(m_dlnaAction);
}

void MainWindow::showDLNADialog()
{
    if (!m_dlnaDialog) {
        m_dlnaDialog = new DLNADeviceDialog(m_player, this);
    }
    m_dlnaDialog->show();
    m_dlnaDialog->raise();
    m_dlnaDialog->activateWindow();
}

void MainWindow::onDLNAConnectionStateChanged(bool connected)
{
    updateDLNAStatus();
}

void MainWindow::updateDLNAStatus()
{
    if (m_player->isDLNAConnected()) {
        m_dlnaStatusLabel->setText(tr("DLNA: Connected to %1").arg(m_player->getCurrentDLNADevice()));
    } else {
        m_dlnaStatusLabel->setText(tr("DLNA: Not connected"));
    }
} 