#include "playlist.h"
#include <QRandomGenerator>

Playlist::Playlist(QObject *parent)
    : QObject(parent)
    , m_currentIndex(-1)
    , m_playMode(Sequential)
{
}

Playlist::Playlist(const QString &name, QObject *parent)
    : QObject(parent)
    , m_name(name)
    , m_currentIndex(-1)
    , m_playMode(Sequential)
{
}

void Playlist::addFile(const MusicFile &file)
{
    m_files.append(file);
    emit playlistChanged();
}

void Playlist::removeFile(int index)
{
    if (index >= 0 && index < m_files.size()) {
        m_files.removeAt(index);
        if (index == m_currentIndex) {
            m_currentIndex = -1;
            emit currentIndexChanged(m_currentIndex);
        } else if (index < m_currentIndex) {
            m_currentIndex--;
            emit currentIndexChanged(m_currentIndex);
        }
        emit playlistChanged();
    }
}

void Playlist::clear()
{
    m_files.clear();
    m_currentIndex = -1;
    emit currentIndexChanged(m_currentIndex);
    emit playlistChanged();
}

int Playlist::nextIndex() const
{
    if (m_files.isEmpty()) {
        return -1;
    }

    switch (m_playMode) {
    case Sequential:
        return (m_currentIndex + 1) % m_files.size();
    case Random:
        return QRandomGenerator::global()->bounded(m_files.size());
    case RepeatOne:
        return m_currentIndex;
    case RepeatAll:
        return (m_currentIndex + 1) % m_files.size();
    }

    return -1;
}

int Playlist::previousIndex() const
{
    if (m_files.isEmpty()) {
        return -1;
    }

    switch (m_playMode) {
    case Sequential:
    case RepeatAll:
        return (m_currentIndex - 1 + m_files.size()) % m_files.size();
    case Random:
        return QRandomGenerator::global()->bounded(m_files.size());
    case RepeatOne:
        return m_currentIndex;
    }

    return -1;
}

int Playlist::currentIndex() const
{
    return m_currentIndex;
}

void Playlist::setCurrentIndex(int index)
{
    if (index != m_currentIndex && index >= -1 && index < m_files.size()) {
        m_currentIndex = index;
        emit currentIndexChanged(index);
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

QString Playlist::name() const
{
    return m_name;
}

void Playlist::setName(const QString &name)
{
    m_name = name;
}

int Playlist::count() const
{
    return m_files.size();
}

MusicFile Playlist::at(int index) const
{
    return m_files.value(index);
}

QList<MusicFile> Playlist::files() const
{
    return m_files;
} 