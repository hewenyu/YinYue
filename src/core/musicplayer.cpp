#include "musicplayer.h"
#include <QEventLoop>
#include <QTimer>
#include <QDebug>
#include <QThread>

MusicPlayer::MusicPlayer(QObject *parent)
    : QObject(parent)
    , m_player(new QMediaPlayer(this))
    , m_playlist(nullptr)
    , m_eventLoop(nullptr)
{
    // 连接信号
    connect(m_player, &QMediaPlayer::stateChanged, this, &MusicPlayer::stateChanged);
    connect(m_player, &QMediaPlayer::stateChanged, this, &MusicPlayer::onStateChanged);
    connect(m_player, &QMediaPlayer::positionChanged, this, &MusicPlayer::positionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &MusicPlayer::durationChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &MusicPlayer::mediaStatusChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &MusicPlayer::onMediaStatusChanged);
    
    // 使用 Qt5 风格的错误信号连接
    connect(m_player, static_cast<void(QMediaPlayer::*)(QMediaPlayer::Error)>(&QMediaPlayer::error),
            this, [this](QMediaPlayer::Error error) {
                emit errorOccurred(m_player->errorString());
            });

    // 设置默认音量��100
    m_player->setVolume(100);
}

MusicPlayer::~MusicPlayer()
{
    if (m_eventLoop) {
        m_eventLoop->quit();
        delete m_eventLoop;
    }
}

bool MusicPlayer::waitForMediaLoaded(int timeout)
{
    qDebug() << "Waiting for media to load, current status:" << m_player->mediaStatus()
             << "media:" << (m_player->media().isNull() ? "null" : "valid");
    
    if (m_eventLoop) {
        m_eventLoop->quit();
        delete m_eventLoop;
    }
    m_eventLoop = new QEventLoop(this);
    
    bool mediaLoaded = false;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, m_eventLoop, [this, &mediaLoaded]() {
        qDebug() << "Media load timeout, status:" << m_player->mediaStatus();
        mediaLoaded = false;
        if (m_eventLoop) {
            m_eventLoop->quit();
        }
    });
    timer.start(timeout);

    QMetaObject::Connection conn = connect(m_player, &QMediaPlayer::mediaStatusChanged, 
        [this, &mediaLoaded](QMediaPlayer::MediaStatus status) {
            qDebug() << "Media status changed during wait:" << status;
            if (status == QMediaPlayer::LoadedMedia) {
                mediaLoaded = true;
                if (m_eventLoop) {
                    m_eventLoop->quit();
                }
            } else if (status == QMediaPlayer::InvalidMedia) {
                mediaLoaded = false;
                if (m_eventLoop) {
                    m_eventLoop->quit();
                }
            }
        });
    
    m_eventLoop->exec();
    
    // 断开连接
    disconnect(conn);
    
    qDebug() << "Media load wait finished, status:" << m_player->mediaStatus()
             << "loaded:" << mediaLoaded;
    return mediaLoaded;
}

bool MusicPlayer::waitForState(QMediaPlayer::State targetState, int timeout)
{
    qDebug() << "Waiting for state:" << targetState << ", current state:" << m_player->state()
             << "media status:" << m_player->mediaStatus();
    
    if (m_player->state() == targetState) {
        qDebug() << "Already in target state";
        return true;
    }
    
    if (m_eventLoop) {
        m_eventLoop->quit();
        delete m_eventLoop;
    }
    m_eventLoop = new QEventLoop(this);
    
    bool stateReached = false;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, m_eventLoop, [this, &stateReached]() {
        qDebug() << "State change timeout, current state:" << m_player->state();
        stateReached = false;
        if (m_eventLoop) {
            m_eventLoop->quit();
        }
    });
    timer.start(timeout);

    QMetaObject::Connection conn = connect(m_player, &QMediaPlayer::stateChanged,
        [this, targetState, &stateReached](QMediaPlayer::State state) {
            qDebug() << "State changed during wait:" << state;
            if (state == targetState) {
                stateReached = true;
                if (m_eventLoop) {
                    m_eventLoop->quit();
                }
            }
        });
    
    m_eventLoop->exec();
    
    // 断开连接
    disconnect(conn);
    
    qDebug() << "State wait finished, current state:" << m_player->state()
             << "reached:" << stateReached;
    return stateReached;
}

void MusicPlayer::play()
{
    if (!m_playlist || m_playlist->count() == 0) {
        qDebug() << "Cannot play: playlist is empty or null";
        return;
    }

    qDebug() << "Play requested, current state:" << m_player->state()
             << "media status:" << m_player->mediaStatus()
             << "current index:" << (m_playlist ? m_playlist->currentIndex() : -1);

    // 如果是第一次播放，设置当前索引为0
    if (m_playlist->currentIndex() == -1) {
        qDebug() << "First play, setting current index to 0";
        m_playlist->setCurrentIndex(0);
    }

    if (m_player->state() == QMediaPlayer::StoppedState || 
        m_player->mediaStatus() == QMediaPlayer::NoMedia ||
        m_player->mediaStatus() == QMediaPlayer::InvalidMedia) {
        qDebug() << "Loading media from playlist at index:" << m_playlist->currentIndex();
        MusicFile currentFile = m_playlist->at(m_playlist->currentIndex());
        m_player->setMedia(QMediaContent(currentFile.fileUrl()));
        
        if (waitForMediaLoaded()) {
            qDebug() << "Media loaded successfully, starting playback";
            m_player->play();
            if (!waitForState(QMediaPlayer::PlayingState)) {
                qDebug() << "Failed to enter playing state";
            }
        } else {
            qDebug() << "Failed to load media";
        }
    } else {
        qDebug() << "Resuming playback";
        m_player->play();
        if (!waitForState(QMediaPlayer::PlayingState)) {
            qDebug() << "Failed to enter playing state";
        }
    }
}

void MusicPlayer::pause()
{
    qDebug() << "Pause requested, current state:" << m_player->state()
             << "media status:" << m_player->mediaStatus();

    if (m_player->state() == QMediaPlayer::PlayingState) {
        m_player->pause();
        if (!waitForState(QMediaPlayer::PausedState)) {
            qDebug() << "Failed to enter paused state";
            // 如果暂停失败，尝试停止播放
            stop();
        }
    }
}

void MusicPlayer::stop()
{
    qDebug() << "Stop requested, current state:" << m_player->state()
             << "media status:" << m_player->mediaStatus();

    if (m_player->state() != QMediaPlayer::StoppedState) {
        m_player->stop();
        if (!waitForState(QMediaPlayer::StoppedState)) {
            qDebug() << "Failed to enter stopped state";
            // 如���停止失败，尝试重置媒体
            m_player->setMedia(QMediaContent());
            waitForState(QMediaPlayer::StoppedState);
        }
    }
}

void MusicPlayer::next()
{
    qDebug() << "Next track requested, current state:" << m_player->state()
             << "media status:" << m_player->mediaStatus()
             << "current index:" << (m_playlist ? m_playlist->currentIndex() : -1);

    if (!m_playlist || m_playlist->count() == 0) {
        qDebug() << "Cannot play next: playlist is empty or null";
        return;
    }

    // 如果是第一次播放，设置当前索引为0
    if (m_playlist->currentIndex() == -1) {
        qDebug() << "First play, setting current index to 0";
        m_playlist->setCurrentIndex(0);
    }

    int nextIndex = m_playlist->nextIndex();
    qDebug() << "Next index:" << nextIndex;
    
    if (nextIndex >= 0) {
        // 先停止当前播放
        stop();
        QThread::msleep(500);  // 给一些时间让播放器完全停止
        
        qDebug() << "Setting current index to:" << nextIndex;
        m_playlist->setCurrentIndex(nextIndex);
        MusicFile nextFile = m_playlist->at(nextIndex);
        
        qDebug() << "Loading next track:" << nextFile.filePath();
        m_player->setMedia(QMediaContent(nextFile.fileUrl()));
        
        // 等待媒体加载完成
        if (waitForMediaLoaded()) {
            qDebug() << "Next track loaded successfully, starting playback";
            m_player->play();
            if (!waitForState(QMediaPlayer::PlayingState)) {
                qDebug() << "Failed to start playing next track";
            }
        } else {
            qDebug() << "Failed to load next track";
        }
    } else {
        qDebug() << "No next track available";
    }
}

void MusicPlayer::previous()
{
    qDebug() << "Previous track requested, current state:" << m_player->state()
             << "media status:" << m_player->mediaStatus()
             << "current index:" << (m_playlist ? m_playlist->currentIndex() : -1);

    if (!m_playlist || m_playlist->count() == 0) {
        qDebug() << "Cannot play previous: playlist is empty or null";
        return;
    }

    // 如果第一次播放，设置当前索引为0
    if (m_playlist->currentIndex() == -1) {
        qDebug() << "First play, setting current index to 0";
        m_playlist->setCurrentIndex(0);
    }

    int prevIndex = m_playlist->previousIndex();
    qDebug() << "Previous index:" << prevIndex;
    
    if (prevIndex >= 0) {
        // 先停止当前播放
        stop();
        QThread::msleep(500);  // 给一些时间让播放器完全停止
        
        qDebug() << "Setting current index to:" << prevIndex;
        m_playlist->setCurrentIndex(prevIndex);
        MusicFile prevFile = m_playlist->at(prevIndex);
        
        qDebug() << "Loading previous track:" << prevFile.filePath();
        m_player->setMedia(QMediaContent(prevFile.fileUrl()));
        
        // 等待媒体加载完成
        if (waitForMediaLoaded()) {
            qDebug() << "Previous track loaded successfully, starting playback";
            m_player->play();
            if (!waitForState(QMediaPlayer::PlayingState)) {
                qDebug() << "Failed to start playing previous track";
            }
        } else {
            qDebug() << "Failed to load previous track";
        }
    } else {
        qDebug() << "No previous track available";
    }
}

void MusicPlayer::onStateChanged(QMediaPlayer::State state)
{
    qDebug() << "State changed to:" << state;
    if (m_eventLoop) {
        m_eventLoop->quit();
    }
}

void MusicPlayer::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    qDebug() << "Media status changed to:" << status;
    if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::InvalidMedia) {
        if (m_eventLoop) {
            m_eventLoop->quit();
        }
    }

    if (status == QMediaPlayer::EndOfMedia && m_playlist) {
        switch (m_playlist->playMode()) {
            case Playlist::Sequential: {
                // 顺序播放模式：如果不是最后一首，播放下一首
                int nextIndex = m_playlist->currentIndex() + 1;
                if (nextIndex < m_playlist->count()) {
                    m_playlist->setCurrentIndex(nextIndex);
                    MusicFile nextFile = m_playlist->at(nextIndex);
                    setSource(nextFile.fileUrl());
                    emit currentSongChanged(nextIndex);
                    play();
                } else {
                    // 最后一首播放完就停止
                    stop();
                }
                break;
            }
            case Playlist::Random: {
                // 随机播放模式：随机选择一首
                int nextIndex = m_playlist->nextIndex();
                if (nextIndex != -1) {
                    m_playlist->setCurrentIndex(nextIndex);
                    MusicFile nextFile = m_playlist->at(nextIndex);
                    setSource(nextFile.fileUrl());
                    emit currentSongChanged(nextIndex);
                    play();
                }
                break;
            }
            case Playlist::RepeatOne: {
                // 单曲循环模式：重新播放当前歌曲
                play();
                break;
            }
            case Playlist::RepeatAll: {
                // 列表循环模式：播放下一首（如果是最后一首则回到第一首）
                int nextIndex = m_playlist->nextIndex();
                if (nextIndex != -1) {
                    m_playlist->setCurrentIndex(nextIndex);
                    MusicFile nextFile = m_playlist->at(nextIndex);
                    setSource(nextFile.fileUrl());
                    emit currentSongChanged(nextIndex);
                    play();
                }
                break;
            }
        }
    }
}

void MusicPlayer::setVolume(int volume)
{
    m_player->setVolume(volume);
    emit volumeChanged(volume);
}

void MusicPlayer::setPosition(qint64 position)
{
    m_player->setPosition(position);
}

void MusicPlayer::setSource(const QUrl &source)
{
    m_player->setMedia(QMediaContent(source));
}

Playlist::PlayMode MusicPlayer::playMode() const
{
    return m_playlist ? m_playlist->playMode() : Playlist::Sequential;
}

void MusicPlayer::setPlayMode(Playlist::PlayMode mode)
{
    if (m_playlist) {
        m_playlist->setPlayMode(mode);
        emit playModeChanged(mode);
    }
}

void MusicPlayer::togglePlayMode()
{
    if (!m_playlist) return;
    
    Playlist::PlayMode currentMode = m_playlist->playMode();
    Playlist::PlayMode newMode;
    
    // 循环切换播放模式：顺序 -> 随机 -> 单曲循环 -> 列表循环 -> 顺序
    switch (currentMode) {
        case Playlist::Sequential:
            newMode = Playlist::Random;
            break;
        case Playlist::Random:
            newMode = Playlist::RepeatOne;
            break;
        case Playlist::RepeatOne:
            newMode = Playlist::RepeatAll;
            break;
        case Playlist::RepeatAll:
            newMode = Playlist::Sequential;
            break;
        default:
            newMode = Playlist::Sequential;
    }
    
    setPlayMode(newMode);
}

void MusicPlayer::onPlaylistChanged()
{
    // 播放列表添加歌曲不影响播放器播放音乐
    // 只有在播放列表为空时才停止播放
    if (m_player->state() != QMediaPlayer::StoppedState && m_playlist && m_playlist->count() == 0) {
        stop();
        m_player->setMedia(QMediaContent());  // 清除当前媒体
    }
}

QMediaPlayer::State MusicPlayer::state() const
{
    return m_player->state();
}

qint64 MusicPlayer::position() const
{
    return m_player->position();
}

qint64 MusicPlayer::duration() const
{
    return m_player->duration();
}

int MusicPlayer::volume() const
{
    return m_player->volume();
} 