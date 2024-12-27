#ifndef MUSICPLAYER_H
#define MUSICPLAYER_H

#include <QObject>
#include <QMediaPlayer>
#include <QUrl>
#include <QMediaContent>
#include <QEventLoop>
#include "models/playlist.h"
#include "models/dlnadevice.h"
#include "core/dlnamanager.h"

class MusicPlayer : public QObject
{
    Q_OBJECT
public:
    explicit MusicPlayer(QObject *parent = nullptr);
    ~MusicPlayer();

    // DLNA 相关功能
    void startDLNADiscovery();
    void stopDLNADiscovery();
    QList<DLNADevice> getAvailableDLNADevices() const;
    bool connectToDLNADevice(const QString& deviceId);
    void disconnectFromDLNADevice();
    bool isDLNAConnected() const;
    QString getCurrentDLNADevice() const;

    // 基本控制函数
    void play();
    void pause();
    void stop();
    void next();
    void previous();
    void setVolume(int volume);
    void setPosition(qint64 position);
    void setSource(const QUrl &source);
    void setPlaylist(Playlist *playlist) { m_playlist = playlist; }
    
    // 播放模式控制
    Playlist::PlayMode playMode() const;
    void setPlayMode(Playlist::PlayMode mode);
    void togglePlayMode();

    // 获取状态
    QMediaPlayer::State state() const;
    qint64 position() const;
    qint64 duration() const;
    int volume() const;

public slots:
    void onPlaylistChanged();

private slots:
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onStateChanged(QMediaPlayer::State state);
    void onDLNAPlaybackStateChanged(const QString& state);

signals:
    void stateChanged(QMediaPlayer::State state);
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void volumeChanged(int volume);
    void mediaStatusChanged(QMediaPlayer::MediaStatus status);
    void playModeChanged(Playlist::PlayMode mode);
    void errorOccurred(const QString &error);
    void currentSongChanged(int index);
    
    // DLNA 相关信号
    void dlnaDeviceDiscovered(const QString& deviceId, const QString& deviceName);
    void dlnaDeviceLost(const QString& deviceId);
    void dlnaConnectionStateChanged(bool connected);
    void dlnaError(const QString& error);

private:
    // 辅助函数：等待媒体加载或状态变化
    bool waitForMediaLoaded(int timeout = 5000);
    bool waitForState(QMediaPlayer::State targetState, int timeout = 5000);
    bool switchToTrack(int index, const QString& operation);

    QMediaPlayer *m_player;
    Playlist *m_playlist;
    QEventLoop *m_eventLoop;
    
    // DLNA 相关私有成员
    DLNAManager* m_dlnaManager;
    bool m_useDLNA;
    QString m_currentDLNADevice;
    QList<DLNADevice> m_availableDLNADevices;
    bool m_dlnaConnected;
};

#endif // MUSICPLAYER_H 