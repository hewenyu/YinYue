#ifndef LOCALPLAYER_H
#define LOCALPLAYER_H

#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QUrl>

class LocalPlayer : public QObject
{
    Q_OBJECT

public:
    explicit LocalPlayer(QObject *parent = nullptr);
    ~LocalPlayer();

    // 播放控制
    void play(const QUrl& url);
    void pause();
    void stop();
    void setVolume(int volume);
    void setPosition(qint64 position);

    // 获取状态
    QString getPlaybackState() const;
    qint64 getPosition() const;
    qint64 getDuration() const;
    int getVolume() const;

signals:
    void playbackStateChanged(const QString& state);
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void volumeChanged(int volume);
    void error(const QString& message);

private slots:
    void handleMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void handleStateChanged(QMediaPlayer::State state);
    void handlePositionChanged(qint64 position);
    void handleDurationChanged(qint64 duration);
    void handleVolumeChanged(int volume);
    void handleError(QMediaPlayer::Error error);

private:
    QMediaPlayer* m_player;
    QString m_currentState;
    qint64 m_currentPosition;
    qint64 m_currentDuration;
    int m_currentVolume;
};

#endif // LOCALPLAYER_H 