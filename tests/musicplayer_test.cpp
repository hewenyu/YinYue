#include <QtTest>
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
    player = new MusicPlayer();
    testPlaylist = new Playlist();
    
    // 创建测试音乐文件，使用实际的音频文件
    testFile1 = new MusicFile("/home/hewenyu/Music/天地龙鳞-王力宏.mp3");
    testFile2 = new MusicFile("/home/hewenyu/Music/如愿-王菲.mp3");
    
    // 添加测试音乐文件到播放列表
    testPlaylist->addFile(*testFile1);
    testPlaylist->addFile(*testFile2);
    player->setPlaylist(testPlaylist);

    // 等待媒体加载完成
    QTest::qWait(1000);
}

void MusicPlayerTest::testInitialState()
{
    QCOMPARE(player->state(), QMediaPlayer::StoppedState);
    QCOMPARE(player->volume(), 100);
    QCOMPARE(player->position(), 0);
    QCOMPARE(player->playMode(), Playlist::Sequential);
}

void MusicPlayerTest::testPlayPauseStop()
{
    // 测试播放
    player->play();
    QTRY_COMPARE_WITH_TIMEOUT(player->state(), QMediaPlayer::PlayingState, 3000);

    // 等待一段时间确保音乐在播放
    QTest::qWait(1000);

    // 测试暂停
    player->pause();
    QTRY_COMPARE_WITH_TIMEOUT(player->state(), QMediaPlayer::PausedState, 1000);

    // 测试停止
    player->stop();
    QTRY_COMPARE_WITH_TIMEOUT(player->state(), QMediaPlayer::StoppedState, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(player->position(), qint64(0), 1000);
}

void MusicPlayerTest::testVolumeControl()
{
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
    // 测试播放列表操作
    player->play();
    QTRY_COMPARE_WITH_TIMEOUT(player->state(), QMediaPlayer::PlayingState, 3000);

    // 等待一段时间确保音乐在播放
    QTest::qWait(1000);

    // 记录当前位置
    int currentIndex = testPlaylist->currentIndex();

    // 测试下一首
    player->next();
    QTRY_VERIFY_WITH_TIMEOUT(testPlaylist->currentIndex() != currentIndex, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(player->state() == QMediaPlayer::PlayingState, 3000);

    // 等待一段时间确保新的歌曲在播放
    QTest::qWait(1000);

    // 测试上一首
    currentIndex = testPlaylist->currentIndex();
    player->previous();
    QTRY_VERIFY_WITH_TIMEOUT(testPlaylist->currentIndex() != currentIndex, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(player->state() == QMediaPlayer::PlayingState, 3000);
}

void MusicPlayerTest::testPlayModes()
{
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
    delete testFile1;
    delete testFile2;
    delete player;
    delete testPlaylist;
}

QTEST_MAIN(MusicPlayerTest)
#include "musicplayer_test.moc" 