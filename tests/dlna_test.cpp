#include <QtTest>
#include <QSignalSpy>
#include <QDebug>
#include "core/dlnamanager.h"
#include "core/musicplayer.h"
#include "models/musicfile.h"
#include <unistd.h>
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
    QString m_realMp3Path1;
    QString m_realMp3Path2;

    void createTestFiles();
    void setupTestDevice();
};

// 初始化测试环境
void DLNATest::initTestCase()
{
    qDebug() << "=== 初始化测试环境 ===";
    
    // 创建测试对象
    m_dlnaManager = new DLNAManager(this);
    m_player = new MusicPlayer(this);
    
    // create test files
    createTestFiles();
    // setup test device
    setupTestDevice();
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
    qDebug() << "启动设备发现...";
    m_dlnaManager->startDiscovery();
    QSignalSpy discoveredSpy(m_dlnaManager, SIGNAL(deviceDiscovered(QString,QString)));
    qDebug() << "等待设备发现信号...";
    bool signalReceived = discoveredSpy.wait(10000);  // 增加等待时间到10秒
    qDebug() << "信号等待结果:" << (signalReceived ? "成功" : "超时");
    QVERIFY(signalReceived);
}

void DLNATest::cleanup()
{
    // 每个测试用例结束后的清理
    m_dlnaManager->stopDiscovery();
    m_dlnaManager->disconnectFromDevice();
    qDebug() << "测试用例结束\n";
}

void DLNATest::createTestFiles()
{
    qDebug() << "创建测试文件...";
    // 添加实际的音频文件
    m_testFiles.append(MusicFile(m_realMp3Path1));
    m_testFiles.append(MusicFile(m_realMp3Path2));
    qDebug() << "测试文件创建完成，文件数量:" << m_testFiles.size();
}

void DLNATest::setupTestDevice()
{
    qDebug() << "设置测试设备...";
    m_testDeviceId = "uuid:507b4406-58e3-4463-95bf-6211f55f12a4";  // 小爱音响
    qDebug() << "测试设备ID:" << m_testDeviceId;
}



void DLNATest::testDeviceDiscovery()
{
   
    bool found = false;
    int maxRetries = 10;  // 最大重试次数
    
    // 获取设备列表
    QList<DLNAManager::DLNADevice> devices = m_dlnaManager->getAvailableDevices();
    // 打印设备数量
    qDebug() << "设备数量:" << devices.size();


    while (devices.size() != 2)
    {
        qDebug() << "等待设备发现...";
        devices = m_dlnaManager->getAvailableDevices();
        qDebug() << "设备数量:" << devices.size();
        sleep(1);
        if (devices.size() == 2)
        {
            break;
        }
    }
    
    // 验证设备列表
    for (const auto& device : devices) {
        if (device.id == m_testDeviceId) {
            found = true;
            qDebug() << "找到目标设备:" << device.id;
            break;
        }
    }
    qDebug() << "发现的设备ID:" << (found ? m_testDeviceId : "未找到");
    qDebug() << "期望的设备ID:" << m_testDeviceId;
    QVERIFY(found);
}

// testDeviceConnection
void DLNATest::testDeviceConnection()
{
    // 后面来写
    QVERIFY(true);
}

void DLNATest::testDeviceLost()
{
    // 后面来写
    QVERIFY(true);
}

void DLNATest::testMultipleDevices()
{
    // 后面来写
    QVERIFY(true);
}

void DLNATest::testInvalidDevice()
{
    // 后面来写
    QVERIFY(true);
}   

void DLNATest::testPlayMedia()
{
    // 后面来写
    QVERIFY(true);
}

void DLNATest::testPauseMedia()
{
    // 后面来写
    QVERIFY(true);
}

void DLNATest::testStopMedia()
{
    // 后面来写
    QVERIFY(true);
}

void DLNATest::testVolumeControl()
{
    // 后面来写
    QVERIFY(true);
}

void DLNATest::testSeekPosition()
{
    // 后面来写
    QVERIFY(true);
}   

void DLNATest::testConnectionStateChanges()
{
    // 后面来写
    QVERIFY(true);
}

void DLNATest::testPlaybackStateChanges()
{
    // 后面来写
    QVERIFY(true);
}

void DLNATest::testErrorHandling()
{
    // 后面来写
    QVERIFY(true);
}

void DLNATest::testSwitchToLocalPlayback()
{
    // 后面来写
    QVERIFY(true);
}

void DLNATest::testSwitchToDLNAPlayback()
{
    // 后面来写
    QVERIFY(true);
}


void DLNATest::testAutoReconnect()
{
    // 后面来写
    QVERIFY(true);
}



QTEST_MAIN(DLNATest)
#include "dlna_test.moc" 
