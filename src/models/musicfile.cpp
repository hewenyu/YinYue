#include "musicfile.h"
#include <QFileInfo>
#include <QDebug>
#include <taglib/fileref.h>
#include <taglib/tag.h>

MusicFile::MusicFile()
    : m_duration(0)
{
}

MusicFile::MusicFile(const QString &filePath)
    : m_duration(0)
    , m_filePath(filePath)
    , m_fileUrl(QUrl::fromLocalFile(filePath))
{
    QFileInfo fileInfo(filePath);
    m_lastModified = fileInfo.lastModified();
    loadMetadata();
}

bool MusicFile::loadMetadata()
{
    if (m_filePath.isEmpty()) {
        return false;
    }

    TagLib::FileRef f(m_filePath.toUtf8().constData());
    if (f.isNull() || !f.tag()) {
        qWarning() << "Failed to load metadata for file:" << m_filePath;
        QFileInfo fileInfo(m_filePath);
        m_title = fileInfo.fileName();
        return false;
    }

    TagLib::Tag *tag = f.tag();
    m_title = QString::fromStdString(tag->title().to8Bit(true));
    m_artist = QString::fromStdString(tag->artist().to8Bit(true));
    m_album = QString::fromStdString(tag->album().to8Bit(true));
    m_genre = QString::fromStdString(tag->genre().to8Bit(true));

    if (f.audioProperties()) {
        m_duration = f.audioProperties()->lengthInMilliseconds();
    }

    // 如果标题为空，使用文件名
    if (m_title.isEmpty()) {
        QFileInfo fileInfo(m_filePath);
        m_title = fileInfo.fileName();
    }

    return true;
} 