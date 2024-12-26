#include <QtTest>
#include <QSignalSpy>
#include <QDebug>
#include "core/dlnamanager.h"
#include "core/musicplayer.h"
#include "models/musicfile.h"

class DLNATest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // DLNA Manager 测试
    void testDeviceDiscovery();
    void testDeviceConnection();
    void testDeviceLost();
    void testMultipleDevices();
    void testInvalidDevice();

    // DLNA 播放控制测试
    void testPlayMedia();
    void testPauseMedia();
    void testStopMedia();
    void testVolumeControl();
    void testSeekPosition();

    // DLNA 状态变化测试
    void testConnectionStateChanges();
    void testPlaybackStateChanges();
    void testErrorHandling();

    // DLNA 与本地播放切换测试
    void testSwitchToLocalPlayback();
    void testSwitchToDLNAPlayback();
    void testAutoReconnect();

private:
    DLNAManager* m_dlnaManager;
    MusicPlayer* m_player;
    QList<MusicFile> m_testFiles;
    QString m_testDeviceId;
    QString m_realMp3Path;

    void createTestFiles();
    void setupTestDevice();
};

void DLNATest::initTestCase()
{
    qDebug() << "=== 初始化测试环境 ===";
    
    // 创建测试对象
    m_dlnaManager = new DLNAManager(this);
    m_player = new MusicPlayer(this);
    
    // 设置实际音频文件路径
    m_realMp3Path = "/home/hewenyu/Music/天地龙鳞-王力宏.mp3";
    qDebug() << "使用实际音频文件:" << m_realMp3Path;
    
    // 创建测试文件
    qDebug() << "创建测试文件...";
    m_testFiles.append(m_realMp3Path);
    qDebug() << "测试文件创建完成，文件数量:" << m_testFiles.size();
    
    // 设置测试设备
    qDebug() << "设置测试设备...";
    m_testDeviceId = "uuid:507b4406-58e3-4463-95bf-6211f55f12a4";  // 小爱音箱的设备ID
    qDebug() << "测试设备ID:" << m_testDeviceId;
    
    qDebug() << "测试环境初始化完成";
}

void DLNATest::cleanupTestCase()
{
    qDebug() << "=== 清理测试环境 ===";
    delete m_player;
    delete m_dlnaManager;
    qDebug() << "测试环境清理完成";
}

void DLNATest::init()
{
    qDebug() << "\n=== 开始新的测试用例 ===";
    // 每个测试用例开始前的设置
    m_dlnaManager->stopDiscovery();
    m_dlnaManager->disconnectFromDevice();
}

void DLNATest::cleanup()
{
    // 每个测试用例结��后的清理
    m_dlnaManager->stopDiscovery();
    m_dlnaManager->disconnectFromDevice();
    qDebug() << "测试用例结束\n";
}

void DLNATest::createTestFiles()
{
    qDebug() << "创建测试文件...";
    // 添加实际的音频文件
    m_testFiles.append(MusicFile(m_realMp3Path));
    qDebug() << "测试文件创建完成，文件数量:" << m_testFiles.size();
}

void DLNATest::setupTestDevice()
{
    qDebug() << "设置测试设备...";
    m_testDeviceId = "uuid:a3fc6529-5b62-d12e-cfe1-f4546e07a920";  // 月半的电视
    qDebug() << "测试设备ID:" << m_testDeviceId;
}

void DLNATest::testDeviceDiscovery()
{
    qDebug() << "\n=== 开始新的测试用例 ===";
    m_dlnaManager->stopDiscovery();
    
    qDebug() << "\n=== 测试设备发现 ===";
    qDebug() << "启动设备发现...";
    
    QSignalSpy discoveredSpy(m_dlnaManager, SIGNAL(deviceDiscovered(QString,QString)));
    m_dlnaManager->startDiscovery();
    
    qDebug() << "等待设备发现信号...";
    bool signalReceived = discoveredSpy.wait(10000);  // 增加等待时间到10秒
    qDebug() << "信号等待结果:" << (signalReceived ? "成功" : "超时");
    QVERIFY(signalReceived);
    
    qDebug() << "验证发现的设备...";
    QList<DLNAManager::DLNADevice> devices = m_dlnaManager->getAvailableDevices();
    qDebug() << "发现的设备数量:" << devices.size();
    
    bool found = false;
    for (const auto& device : devices) {
        qDebug() << "检查设备:" << device.id << device.name << device.location;
        if (device.id == m_testDeviceId) {
            found = true;
            qDebug() << "找到目标设备:" << device.id;
            break;
        }
    }
    
    qDebug() << "发现的设备ID:" << (found ? m_testDeviceId : "未找到");
    qDebug() << "期望的设备ID:" << m_testDeviceId;
    QVERIFY(found);
    
    qDebug() << "设备发现测试完成";
    m_dlnaManager->stopDiscovery();
    qDebug() << "测试用例结束\n";
}

void DLNATest::testDeviceConnection()
{
    qDebug() << "\n=== 测试设备连接 ===";
    QSignalSpy connectionSpy(m_dlnaManager, &DLNAManager::connectionStateChanged);
    
    qDebug() << "尝试连接设备:" << m_testDeviceId;
    bool connected = m_dlnaManager->connectToDevice(m_testDeviceId);
    QVERIFY(connected);
    
    qDebug() << "等待连接状态变化信号...";
    QVERIFY(connectionSpy.wait(5000));
    
    qDebug() << "验证连接状态...";
    QVERIFY(m_dlnaManager->isConnected());
    QVERIFY(m_dlnaManager->getCurrentDeviceId() == m_testDeviceId);
    qDebug() << "设备连接测试完成";
}

void DLNATest::testDeviceLost()
{
    QSignalSpy lostSpy(m_dlnaManager, &DLNAManager::deviceLost);
    
    // 先连接设备
    m_dlnaManager->connectToDevice(m_testDeviceId);
    
    // 模拟设备丢失
    // 这里需要实现一个方法来模拟设备丢失
    
    // 等待设备丢失信号
    QVERIFY(lostSpy.wait(5000));
    
    // 验证设备已断开
    QVERIFY(!m_dlnaManager->isConnected());
    QVERIFY(m_dlnaManager->getCurrentDeviceId().isEmpty());
}

void DLNATest::testMultipleDevices()
{
    QSignalSpy discoveredSpy(m_dlnaManager, &DLNAManager::deviceDiscovered);
    
    // 启动设备发现
    m_dlnaManager->startDiscovery();
    
    // 等待多个设备被发现
    QVERIFY(discoveredSpy.wait(5000));
    
    // 验证可以获取所有可用设备
    QList<DLNAManager::DLNADevice> devices = m_dlnaManager->getAvailableDevices();
    QVERIFY(devices.size() > 1);
}

void DLNATest::testInvalidDevice()
{
    QSignalSpy errorSpy(m_dlnaManager, &DLNAManager::error);
    
    // 尝试连接无效设备
    bool connected = m_dlnaManager->connectToDevice("invalid_device_id");
    
    // 验证连接失败
    QVERIFY(!connected);
    QVERIFY(errorSpy.count() > 0);
}

void DLNATest::testPlayMedia()
{
    qDebug() << "\n=== 测试媒体播放 ===";
    
    qDebug() << "连接设备...";
    m_dlnaManager->connectToDevice(m_testDeviceId);
    
    QSignalSpy playbackSpy(m_dlnaManager, &DLNAManager::playbackStateChanged);
    
    qDebug() << "播放测试文件:" << m_testFiles[0].filePath();
    bool success = m_dlnaManager->playMedia(m_testFiles[0].fileUrl());
    QVERIFY(success);
    
    qDebug() << "等待播放状态变化...";
    QVERIFY(playbackSpy.wait(5000));
    QList<QVariant> arguments = playbackSpy.takeFirst();
    QVERIFY(arguments.at(0).toString() == "PLAYING");
    qDebug() << "媒体播放测试完成";
}

void DLNATest::testPauseMedia()
{
    qDebug() << "\n=== 测试媒体暂停 ===";
    
    qDebug() << "连接设备并播放媒体...";
    m_dlnaManager->connectToDevice(m_testDeviceId);
    m_dlnaManager->playMedia(m_testFiles[0].fileUrl());
    
    QSignalSpy playbackSpy(m_dlnaManager, &DLNAManager::playbackStateChanged);
    
    qDebug() << "暂停播放...";
    bool success = m_dlnaManager->pauseMedia();
    QVERIFY(success);
    
    qDebug() << "等待播放状态变化...";
    QVERIFY(playbackSpy.wait(5000));
    QList<QVariant> arguments = playbackSpy.takeFirst();
    QVERIFY(arguments.at(0).toString() == "PAUSED");
    qDebug() << "媒体暂停测试完成";
}

void DLNATest::testStopMedia()
{
    // 先播放媒体
    m_dlnaManager->connectToDevice(m_testDeviceId);
    m_dlnaManager->playMedia(m_testFiles[0].fileUrl());
    
    QSignalSpy playbackSpy(m_dlnaManager, &DLNAManager::playbackStateChanged);
    
    // 停止播放
    bool success = m_dlnaManager->stopMedia();
    QVERIFY(success);
    
    // 等待播放状态变化
    QVERIFY(playbackSpy.wait(5000));
    QList<QVariant> arguments = playbackSpy.takeFirst();
    QVERIFY(arguments.at(0).toString() == "STOPPED");
}

void DLNATest::testVolumeControl()
{
    // 先连接设备
    m_dlnaManager->connectToDevice(m_testDeviceId);
    
    // 测试设置音量
    bool success = m_dlnaManager->setVolume(50);
    QVERIFY(success);
    
    // 测试静音
    success = m_dlnaManager->setVolume(0);
    QVERIFY(success);
    
    // 测试最大音量
    success = m_dlnaManager->setVolume(100);
    QVERIFY(success);
}

void DLNATest::testSeekPosition()
{
    // 先播放媒体
    m_dlnaManager->connectToDevice(m_testDeviceId);
    m_dlnaManager->playMedia(m_testFiles[0].fileUrl());
    
    // 测试跳转到指定位置
    bool success = m_dlnaManager->seekTo(30000); // 跳转到30秒
    QVERIFY(success);
}

void DLNATest::testConnectionStateChanges()
{
    QSignalSpy connectionSpy(m_dlnaManager, &DLNAManager::connectionStateChanged);
    
    // 测试连接
    m_dlnaManager->connectToDevice(m_testDeviceId);
    QVERIFY(connectionSpy.wait(5000));
    QVERIFY(connectionSpy.takeFirst().at(0).toBool());
    
    // 测试断开连接
    m_dlnaManager->disconnectFromDevice();
    QVERIFY(connectionSpy.wait(5000));
    QVERIFY(!connectionSpy.takeFirst().at(0).toBool());
}

void DLNATest::testPlaybackStateChanges()
{
    QSignalSpy playbackSpy(m_dlnaManager, &DLNAManager::playbackStateChanged);
    
    // 连接设备并播放媒体
    m_dlnaManager->connectToDevice(m_testDeviceId);
    m_dlnaManager->playMedia(m_testFiles[0].fileUrl());
    
    // 验证状态变化序列：PLAYING -> PAUSED -> STOPPED
    QVERIFY(playbackSpy.wait(5000));
    QVERIFY(playbackSpy.takeFirst().at(0).toString() == "PLAYING");
    
    m_dlnaManager->pauseMedia();
    QVERIFY(playbackSpy.wait(5000));
    QVERIFY(playbackSpy.takeFirst().at(0).toString() == "PAUSED");
    
    m_dlnaManager->stopMedia();
    QVERIFY(playbackSpy.wait(5000));
    QVERIFY(playbackSpy.takeFirst().at(0).toString() == "STOPPED");
}

void DLNATest::testErrorHandling()
{
    QSignalSpy errorSpy(m_dlnaManager, &DLNAManager::error);
    
    // 测试无效URL
    m_dlnaManager->connectToDevice(m_testDeviceId);
    bool success = m_dlnaManager->playMedia(QUrl("invalid://url"));
    
    // 验证错误处理
    QVERIFY(!success);
    QVERIFY(errorSpy.count() > 0);
}

void DLNATest::testSwitchToLocalPlayback()
{
    qDebug() << "\n=== 测试切换到本地播放 ===";
    
    qDebug() << "通过DLNA播放...";
    m_player->connectToDLNADevice(m_testDeviceId);
    m_player->setSource(m_testFiles[0].fileUrl());
    m_player->play();
    
    QSignalSpy stateSpy(m_player, &MusicPlayer::stateChanged);
    
    qDebug() << "断开DLNA连接...";
    m_player->disconnectFromDLNADevice();
    
    qDebug() << "验证切换到本地播放...";
    QVERIFY(stateSpy.wait(5000));
    QVERIFY(!m_player->isDLNAConnected());
    QCOMPARE(m_player->state(), QMediaPlayer::PlayingState);
    qDebug() << "切换到本地播放测试完成";
}

void DLNATest::testSwitchToDLNAPlayback()
{
    qDebug() << "\n=== 测试切换到DLNA播放 ===";
    
    qDebug() << "本地播放...";
    m_player->setSource(m_testFiles[0].fileUrl());
    m_player->play();
    
    QSignalSpy stateSpy(m_player, &MusicPlayer::stateChanged);
    
    qDebug() << "连接DLNA设备...";
    bool success = m_player->connectToDLNADevice(m_testDeviceId);
    QVERIFY(success);
    
    qDebug() << "验证切换到DLNA播放...";
    QVERIFY(stateSpy.wait(5000));
    QVERIFY(m_player->isDLNAConnected());
    qDebug() << "切换到DLNA播放测试完成";
}

void DLNATest::testAutoReconnect()
{
    QSignalSpy connectionSpy(m_dlnaManager, &DLNAManager::connectionStateChanged);
    
    // 先连接设备
    m_dlnaManager->connectToDevice(m_testDeviceId);
    QVERIFY(connectionSpy.wait(5000));
    
    // 模拟连接断开
    // 这里需要实现一个方法来模拟连接断开
    
    // 验证自动重连
    QVERIFY(connectionSpy.wait(5000));
    QVERIFY(m_dlnaManager->isConnected());
}

QTEST_MAIN(DLNATest)
#include "dlna_test.moc" 