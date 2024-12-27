#include "playlist.h"
#include <QDebug>
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
            return file.title();
        case Qt::ToolTipRole:
            return file.filePath();
        default:
            return QVariant();
    }
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
    if (m_files.isEmpty()) {
        return -1;
    }

    switch (m_playMode) {
        case Sequential:
            return (m_currentIndex + 1 < m_files.count()) ? m_currentIndex + 1 : -1;
        case Random: {
            if (m_files.count() <= 1) {
                return m_currentIndex;
            }
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, m_files.count() - 1);
            int nextIndex;
            do {
                nextIndex = dis(gen);
            } while (nextIndex == m_currentIndex);
            return nextIndex;
        }
        case RepeatOne:
            return m_currentIndex;
        case RepeatAll:
            return (m_currentIndex + 1) % m_files.count();
        default:
            return -1;
    }
}

int Playlist::previousIndex() const
{
    if (m_files.isEmpty()) {
        return -1;
    }

    switch (m_playMode) {
        case Sequential:
            return (m_currentIndex > 0) ? m_currentIndex - 1 : -1;
        case Random: {
            if (m_files.count() <= 1) {
                return m_currentIndex;
            }
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, m_files.count() - 1);
            int prevIndex;
            do {
                prevIndex = dis(gen);
            } while (prevIndex == m_currentIndex);
            return prevIndex;
        }
        case RepeatOne:
            return m_currentIndex;
        case RepeatAll:
            return (m_currentIndex > 0) ? m_currentIndex - 1 : m_files.count() - 1;
        default:
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