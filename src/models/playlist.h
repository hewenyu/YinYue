#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <QObject>
#include <QList>
#include <QAbstractListModel>
#include "musicfile.h"

class Playlist : public QAbstractListModel
{
    Q_OBJECT

public:
    enum PlayMode {
        Sequential,  // 顺序播放
        Random,     // 随机播放
        RepeatOne,  // 单曲循环
        RepeatAll   // 列表循环
    };
    Q_ENUM(PlayMode)

    explicit Playlist(QObject *parent = nullptr);
    ~Playlist();

    // QAbstractListModel接口
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    // 播放列表操作
    void addFile(const MusicFile &file);
    void removeFile(int index);
    void clear();
    MusicFile at(int index) const;
    int count() const;

    // 播放控制
    int currentIndex() const;
    void setCurrentIndex(int index);
    int nextIndex() const;
    int previousIndex() const;

    // 播放模式
    PlayMode playMode() const;
    void setPlayMode(PlayMode mode);

signals:
    void currentIndexChanged(int index);
    void playModeChanged(PlayMode mode);

private:
    QList<MusicFile> m_files;
    int m_currentIndex;
    PlayMode m_playMode;
};

#endif // PLAYLIST_H 