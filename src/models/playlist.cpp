#include "playlist.h"
#include <QDebug>
#include <QFileInfo>
#include <algorithm>
#include <random>

Playlist::Playlist(QObject *parent)
    : QAbstractListModel(parent)
    , m_currentIndex(-1)
    , m_playMode(Sequential)
{
}

Playlist::~Playlist()
{
}

int Playlist::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_files.count();
}

QVariant Playlist::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_files.count()) {
        return QVariant();
    }

    const MusicFile &file = m_files.at(index.row());
    switch (role) {
        case Qt::DisplayRole:
        case TitleRole:
            return file.title().isEmpty() ? QFileInfo(file.filePath()).fileName() : file.title();
        case ArtistRole:
            return file.artist();
        case AlbumRole:
            return file.album();
        case GenreRole:
            return file.genre();
        case DurationRole:
            return file.duration();
        case FilePathRole:
        case Qt::ToolTipRole:
            return file.filePath();
        default:
            return QVariant();
    }
}

QHash<int, QByteArray> Playlist::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[TitleRole] = "title";
    roles[ArtistRole] = "artist";
    roles[AlbumRole] = "album";
    roles[GenreRole] = "genre";
    roles[DurationRole] = "duration";
    roles[FilePathRole] = "filePath";
    return roles;
}

void Playlist::addFile(const MusicFile &file)
{
    beginInsertRows(QModelIndex(), m_files.count(), m_files.count());
    m_files.append(file);
    endInsertRows();
}

void Playlist::removeFile(int index)
{
    if (index < 0 || index >= m_files.count()) {
        return;
    }

    beginRemoveRows(QModelIndex(), index, index);
    m_files.removeAt(index);
    endRemoveRows();

    if (m_currentIndex == index) {
        m_currentIndex = -1;
        emit currentIndexChanged(m_currentIndex);
    } else if (m_currentIndex > index) {
        m_currentIndex--;
        emit currentIndexChanged(m_currentIndex);
    }
}

void Playlist::clear()
{
    if (m_files.isEmpty()) {
        return;
    }

    beginRemoveRows(QModelIndex(), 0, m_files.count() - 1);
    m_files.clear();
    endRemoveRows();

    m_currentIndex = -1;
    emit currentIndexChanged(m_currentIndex);
}

MusicFile Playlist::at(int index) const
{
    if (index < 0 || index >= m_files.count()) {
        return MusicFile();
    }
    return m_files.at(index);
}

int Playlist::count() const
{
    return m_files.count();
}

int Playlist::currentIndex() const
{
    return m_currentIndex;
}

void Playlist::setCurrentIndex(int index)
{
    if (index == m_currentIndex) {
        return;
    }

    if (index < -1 || index >= m_files.count()) {
        index = -1;
    }

    m_currentIndex = index;
    emit currentIndexChanged(m_currentIndex);
}

int Playlist::nextIndex() const
{
    qDebug() << "计算下一首歌曲索引";
    qDebug() << "当前播放模式:" << m_playMode;
    qDebug() << "当前索引:" << m_currentIndex;
    qDebug() << "播放列表大小:" << m_files.count();

    if (m_files.isEmpty()) {
        qDebug() << "播放列表为空，返回-1";
        return -1;
    }

    switch (m_playMode) {
        case Sequential: {
            int next = m_currentIndex + 1;
            qDebug() << "顺序播放模式，下一首索引:" << (next < m_files.count() ? next : -1);
            return (next < m_files.count()) ? next : -1;
        }
        case Random: {
            if (m_files.count() <= 1) {
                qDebug() << "播放列表只有一首歌或为空，返回当前索引";
                return m_currentIndex;
            }
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, m_files.count() - 1);
            int nextIndex;
            do {
                nextIndex = dis(gen);
            } while (nextIndex == m_currentIndex);
            qDebug() << "随机播放模式，随机选择的下一首索引:" << nextIndex;
            return nextIndex;
        }
        case RepeatOne:
            qDebug() << "单曲循环模式，返回当前索引:" << m_currentIndex;
            return m_currentIndex;
        case RepeatAll: {
            int next = (m_currentIndex + 1) % m_files.count();
            qDebug() << "列表循环模式，下一首索引:" << next;
            return next;
        }
        default:
            qDebug() << "未知播放模式，返回-1";
            return -1;
    }
}

int Playlist::previousIndex() const
{
    qDebug() << "计算上一首歌曲索引";
    qDebug() << "当前播放模式:" << m_playMode;
    qDebug() << "当前索引:" << m_currentIndex;
    qDebug() << "播放列表大小:" << m_files.count();

    if (m_files.isEmpty()) {
        qDebug() << "播放列表为空，返回-1";
        return -1;
    }

    switch (m_playMode) {
        case Sequential: {
            int prev = m_currentIndex - 1;
            qDebug() << "顺序播放模式，上一首索引:" << (prev >= 0 ? prev : -1);
            return (prev >= 0) ? prev : -1;
        }
        case Random: {
            if (m_files.count() <= 1) {
                qDebug() << "播放列表只有一首歌或为空，返回当前索引";
                return m_currentIndex;
            }
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, m_files.count() - 1);
            int prevIndex;
            do {
                prevIndex = dis(gen);
            } while (prevIndex == m_currentIndex);
            qDebug() << "随机播放模式，随机选择的上一首索引:" << prevIndex;
            return prevIndex;
        }
        case RepeatOne:
            qDebug() << "单曲循环模式，返回当前索引:" << m_currentIndex;
            return m_currentIndex;
        case RepeatAll: {
            int prev = (m_currentIndex > 0) ? m_currentIndex - 1 : m_files.count() - 1;
            qDebug() << "列表循环模式，上一首索引:" << prev;
            return prev;
        }
        default:
            qDebug() << "未知播放模式，返回-1";
            return -1;
    }
}

Playlist::PlayMode Playlist::playMode() const
{
    return m_playMode;
}

void Playlist::setPlayMode(PlayMode mode)
{
    if (m_playMode != mode) {
        m_playMode = mode;
        emit playModeChanged(mode);
    }
} 