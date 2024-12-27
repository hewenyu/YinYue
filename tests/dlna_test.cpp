#include <QTest>
#include <QSignalSpy>
#include <QDebug>
#include <QFile>
#include "../src/core/dlnamanager.h"
#include "../src/models/dlnadevice.h"

class DLNATest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {
        qDebug() << "Starting DLNA tests";
        // 验证测试音频文件是否存在
        QVERIFY2(QFile::exists(m_testMp3Path1), "测试文件1不存在");
        QVERIFY2(QFile::exists(m_testMp3Path2), "测试文件2不存在");
    }

    void cleanupTestCase() {
        qDebug() << "Finishing DLNA tests";
    }

    void init() {
        manager = new DLNAManager(this);
    }

    void cleanup() {
        if (manager->isConnected()) {
            manager->disconnectFromDevice();
        }
        delete manager;
    }

    // 测试设备发现功能
    void testDeviceDiscovery() {
        QSignalSpy discoveredSpy(manager, SIGNAL(deviceDiscovered(QString,QString)));
        QSignalSpy lostSpy(manager, SIGNAL(deviceLost(QString)));

        // 启动设备发现
        manager->startDiscovery();

        // 等待发现设备（最多等待10秒）
        bool found = false;
        for (int i = 0; i < 10 && !found; ++i) {
            discoveredSpy.wait(1000);
            QList<DLNADevice> devices = manager->getAvailableDevices();
            qDebug() << "发现的设备数量:" << devices.size();
            
            for (const auto& device : devices) {
                qDebug() << "设备:" << device.friendlyName;
                if (device.UDN.trimmed() == m_targetDeviceId) {
                    found = true;
                    qDebug() << "找到目标设备:" << device.friendlyName;
                    break;
                }
            }
        }

        // 停止发现
        manager->stopDiscovery();

        // 验证是否找到目标设备
        if (!found) {
            qDebug() << "目标设备ID:" << m_targetDeviceId;
            qDebug() << "未找到目标设备，可能需要确认设备是否在线或网络连接是否正常";
        }
        QVERIFY2(found, "未找到目标设备");
    }

    // 测试设备连接功能
    void testDeviceConnection() {
        QSignalSpy connectionSpy(manager, SIGNAL(connectionStateChanged(bool)));
        
        // 先发现设备
        manager->startDiscovery();
        
        // 等待发现目标设备（最多等待10秒）
        bool found = false;
        for (int i = 0; i < 10 && !found; ++i) {
            QTest::qWait(1000);
            QList<DLNADevice> devices = manager->getAvailableDevices();
            for (const auto& device : devices) {
                if (device.UDN.trimmed() == m_targetDeviceId) {
                    found = true;
                    break;
                }
            }
        }
        
        if (!found) {
            QSKIP("未找到目标设备，跳过连接测试");
        }
        
        // 尝试连接目标设备
        bool connected = manager->connectToDevice(m_targetDeviceId);
        if (!connected) {
            qDebug() << "连接失败，当前设备信息:";
            const auto& devices = manager->getAvailableDevices();
            for (const auto& device : devices) {
                if (device.UDN.trimmed() == m_targetDeviceId) {
                    qDebug() << "设备名称:" << device.friendlyName;
                    qDebug() << "设备位置:" << device.URLBase;
                    qDebug() << "设备类型:" << device.deviceType;
                    qDebug() << "设备地址:" << device.address.toString() << ":" << device.port;
                    break;
                }
            }
        }
        QVERIFY2(connected, "连接设备失败");
            
        // 等待连接状态变化
        if (connected) {
            bool stateChanged = connectionSpy.wait(2000);
            QVERIFY2(stateChanged, "未收到连接状态变化信号");
        }
            
        // 验证连接状态
        QVERIFY2(manager->isConnected(), "设备应该处于连接状态");
        QCOMPARE(manager->getCurrentDeviceId(), m_targetDeviceId);
            
        // 断开连接
        manager->disconnectFromDevice();
        QVERIFY2(!manager->isConnected(), "设备应该处于断开状态");
        
        manager->stopDiscovery();
    }

    // 测试媒体控制功能
    void testMediaControl() {
        // 先连接设备
        manager->startDiscovery();
        
        // 等待发现目标设备（最多等待10秒）
        bool found = false;
        for (int i = 0; i < 10 && !found; ++i) {
            QTest::qWait(1000);
            QList<DLNADevice> devices = manager->getAvailableDevices();
            for (const auto& device : devices) {
                if (device.UDN.trimmed() == m_targetDeviceId) {
                    found = true;
                    break;
                }
            }
        }
        
        if (!found) {
            QSKIP("未找到目标设备，跳过媒体控制测试");
        }
        
        if (manager->connectToDevice(m_targetDeviceId)) {
            QSignalSpy playbackSpy(manager, SIGNAL(playbackStateChanged(QString)));
            
            // 测试播放第一个文件
            QUrl testUrl1 = QUrl::fromLocalFile(m_testMp3Path1);
            qDebug() << "测试播放文件1:" << testUrl1.toString();
            bool playSuccess = manager->playMedia(testUrl1);
            if (!playSuccess) {
                qDebug() << "播放失败，检查文件路径:" << m_testMp3Path1;
                qDebug() << "文件是否存在:" << QFile::exists(m_testMp3Path1);
            }
            QVERIFY2(playSuccess, "播放文件1失败");
            
            if (playSuccess) {
                bool stateChanged = playbackSpy.wait(2000);
                QVERIFY2(stateChanged, "未收到播放状态变化信号");
            }
            
            // 测试暂停
            QTest::qWait(5000); // 播放5秒后暂停
            bool pauseSuccess = manager->pauseMedia();
            QVERIFY2(pauseSuccess, "暂停失败");
            
            if (pauseSuccess) {
                bool stateChanged = playbackSpy.wait(1000);
                QVERIFY2(stateChanged, "未收到暂停状态变化信号");
            }
            
            // 测试继续播放
            QTest::qWait(2000); // 暂停2秒后继续
            playSuccess = manager->playMedia(testUrl1);
            QVERIFY2(playSuccess, "继续播放失败");
            
            // 测试播放第二个文件
            QTest::qWait(3000);
            QUrl testUrl2 = QUrl::fromLocalFile(m_testMp3Path2);
            qDebug() << "测试播放文件2:" << testUrl2.toString();
            playSuccess = manager->playMedia(testUrl2);
            QVERIFY2(playSuccess, "播放文件2失败");
            
            // 测试音量控制
            QTest::qWait(3000);
            QVERIFY2(manager->setVolume(30), "设置音量30%失败");
            QTest::qWait(2000);
            QVERIFY2(manager->setVolume(60), "设置音量60%失败");
            
            // 测试停止
            QTest::qWait(3000);
            bool stopSuccess = manager->stopMedia();
            QVERIFY2(stopSuccess, "停止播放失败");
            
            manager->disconnectFromDevice();
        } else {
            QFAIL("连接目标设备失败");
        }
        
        manager->stopDiscovery();
    }

private:
    DLNAManager* manager;
    const QString m_targetDeviceId = "uuid:507b4406-58e3-4463-95bf-6211f55f12a4";
    const QString m_testMp3Path1 = "/home/hewenyu/Music/天地龙鳞-王力宏.mp3";
    const QString m_testMp3Path2 = "/home/hewenyu/Music/如愿-王菲.mp3";
};

QTEST_MAIN(DLNATest)
#include "dlna_test.moc" 
