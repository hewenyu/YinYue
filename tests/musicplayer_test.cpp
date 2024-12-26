#include <QtTest>
#include <QDebug>
#include "core/musicplayer.h"
#include "models/musicfile.h"

class MusicPlayerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testInitialState();
    void testPlayPauseStop();
    void testVolumeControl();
    void testPlaylistOperations();
    void testPlayModes();
    void cleanupTestCase();

private:
    MusicPlayer* player;
    Playlist* testPlaylist;
    MusicFile* testFile1;
    MusicFile* testFile2;
};

void MusicPlayerTest::initTestCase()
{
    qDebug() << "Setting up test case";
    player = new MusicPlayer();
    testPlaylist = new Playlist();
    
    // 创建测试音乐文件，使用实际的音频文件
    testFile1 = new MusicFile("/home/hewenyu/Music/天地龙鳞-王力宏.mp3");
    testFile2 = new MusicFile("/home/hewenyu/Music/如愿-王菲.mp3");
    
    qDebug() << "Adding files to playlist";
    // 添加测试音乐文件到播放列表
    testPlaylist->addFile(*testFile1);
    testPlaylist->addFile(*testFile2);
    player->setPlaylist(testPlaylist);

    // 等待媒体加载完成
    QTest::qWait(2000);
    qDebug() << "Test case setup complete";
}

void MusicPlayerTest::testInitialState()
{
    qDebug() << "Testing initial state";
    QCOMPARE(player->state(), QMediaPlayer::StoppedState);
    QCOMPARE(player->volume(), 100);
    QCOMPARE(player->position(), 0);
    QCOMPARE(player->playMode(), Playlist::Sequential);
}

void MusicPlayerTest::testPlayPauseStop()
{
    qDebug() << "Testing play/pause/stop";
    // 确保初始状态是停止状态
    player->stop();
    QTest::qWait(1000);
    QCOMPARE(player->state(), QMediaPlayer::StoppedState);

    // 测试播放
    qDebug() << "Testing play";
    player->play();
    QTRY_VERIFY_WITH_TIMEOUT(player->state() == QMediaPlayer::PlayingState, 10000);
    QTest::qWait(2000);  // 等待一段时间确保音乐在播放

    // 测试暂停
    qDebug() << "Testing pause";
    player->pause();
    QTRY_VERIFY_WITH_TIMEOUT(player->state() == QMediaPlayer::PausedState, 5000);
    QTest::qWait(1000);

    // 测试停止
    qDebug() << "Testing stop";
    player->stop();
    QTRY_VERIFY_WITH_TIMEOUT(player->state() == QMediaPlayer::StoppedState, 5000);
    QTest::qWait(1000);
    QTRY_COMPARE_WITH_TIMEOUT(player->position(), qint64(0), 5000);
}

void MusicPlayerTest::testVolumeControl()
{
    qDebug() << "Testing volume control";
    // 测试音量控制
    player->setVolume(50);
    QCOMPARE(player->volume(), 50);

    player->setVolume(0);
    QCOMPARE(player->volume(), 0);

    player->setVolume(100);
    QCOMPARE(player->volume(), 100);
}

void MusicPlayerTest::testPlaylistOperations()
{
    qDebug() << "Testing playlist operations";
    // 确保初始状态是停止状态
    player->stop();
    QTest::qWait(1000);

    // 测试播放列表操作
    qDebug() << "Starting playback";
    player->play();
    QTRY_VERIFY_WITH_TIMEOUT(player->state() == QMediaPlayer::PlayingState, 10000);
    QTest::qWait(2000);

    // 记录当前位置
    int currentIndex = testPlaylist->currentIndex();
    qint64 currentPosition = player->position();
    qDebug() << "Current index:" << currentIndex << "position:" << currentPosition;

    // 测试下一首
    qDebug() << "Testing next track";
    player->next();
    QTRY_VERIFY_WITH_TIMEOUT(testPlaylist->currentIndex() != currentIndex, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(player->state() == QMediaPlayer::PlayingState, 10000);
    QTest::qWait(2000);

    // 测试上一首
    qDebug() << "Testing previous track";
    currentIndex = testPlaylist->currentIndex();
    player->previous();
    QTRY_VERIFY_WITH_TIMEOUT(testPlaylist->currentIndex() != currentIndex, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(player->state() == QMediaPlayer::PlayingState, 10000);
}

void MusicPlayerTest::testPlayModes()
{
    qDebug() << "Testing play modes";
    // 测试播放模式
    player->setPlayMode(Playlist::Sequential);
    QCOMPARE(player->playMode(), Playlist::Sequential);

    player->setPlayMode(Playlist::Random);
    QCOMPARE(player->playMode(), Playlist::Random);

    player->setPlayMode(Playlist::RepeatAll);
    QCOMPARE(player->playMode(), Playlist::RepeatAll);

    player->setPlayMode(Playlist::RepeatOne);
    QCOMPARE(player->playMode(), Playlist::RepeatOne);
}

void MusicPlayerTest::cleanupTestCase()
{
    qDebug() << "Cleaning up test case";
    delete testFile1;
    delete testFile2;
    delete player;
    delete testPlaylist;
}

QTEST_MAIN(MusicPlayerTest)
#include "musicplayer_test.moc" 