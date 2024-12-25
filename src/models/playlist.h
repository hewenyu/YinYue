#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <QObject>
#include <QList>
#include <QString>
#include "musicfile.h"

class Playlist : public QObject
{
    Q_OBJECT
public:
    enum PlayMode {
        Sequential,  // 顺序播放
        Random,     // 随机播放
        RepeatOne,  // 单曲循环
        RepeatAll   // 列表循环
    };

    explicit Playlist(QObject *parent = nullptr);
    explicit Playlist(const QString &name, QObject *parent = nullptr);

    // 基本操作
    void addFile(const MusicFile &file);
    void removeFile(int index);
    void clear();
    
    // 播放控制
    int nextIndex() const;
    int previousIndex() const;
    int currentIndex() const;
    void setCurrentIndex(int index);
    
    // 播放模式
    PlayMode playMode() const;
    void setPlayMode(PlayMode mode);
    
    // 获取信息
    QString name() const;
    void setName(const QString &name);
    int count() const;
    MusicFile at(int index) const;
    QList<MusicFile> files() const;

signals:
    void currentIndexChanged(int index);
    void playModeChanged(PlayMode mode);
    void playlistChanged();

private:
    QString m_name;
    QList<MusicFile> m_files;
    int m_currentIndex;
    PlayMode m_playMode;
};

#endif // PLAYLIST_H 