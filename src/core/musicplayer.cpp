#include "musicplayer.h"
#include <QDebug>

MusicPlayer::MusicPlayer(QObject *parent)
    : QObject(parent)
    , m_localPlayer(new LocalPlayer(this))
    , m_dlnaManager(new DLNAManager(this))
    , m_playlist(nullptr)
    , m_currentPlaybackState("Stopped")
    , m_currentPosition(0)
    , m_currentDuration(0)
    , m_currentVolume(100)
    , m_isDeviceConnected(false)
{
    // 连接本地播放器信号
    connect(m_localPlayer, &LocalPlayer::playbackStateChanged,
            this, &MusicPlayer::handleLocalPlaybackStateChanged);
    connect(m_localPlayer, &LocalPlayer::positionChanged,
            this, &MusicPlayer::handleLocalPositionChanged);
    connect(m_localPlayer, &LocalPlayer::durationChanged,
            this, &MusicPlayer::handleLocalDurationChanged);
    connect(m_localPlayer, &LocalPlayer::volumeChanged,
            this, &MusicPlayer::handleLocalVolumeChanged);
    connect(m_localPlayer, &LocalPlayer::error,
            this, &MusicPlayer::handleLocalError);

    // 连接DLNA管理器信号
    connect(m_dlnaManager, &DLNAManager::deviceDiscovered,
            this, &MusicPlayer::handleDeviceDiscovered);
    connect(m_dlnaManager, &DLNAManager::deviceLost,
            this, &MusicPlayer::handleDeviceLost);
    connect(m_dlnaManager, &DLNAManager::connectionStateChanged,
            this, &MusicPlayer::handleDeviceConnectionChanged);
    connect(m_dlnaManager, &DLNAManager::playbackStateChanged,
            this, &MusicPlayer::handleDevicePlaybackStateChanged);
    connect(m_dlnaManager, &DLNAManager::error,
            this, &MusicPlayer::handleDeviceError);

    // 连接本地播放器和DLNA管理器的同步信号
    connect(m_dlnaManager, &DLNAManager::requestLocalPlay,
            m_localPlayer, &LocalPlayer::play);
    connect(m_dlnaManager, &DLNAManager::requestLocalPause,
            m_localPlayer, &LocalPlayer::pause);
    connect(m_dlnaManager, &DLNAManager::requestLocalStop,
            m_localPlayer, &LocalPlayer::stop);
    connect(m_dlnaManager, &DLNAManager::requestLocalSeek,
            m_localPlayer, &LocalPlayer::setPosition);
    connect(m_dlnaManager, &DLNAManager::requestLocalVolume,
            m_localPlayer, &LocalPlayer::setVolume);
}

MusicPlayer::~MusicPlayer()
{
    stop();
    if (m_isDeviceConnected) {
        disconnectFromDevice();
    }
}

// 播放控制方法
void MusicPlayer::play(const QUrl& url)
{
    qDebug() << "\n开始播放:" << url.toString();
    
    if (m_isDeviceConnected) {
        // 如果连接了DLNA设备，通过DLNA播放
        m_dlnaManager->playMedia(url);
    } else {
        // 否则使用本地播放器
        m_localPlayer->play(url);
    }
}

void MusicPlayer::pause()
{
    qDebug() << "暂停播放";
    
    if (m_isDeviceConnected) {
        m_dlnaManager->pauseMedia();
    } else {
        m_localPlayer->pause();
    }
}

void MusicPlayer::stop()
{
    qDebug() << "停止播放";
    
    if (m_isDeviceConnected) {
        m_dlnaManager->stopMedia();
    } else {
        m_localPlayer->stop();
    }
}

void MusicPlayer::setVolume(int volume)
{
    qDebug() << "设置音量:" << volume;
    
    if (m_isDeviceConnected) {
        m_dlnaManager->setVolume(volume);
    } else {
        m_localPlayer->setVolume(volume);
    }
}

void MusicPlayer::seekTo(qint64 position)
{
    qDebug() << "跳转到:" << position;
    
    if (m_isDeviceConnected) {
        m_dlnaManager->seekTo(position);
    } else {
        m_localPlayer->setPosition(position);
    }
}

// DLNA设备管理方法
void MusicPlayer::startDeviceDiscovery()
{
    m_dlnaManager->startDiscovery();
}

void MusicPlayer::stopDeviceDiscovery()
{
    m_dlnaManager->stopDiscovery();
}

QList<DLNADevice> MusicPlayer::getAvailableDevices() const
{
    return m_dlnaManager->getAvailableDevices();
}

bool MusicPlayer::connectToDevice(const QString& deviceId)
{
    bool success = m_dlnaManager->connectToDevice(deviceId);
    if (success) {
        m_isDeviceConnected = true;
    }
    return success;
}

void MusicPlayer::disconnectFromDevice()
{
    m_dlnaManager->disconnectFromDevice();
    m_isDeviceConnected = false;
}

bool MusicPlayer::isDeviceConnected() const
{
    return m_isDeviceConnected;
}

QString MusicPlayer::getCurrentDeviceId() const
{
    return m_dlnaManager->getCurrentDeviceId();
}

// 状态获取方法
QString MusicPlayer::getPlaybackState() const
{
    return m_currentPlaybackState;
}

qint64 MusicPlayer::getPosition() const
{
    return m_currentPosition;
}

qint64 MusicPlayer::getDuration() const
{
    return m_currentDuration;
}

int MusicPlayer::getVolume() const
{
    return m_currentVolume;
}

// 本地播放器状态处理
void MusicPlayer::handleLocalPlaybackStateChanged(const QString& state)
{
    if (!m_isDeviceConnected) {
        m_currentPlaybackState = state;
        emit playbackStateChanged(state);

        // 如果当前歌曲播放完毕，根据播放模式播放下一首
        if (state == "Stopped" && m_playlist) {
            switch (m_playlist->playMode()) {
                case Playlist::Sequential: {
                    int nextIndex = m_playlist->currentIndex() + 1;
                    if (nextIndex < m_playlist->count()) {
                        playTrack(nextIndex);
                    }
                    break;
                }
                case Playlist::Random: {
                    int nextIndex = m_playlist->nextIndex();
                    if (nextIndex != -1) {
                        playTrack(nextIndex);
                    }
                    break;
                }
                case Playlist::RepeatOne: {
                    playCurrentTrack();
                    break;
                }
                case Playlist::RepeatAll: {
                    int nextIndex = m_playlist->nextIndex();
                    if (nextIndex != -1) {
                        playTrack(nextIndex);
                    }
                    break;
                }
            }
        }
    }
}

void MusicPlayer::handleLocalPositionChanged(qint64 position)
{
    if (!m_isDeviceConnected) {
        m_currentPosition = position;
        emit positionChanged(position);
    }
}

void MusicPlayer::handleLocalDurationChanged(qint64 duration)
{
    if (!m_isDeviceConnected) {
        m_currentDuration = duration;
        emit durationChanged(duration);
    }
}

void MusicPlayer::handleLocalVolumeChanged(int volume)
{
    if (!m_isDeviceConnected) {
        m_currentVolume = volume;
        emit volumeChanged(volume);
    }
}

void MusicPlayer::handleLocalError(const QString& message)
{
    if (!m_isDeviceConnected) {
        emit error(tr("Local player error: %1").arg(message));
    }
}

// DLNA设备状态处理
void MusicPlayer::handleDeviceDiscovered(const QString& deviceId, const QString& deviceName)
{
    emit deviceDiscovered(deviceId, deviceName);
}

void MusicPlayer::handleDeviceLost(const QString& deviceId)
{
    emit deviceLost(deviceId);
}

void MusicPlayer::handleDeviceConnectionChanged(bool connected)
{
    m_isDeviceConnected = connected;
    emit deviceConnectionChanged(connected);
}

void MusicPlayer::handleDevicePlaybackStateChanged(const QString& state)
{
    if (m_isDeviceConnected) {
        m_currentPlaybackState = state;
        emit playbackStateChanged(state);
    }
}

void MusicPlayer::handleDeviceError(const QString& message)
{
    if (m_isDeviceConnected) {
        emit error(tr("DLNA device error: %1").arg(message));
    }
}

void MusicPlayer::play()
{
    if (!m_playlist || m_playlist->count() == 0) {
        qDebug() << "无法播放：播放列表为空";
        return;
    }

    if (m_playlist->currentIndex() == -1) {
        m_playlist->setCurrentIndex(0);
    }

    playCurrentTrack();
}

void MusicPlayer::next()
{
    if (!m_playlist || m_playlist->count() == 0) {
        return;
    }

    int nextIndex = m_playlist->nextIndex();
    if (nextIndex != -1) {
        m_playlist->setCurrentIndex(nextIndex);
        playCurrentTrack();
        emit currentSongChanged(nextIndex);
    }
}

void MusicPlayer::previous()
{
    if (!m_playlist || m_playlist->count() == 0) {
        return;
    }

    int prevIndex = m_playlist->previousIndex();
    if (prevIndex != -1) {
        m_playlist->setCurrentIndex(prevIndex);
        playCurrentTrack();
        emit currentSongChanged(prevIndex);
    }
}

void MusicPlayer::setPlaylist(Playlist* playlist)
{
    if (m_playlist) {
        disconnect(m_playlist, &Playlist::currentIndexChanged, this, &MusicPlayer::handlePlaylistChanged);
        disconnect(m_playlist, &Playlist::playModeChanged, this, &MusicPlayer::playModeChanged);
    }

    m_playlist = playlist;

    if (m_playlist) {
        connect(m_playlist, &Playlist::currentIndexChanged, this, &MusicPlayer::handlePlaylistChanged);
        connect(m_playlist, &Playlist::playModeChanged, this, &MusicPlayer::playModeChanged);
    }
}

void MusicPlayer::setPlayMode(Playlist::PlayMode mode)
{
    if (m_playlist) {
        m_playlist->setPlayMode(mode);
    }
}

Playlist::PlayMode MusicPlayer::playMode() const
{
    return m_playlist ? m_playlist->playMode() : Playlist::Sequential;
}

void MusicPlayer::handlePlaylistChanged()
{
    if (m_playlist->currentIndex() != -1) {
        playCurrentTrack();
        emit currentSongChanged(m_playlist->currentIndex());
    }
}

void MusicPlayer::playCurrentTrack()
{
    if (!m_playlist || m_playlist->currentIndex() == -1) {
        return;
    }

    MusicFile currentFile = m_playlist->at(m_playlist->currentIndex());
    play(currentFile.fileUrl());
}

void MusicPlayer::playTrack(int index)
{
    if (!m_playlist || index < 0 || index >= m_playlist->count()) {
        return;
    }

    m_playlist->setCurrentIndex(index);
    playCurrentTrack();
    emit currentSongChanged(index);
} 