#ifndef MUSICFILE_H
#define MUSICFILE_H

#include <QString>
#include <QUrl>
#include <QDateTime>

class MusicFile
{
public:
    MusicFile();
    MusicFile(const QString &filePath);

    // Getters
    QString title() const { return m_title; }
    QString artist() const { return m_artist; }
    QString album() const { return m_album; }
    QString genre() const { return m_genre; }
    int duration() const { return m_duration; }
    QUrl fileUrl() const { return m_fileUrl; }
    QString filePath() const { return m_filePath; }
    QDateTime lastModified() const { return m_lastModified; }

    // Setters
    void setTitle(const QString &title) { m_title = title; }
    void setArtist(const QString &artist) { m_artist = artist; }
    void setAlbum(const QString &album) { m_album = album; }
    void setGenre(const QString &genre) { m_genre = genre; }
    void setDuration(int duration) { m_duration = duration; }
    void setFileUrl(const QUrl &url) { m_fileUrl = url; }
    void setFilePath(const QString &path) { m_filePath = path; }
    void setLastModified(const QDateTime &dt) { m_lastModified = dt; }

    // 从文件加载元数��
    bool loadMetadata();

private:
    QString m_title;
    QString m_artist;
    QString m_album;
    QString m_genre;
    int m_duration;
    QUrl m_fileUrl;
    QString m_filePath;
    QDateTime m_lastModified;
};

#endif // MUSICFILE_H 