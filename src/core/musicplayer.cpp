#include "musicplayer.h"
#include <QEventLoop>
#include <QTimer>

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

    // 设置默认音量为100
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
    if (m_eventLoop) {
        m_eventLoop->quit();
        delete m_eventLoop;
    }
    m_eventLoop = new QEventLoop(this);
    
    QTimer::singleShot(timeout, m_eventLoop, &QEventLoop::quit);
    m_eventLoop->exec();
    
    return m_player->mediaStatus() == QMediaPlayer::LoadedMedia;
}

bool MusicPlayer::waitForState(QMediaPlayer::State targetState, int timeout)
{
    if (m_player->state() == targetState) {
        return true;
    }
    
    if (m_eventLoop) {
        m_eventLoop->quit();
        delete m_eventLoop;
    }
    m_eventLoop = new QEventLoop(this);
    
    QTimer::singleShot(timeout, m_eventLoop, &QEventLoop::quit);
    m_eventLoop->exec();
    
    return m_player->state() == targetState;
}

void MusicPlayer::play()
{
    if (!m_playlist || m_playlist->count() == 0) {
        return;
    }

    if (m_player->state() == QMediaPlayer::StoppedState) {
        // 如果是停止状态，加载当前歌曲
        MusicFile currentFile = m_playlist->at(m_playlist->currentIndex());
        m_player->setMedia(QMediaContent(currentFile.fileUrl()));
        
        // 等待媒体加载完成
        if (waitForMediaLoaded()) {
            m_player->play();
            waitForState(QMediaPlayer::PlayingState);
        }
    } else {
        m_player->play();
        waitForState(QMediaPlayer::PlayingState);
    }
}

void MusicPlayer::pause()
{
    if (m_player->state() == QMediaPlayer::PlayingState) {
        m_player->pause();
        waitForState(QMediaPlayer::PausedState);
    }
}

void MusicPlayer::stop()
{
    if (m_player->state() != QMediaPlayer::StoppedState) {
        m_player->stop();
        waitForState(QMediaPlayer::StoppedState);
    }
}

void MusicPlayer::next()
{
    if (!m_playlist || m_playlist->count() == 0) {
        return;
    }

    // 如果是第一次播放，设置当前索引为0
    if (m_playlist->currentIndex() == -1) {
        m_playlist->setCurrentIndex(0);
    }

    int nextIndex = m_playlist->nextIndex();
    if (nextIndex >= 0) {
        stop();  // 先停止当前播放
        
        m_playlist->setCurrentIndex(nextIndex);
        MusicFile nextFile = m_playlist->at(nextIndex);
        m_player->setMedia(QMediaContent(nextFile.fileUrl()));
        
        // 等待媒体加载完成
        if (waitForMediaLoaded()) {
            m_player->play();
            waitForState(QMediaPlayer::PlayingState);
        }
    }
}

void MusicPlayer::previous()
{
    if (!m_playlist || m_playlist->count() == 0) {
        return;
    }

    // 如果是第一次播放，设置当前索引为0
    if (m_playlist->currentIndex() == -1) {
        m_playlist->setCurrentIndex(0);
    }

    int prevIndex = m_playlist->previousIndex();
    if (prevIndex >= 0) {
        stop();  // 先停止当前播放
        
        m_playlist->setCurrentIndex(prevIndex);
        MusicFile prevFile = m_playlist->at(prevIndex);
        m_player->setMedia(QMediaContent(prevFile.fileUrl()));
        
        // 等待媒体加载完成
        if (waitForMediaLoaded()) {
            m_player->play();
            waitForState(QMediaPlayer::PlayingState);
        }
    }
}

void MusicPlayer::onStateChanged(QMediaPlayer::State state)
{
    if (m_eventLoop) {
        m_eventLoop->quit();
    }
}

void MusicPlayer::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
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