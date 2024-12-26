#include "musicplayer.h"

MusicPlayer::MusicPlayer(QObject *parent)
    : QObject(parent)
    , m_player(new QMediaPlayer(this))
{
    // 连接信号
    connect(m_player, &QMediaPlayer::stateChanged, this, &MusicPlayer::stateChanged);
    connect(m_player, &QMediaPlayer::positionChanged, this, &MusicPlayer::positionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &MusicPlayer::durationChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &MusicPlayer::mediaStatusChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &MusicPlayer::onMediaStatusChanged);
    
    // 使用 Qt5 风格的错误信号连接
    connect(m_player, static_cast<void(QMediaPlayer::*)(QMediaPlayer::Error)>(&QMediaPlayer::error),
            this, [this](QMediaPlayer::Error error) {
                emit errorOccurred(m_player->errorString());
            });

    // 设置默认音量
    m_player->setVolume(50);
}

MusicPlayer::~MusicPlayer()
{
}

void MusicPlayer::play()
{
    m_player->play();
}

void MusicPlayer::pause()
{
    m_player->pause();
}

void MusicPlayer::stop()
{
    m_player->stop();
}

void MusicPlayer::next()
{
    // TODO: 实现下一首功能
}

void MusicPlayer::previous()
{
    // TODO: 实现上一首功能
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

void MusicPlayer::onPlaylistChanged()
{
    // 播放列表添加歌曲不影响播放器播放音乐
    // 只有在播放列表为空时才停止播放
    if (m_player->state() != QMediaPlayer::StoppedState && m_playlist && m_playlist->count() == 0) {
        stop();
        m_player->setMedia(QMediaContent());  // 清除当前媒体
    }
}

void MusicPlayer::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::EndOfMedia && m_playlist) {
        switch (m_playlist->playMode()) {
            case Playlist::Sequential: {
                // 顺序播放模式：如果不是最后一首，播放下一首
                int nextIndex = m_playlist->currentIndex() + 1;
                if (nextIndex < m_playlist->count()) {
                    m_playlist->setCurrentIndex(nextIndex);
                    MusicFile nextFile = m_playlist->at(nextIndex);
                    setSource(nextFile.fileUrl());
                    emit currentSongChanged(nextIndex);  // 发送歌曲改变信号
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
                    emit currentSongChanged(nextIndex);  // 发送歌曲改变信号
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
                    emit currentSongChanged(nextIndex);  // 发送歌曲改变信号
                    play();
                }
                break;
            }
        }
    }
} 