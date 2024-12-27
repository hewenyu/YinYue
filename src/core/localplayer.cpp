#include "localplayer.h"
#include <QDebug>

LocalPlayer::LocalPlayer(QObject *parent)
    : QObject(parent)
    , m_player(new QMediaPlayer(this))
    , m_currentState("Stopped")
    , m_currentPosition(0)
    , m_currentDuration(0)
    , m_currentVolume(100)
{
    // 设置初始音量
    m_player->setVolume(100);

    // 连接信号槽
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &LocalPlayer::handleMediaStatusChanged);
    connect(m_player, &QMediaPlayer::stateChanged, this, &LocalPlayer::handleStateChanged);
    connect(m_player, &QMediaPlayer::positionChanged, this, &LocalPlayer::handlePositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &LocalPlayer::handleDurationChanged);
    connect(m_player, &QMediaPlayer::volumeChanged, this, &LocalPlayer::handleVolumeChanged);
    
    // 使用Qt5风格的错误信号连接
    connect(m_player, static_cast<void(QMediaPlayer::*)(QMediaPlayer::Error)>(&QMediaPlayer::error),
            this, &LocalPlayer::handleError);
}

LocalPlayer::~LocalPlayer()
{
    stop();
}

void LocalPlayer::play(const QUrl& url)
{
    qDebug() << "\n本地播放器: 开始播放" << url.toString();
    m_player->setMedia(QMediaContent(url));
    m_player->play();
}

void LocalPlayer::pause()
{
    qDebug() << "本地播放器: 暂停播放";
    m_player->pause();
}

void LocalPlayer::stop()
{
    qDebug() << "本地播放器: 停止播放";
    m_player->stop();
}

void LocalPlayer::setVolume(int volume)
{
    qDebug() << "本地播放器: 设置音量" << volume;
    m_player->setVolume(volume);
}

void LocalPlayer::setPosition(qint64 position)
{
    qDebug() << "本地播放器: 设置播放位置" << position;
    m_player->setPosition(position);
}

QString LocalPlayer::getPlaybackState() const
{
    return m_currentState;
}

qint64 LocalPlayer::getPosition() const
{
    return m_currentPosition;
}

qint64 LocalPlayer::getDuration() const
{
    return m_currentDuration;
}

int LocalPlayer::getVolume() const
{
    return m_currentVolume;
}

void LocalPlayer::handleMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    qDebug() << "本地播放器: 媒体状态变化" << status;
    switch (status) {
        case QMediaPlayer::LoadingMedia:
            qDebug() << "  正在加载媒体...";
            break;
        case QMediaPlayer::LoadedMedia:
            qDebug() << "  媒体加载完成";
            break;
        case QMediaPlayer::InvalidMedia:
            qDebug() << "  无效的媒体";
            emit error(tr("Invalid media"));
            break;
        default:
            break;
    }
}

void LocalPlayer::handleStateChanged(QMediaPlayer::State state)
{
    QString stateStr;
    switch (state) {
        case QMediaPlayer::PlayingState:
            stateStr = "Playing";
            break;
        case QMediaPlayer::PausedState:
            stateStr = "Paused";
            break;
        case QMediaPlayer::StoppedState:
            stateStr = "Stopped";
            break;
    }
    
    qDebug() << "本地播放器: 播放状态变化" << stateStr;
    m_currentState = stateStr;
    emit playbackStateChanged(stateStr);
}

void LocalPlayer::handlePositionChanged(qint64 position)
{
    m_currentPosition = position;
    emit positionChanged(position);
}

void LocalPlayer::handleDurationChanged(qint64 duration)
{
    m_currentDuration = duration;
    emit durationChanged(duration);
}

void LocalPlayer::handleVolumeChanged(int volume)
{
    m_currentVolume = volume;
    emit volumeChanged(volume);
}

void LocalPlayer::handleError(QMediaPlayer::Error error)
{
    qDebug() << "本地播放器错误:";
    qDebug() << "  错误类型:" << error;
    qDebug() << "  错误描述:" << m_player->errorString();
    emit this->error(m_player->errorString());
} 