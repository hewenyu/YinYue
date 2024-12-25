#ifndef MUSICPLAYER_H
#define MUSICPLAYER_H

#include <QObject>
#include <QMediaPlayer>
#include <QUrl>
#include <QMediaContent>

class MusicPlayer : public QObject
{
    Q_OBJECT
public:
    explicit MusicPlayer(QObject *parent = nullptr);
    ~MusicPlayer();

    // 基本控制函数
    void play();
    void pause();
    void stop();
    void next();
    void previous();
    void setVolume(int volume);
    void setPosition(qint64 position);
    void setSource(const QUrl &source);

    // 获取状态
    QMediaPlayer::State state() const;
    qint64 position() const;
    qint64 duration() const;
    int volume() const;

signals:
    void stateChanged(QMediaPlayer::State state);
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void volumeChanged(int volume);
    void mediaStatusChanged(QMediaPlayer::MediaStatus status);
    void errorOccurred(const QString &error);

private:
    QMediaPlayer *m_player;
};

#endif // MUSICPLAYER_H 