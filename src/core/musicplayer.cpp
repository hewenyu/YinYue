#include "musicplayer.h"
#include <QDebug>
#include <QTimer>

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
    , m_isManualStop(false)
    , m_isProcessingNextTrack(false)
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
    try {
        qDebug() << "\n开始播放:" << url.toString();
        qDebug() << "设备连接状态:" << (m_isDeviceConnected ? "已连接" : "未连接");
        
        if (!url.isValid()) {
            qDebug() << "错误: 无效的URL";
            emit error(tr("Invalid URL: %1").arg(url.toString()));
            return;
        }
        
        if (m_isDeviceConnected) {
            if (!m_dlnaManager) {
                qDebug() << "错误: DLNA管理器未初始化";
                emit error(tr("DLNA manager not initialized"));
                return;
            }
            qDebug() << "尝试通过DLNA播放...";
            qDebug() << "当前DLNA设备ID:" << m_dlnaManager->getCurrentDeviceId();
            m_dlnaManager->playMedia(url);
        } else {
            if (!m_localPlayer) {
                qDebug() << "错误: 本地播放器未初始化";
                emit error(tr("Local player not initialized"));
                return;
            }
            qDebug() << "使用本地播放器播放";
            m_localPlayer->play(url);
        }
    } catch (const std::exception& e) {
        qDebug() << "播放过程中发生异常:" << e.what();
        emit error(tr("Playback error: %1").arg(e.what()));
    } catch (...) {
        qDebug() << "播放过程中发生未知异常";
        emit error(tr("Unknown playback error"));
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
    lockState();
    m_isManualStop = true;  // 标记为手动停止
    unlockState();
    
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
    try {
        qDebug() << "\n尝试连接DLNA设备:" << deviceId;
        if (!m_dlnaManager) {
            qDebug() << "错误: DLNA管理器未初始化";
            emit error(tr("DLNA manager not initialized"));
            return false;
        }
        
        if (deviceId.isEmpty()) {
            qDebug() << "错误: 设备ID为空";
            emit error(tr("Empty device ID"));
            return false;
        }

        bool success = m_dlnaManager->connectToDevice(deviceId);
        qDebug() << "连接" << (success ? "成功" : "失败");
        if (success) {
            m_isDeviceConnected = true;
            qDebug() << "设备连接状态已更新为已连接";
        } else {
            qDebug() << "错误: 连接设备失败";
            emit error(tr("Failed to connect to device: %1").arg(deviceId));
        }
        return success;
    } catch (const std::exception& e) {
        qDebug() << "连接设备过程中发生异常:" << e.what();
        emit error(tr("Connection error: %1").arg(e.what()));
        return false;
    } catch (...) {
        qDebug() << "连接设备过程中发生未知异常";
        emit error(tr("Unknown connection error"));
        return false;
    }
}

void MusicPlayer::disconnectFromDevice()
{
    qDebug() << "\n断开DLNA设备连接";
    qDebug() << "当前设备ID:" << m_dlnaManager->getCurrentDeviceId();
    m_dlnaManager->disconnectFromDevice();
    m_isDeviceConnected = false;
    qDebug() << "设备连接状态已更新为未连接";
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
    qDebug() << "本地播放状态变化:" << state;
    if (!m_isDeviceConnected) {
        lockState();
        // 避免重复设置相同状态
        if (m_currentPlaybackState != state) {
            m_currentPlaybackState = state;
            emit playbackStateChanged(state);

            // 只在真正停止播放时（不是��停）且不是手动停止时，才自动播放下一首
            if (state == "Stopped" && m_playlist && m_playlist->currentIndex() != -1) {
                if (!m_isManualStop) {
                    if (!m_isProcessingNextTrack) {
                        // 检查当前播放时长，只有当播放时长超过1时才认为是正常播放结束
                        if (m_currentPosition > 1000) {
                            m_isProcessingNextTrack = true;
                            unlockState();  // 在开始处理下一首歌之前释放锁
                            
                            qDebug() << "当前歌曲播放完毕，检查是否需要播放下一首";
                            qDebug() << "当前播放位置:" << m_currentPosition;
                            
                            // 使用较长的延时确保状态已经完全清理
                            QTimer::singleShot(1000, this, [this]() {
                                lockState();  // 重新获取锁来处理下一首歌
                                if (!m_isManualStop) {  // 再次检查是否是手动停止
                                    switch (m_playlist->playMode()) {
                                        case Playlist::Sequential: {
                                            int nextIndex = m_playlist->nextIndex();
                                            qDebug() << "顺序播放模式，下一首索引:" << nextIndex;
                                            if (nextIndex != -1) {
                                                m_playlist->setCurrentIndex(nextIndex);
                                                unlockState();
                                                playCurrentTrack();
                                                return;
                                            }
                                            break;
                                        }
                                        case Playlist::Random: {
                                            int nextIndex = m_playlist->nextIndex();
                                            qDebug() << "随机播放模式，下一首索引:" << nextIndex;
                                            if (nextIndex != -1) {
                                                m_playlist->setCurrentIndex(nextIndex);
                                                unlockState();
                                                playCurrentTrack();
                                                return;
                                            }
                                            break;
                                        }
                                        case Playlist::RepeatOne: {
                                            qDebug() << "单曲循环模式，重新播放当前歌曲";
                                            unlockState();
                                            playCurrentTrack();
                                            return;
                                        }
                                        case Playlist::RepeatAll: {
                                            int nextIndex = m_playlist->nextIndex();
                                            qDebug() << "列表循环模式，下一首索引:" << nextIndex;
                                            if (nextIndex != -1) {
                                                m_playlist->setCurrentIndex(nextIndex);
                                                unlockState();
                                                playCurrentTrack();
                                                return;
                                            }
                                            break;
                                        }
                                    }
                                }
                                m_isProcessingNextTrack = false;
                                unlockState();
                            });
                            return;
                        } else {
                            qDebug() << "歌曲播放时间过短，不进行自动切换";
                            m_isProcessingNextTrack = false;
                        }
                    }
                }
                m_isManualStop = false;
            }
        }
        unlockState();
    }
}

void MusicPlayer::handleLocalPositionChanged(qint64 position)
{
    if (!m_isDeviceConnected) {
        lockState();
        m_currentPosition = position;
        unlockState();
        emit positionChanged(position);
    }
}

void MusicPlayer::handleLocalDurationChanged(qint64 duration)
{
    if (!m_isDeviceConnected) {
        lockState();
        m_currentDuration = duration;
        unlockState();
        emit durationChanged(duration);
    }
}

void MusicPlayer::handleLocalVolumeChanged(int volume)
{
    if (!m_isDeviceConnected) {
        lockState();
        m_currentVolume = volume;
        unlockState();
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
    qDebug() << "\n发现DLNA设备:" << deviceName;
    qDebug() << "设备ID:" << deviceId;
    emit deviceDiscovered(deviceId, deviceName);
}

void MusicPlayer::handleDeviceLost(const QString& deviceId)
{
    qDebug() << "\nDLNA设备丢失:" << deviceId;
    emit deviceLost(deviceId);
}

void MusicPlayer::handleDeviceConnectionChanged(bool connected)
{
    qDebug() << "\nDLNA设备连接状态变化:";
    qDebug() << "设备ID:" << m_dlnaManager->getCurrentDeviceId();
    qDebug() << "新状态:" << (connected ? "已连接" : "已断开");
    m_isDeviceConnected = connected;
    emit deviceConnectionChanged(connected);
}

void MusicPlayer::handleDevicePlaybackStateChanged(const QString& state)
{
    qDebug() << "\nDLNA设备播放状态变化:";
    qDebug() << "设备ID:" << m_dlnaManager->getCurrentDeviceId();
    qDebug() << "新状态:" << state;
    if (m_isDeviceConnected) {
        lockState();
        m_currentPlaybackState = state;
        unlockState();
        emit playbackStateChanged(state);
    }
}

void MusicPlayer::handleDeviceError(const QString& message)
{
    qDebug() << "\nDLNA设备错误:";
    qDebug() << "设备ID:" << m_dlnaManager->getCurrentDeviceId();
    qDebug() << "错误信息:" << message;
    if (m_isDeviceConnected) {
        // 如果是严重错误，考虑切换回本地播放
        qDebug() << "检查是否需要切换回本地播放...";
        emit error(tr("DLNA device error: %1").arg(message));
    }
}

void MusicPlayer::play()
{
    if (!m_playlist || m_playlist->count() == 0) {
        qDebug() << "无法播放：播放列表��空";
        return;
    }

    if (m_playlist->currentIndex() == -1) {
        m_playlist->setCurrentIndex(0);
    }

    playCurrentTrack();
}

void MusicPlayer::next()
{
    qDebug() << "切换到下一首歌曲";
    if (!m_playlist || m_playlist->count() == 0) {
        qDebug() << "播放列表为空，无法切换下一首";
        return;
    }

    int nextIndex = m_playlist->nextIndex();
    qDebug() << "当前索引:" << m_playlist->currentIndex() << "下一首索引:" << nextIndex;
    
    if (nextIndex != -1) {
        playTrack(nextIndex);
    } else {
        qDebug() << "已经是最后一首歌曲";
        stop();
    }
}

void MusicPlayer::previous()
{
    qDebug() << "切换到上一首歌曲";
    if (!m_playlist || m_playlist->count() == 0) {
        qDebug() << "播放列表为空，无法切换上一首";
        return;
    }

    int prevIndex = m_playlist->previousIndex();
    qDebug() << "当前索引:" << m_playlist->currentIndex() << "上一首索引:" << prevIndex;
    
    if (prevIndex != -1) {
        playTrack(prevIndex);
    } else {
        qDebug() << "已经是第一首歌曲";
        stop();
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
    try {
        if (!m_playlist) {
            qDebug() << "错误: 播放列表未初始化";
            emit error(tr("Playlist not initialized"));
            return;
        }

        if (m_playlist->currentIndex() == -1) {
            qDebug() << "错误: 无效的播放列表索引";
            emit error(tr("Invalid playlist index"));
            return;
        }

        MusicFile currentFile = m_playlist->at(m_playlist->currentIndex());
        if (currentFile.filePath().isEmpty()) {
            qDebug() << "错误: 无效的文件路径";
            emit error(tr("Invalid file path"));
            return;
        }

        qDebug() << "\n准备播放当前歌曲:";
        qDebug() << "文件路径:" << currentFile.filePath();
        qDebug() << "设备连接状态:" << (m_isDeviceConnected ? "已连接" : "未连接");
        if (m_isDeviceConnected) {
            if (!m_dlnaManager) {
                qDebug() << "错误: DLNA管理器未初始化";
                emit error(tr("DLNA manager not initialized"));
                return;
            }
            qDebug() << "当前DLNA设备:" << m_dlnaManager->getCurrentDeviceId();
        }
        
        // 使用QTimer延迟播放，避免状态冲突
        QTimer::singleShot(500, this, [this, currentFile]() {
            try {
                lockState();
                m_currentPosition = 0;  // 重置播放位置
                m_isProcessingNextTrack = false;  // 重置处理标志
                unlockState();
                
                QUrl fileUrl = currentFile.fileUrl();
                if (!fileUrl.isValid()) {
                    qDebug() << "错误: 无效的文件URL";
                    emit error(tr("Invalid file URL"));
                    return;
                }
                
                qDebug() << "开始播放文件:" << fileUrl.toString();
                play(fileUrl);
                emit currentSongChanged(m_playlist->currentIndex());
            } catch (const std::exception& e) {
                qDebug() << "播放过程中发生异常:" << e.what();
                emit error(tr("Playback error: %1").arg(e.what()));
            } catch (...) {
                qDebug() << "播放过程中发生未知异常";
                emit error(tr("Unknown playback error"));
            }
        });
    } catch (const std::exception& e) {
        qDebug() << "准备播放过程中发生异常:" << e.what();
        emit error(tr("Playback preparation error: %1").arg(e.what()));
    } catch (...) {
        qDebug() << "准备播放过程中发生未知异常";
        emit error(tr("Unknown playback preparation error"));
    }
}

void MusicPlayer::playTrack(int index)
{
    qDebug() << "播放索引" << index << "的歌曲";
    if (!m_playlist || index < 0 || index >= m_playlist->count()) {
        qDebug() << "无效的播放索引";
        return;
    }

    m_playlist->setCurrentIndex(index);
    playCurrentTrack();
} 