#include "musicfile.h"
#include <QFileInfo>
#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QMediaContent>
#include <QEventLoop>

MusicFile::MusicFile()
    : m_duration(0)
{
}

MusicFile::MusicFile(const QString &filePath)
    : m_duration(0)
    , m_filePath(filePath)
{
    QFileInfo fileInfo(filePath);
    m_fileUrl = QUrl::fromLocalFile(filePath);
    m_lastModified = fileInfo.lastModified();
    m_title = fileInfo.baseName(); // 默认使用文件名作为标题
    loadMetadata();
}

bool MusicFile::loadMetadata()
{
    if (m_filePath.isEmpty()) {
        return false;
    }

    // 创建临时的QMediaPlayer来读取元数据
    QMediaPlayer player;
    player.setMedia(QMediaContent(m_fileUrl));

    // 等待元数据加载完成
    QEventLoop loop;
    QObject::connect(&player, SIGNAL(metaDataChanged()), &loop, SLOT(quit()));
    loop.exec();

    // 读取元数据
    if (player.metaData(QMediaMetaData::Title).isValid()) {
        m_title = player.metaData(QMediaMetaData::Title).toString();
    }
    if (player.metaData(QMediaMetaData::AlbumArtist).isValid()) {
        m_artist = player.metaData(QMediaMetaData::AlbumArtist).toString();
    }
    if (player.metaData(QMediaMetaData::AlbumTitle).isValid()) {
        m_album = player.metaData(QMediaMetaData::AlbumTitle).toString();
    }
    if (player.metaData(QMediaMetaData::Genre).isValid()) {
        m_genre = player.metaData(QMediaMetaData::Genre).toString();
    }
    if (player.duration() > 0) {
        m_duration = player.duration();
    }

    return true;
} 