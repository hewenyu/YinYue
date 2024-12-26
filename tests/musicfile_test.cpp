#include <QtTest>
#include "models/musicfile.h"

class MusicFileTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testConstructor();
    void testSettersAndGetters();
    void testLoadMetadata();
    void cleanupTestCase();

private:
    MusicFile* testFile;
    QString testFilePath;
    QString realMp3Path;
};

void MusicFileTest::initTestCase()
{
    // 使用实际的音频文件
    realMp3Path = "/home/hewenyu/Music/天地龙鳞-王力宏.mp3";
    testFilePath = QDir::tempPath() + "/test.mp3";
    testFile = new MusicFile(realMp3Path);
}

void MusicFileTest::testConstructor()
{
    QCOMPARE(testFile->filePath(), realMp3Path);
    QVERIFY(testFile->fileUrl().isValid());
    QVERIFY(QFile::exists(testFile->filePath()));
}

void MusicFileTest::testSettersAndGetters()
{
    // 测试标题
    testFile->setTitle("天地龙鳞");
    QCOMPARE(testFile->title(), QString("天地龙鳞"));

    // 测试艺术家
    testFile->setArtist("王力宏");
    QCOMPARE(testFile->artist(), QString("王力宏"));

    // 测试专辑
    testFile->setAlbum("Test Album");
    QCOMPARE(testFile->album(), QString("Test Album"));

    // 测试流派
    testFile->setGenre("流行");
    QCOMPARE(testFile->genre(), QString("流行"));

    // 测试时长
    testFile->setDuration(180);
    QCOMPARE(testFile->duration(), 180);
}

void MusicFileTest::testLoadMetadata()
{
    // 使用另一个实际的音频文件测试
    MusicFile validFile("/home/hewenyu/Music/如愿-王菲.mp3");
    QVERIFY(validFile.loadMetadata());
    
    // 验证元数据是否正确加载
    QVERIFY(!validFile.title().isEmpty());
    QVERIFY(!validFile.artist().isEmpty());
    QVERIFY(validFile.duration() > 0);
}

void MusicFileTest::cleanupTestCase()
{
    delete testFile;
}

QTEST_MAIN(MusicFileTest)
#include "musicfile_test.moc" 