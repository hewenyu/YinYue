#ifndef MUSICPLAYER_H
#define MUSICPLAYER_H

#include <QObject>
#include <QUrl>
#include "localplayer.h"
#include "dlnamanager.h"
#include "models/playlist.h"

class MusicPlayer : public QObject
{
    Q_OBJECT

public:
    explicit MusicPlayer(QObject *parent = nullptr);
    ~MusicPlayer();

    // 播放控制
    void play(const QUrl& url);
    void play();  // 播放当前曲目
    void pause();
    void stop();
    void next();
    void previous();
    void setVolume(int volume);
    void seekTo(qint64 position);

    // 播放列表控制
    void setPlaylist(Playlist* playlist);
    Playlist* playlist() const { return m_playlist; }
    void setPlayMode(Playlist::PlayMode mode);
    Playlist::PlayMode playMode() const;

    // DLNA设备管理
    void startDeviceDiscovery();
    void stopDeviceDiscovery();
    QList<DLNADevice> getAvailableDevices() const;
    bool connectToDevice(const QString& deviceId);
    void disconnectFromDevice();
    bool isDeviceConnected() const;
    QString getCurrentDeviceId() const;

    // 播放状态
    QString getPlaybackState() const;
    qint64 getPosition() const;
    qint64 getDuration() const;
    int getVolume() const;

signals:
    // 设备相关信号
    void deviceDiscovered(const QString& deviceId, const QString& deviceName);
    void deviceLost(const QString& deviceId);
    void deviceConnectionChanged(bool connected);

    // 播放状态信号
    void playbackStateChanged(const QString& state);
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void volumeChanged(int volume);
    void error(const QString& message);

    // 播放列表相关信号
    void currentSongChanged(int index);
    void playModeChanged(Playlist::PlayMode mode);

private slots:
    // 本地播放器状态处理
    void handleLocalPlaybackStateChanged(const QString& state);
    void handleLocalPositionChanged(qint64 position);
    void handleLocalDurationChanged(qint64 duration);
    void handleLocalVolumeChanged(int volume);
    void handleLocalError(const QString& message);

    // DLNA设备状态处理
    void handleDeviceDiscovered(const QString& deviceId, const QString& deviceName);
    void handleDeviceLost(const QString& deviceId);
    void handleDeviceConnectionChanged(bool connected);
    void handleDevicePlaybackStateChanged(const QString& state);
    void handleDeviceError(const QString& message);

    // 播放列表处理
    void handlePlaylistChanged();

private:
    void playCurrentTrack();
    void playTrack(int index);

    LocalPlayer* m_localPlayer;
    DLNAManager* m_dlnaManager;
    Playlist* m_playlist;
    QString m_currentPlaybackState;
    qint64 m_currentPosition;
    qint64 m_currentDuration;
    int m_currentVolume;
    bool m_isDeviceConnected;
};

#endif // MUSICPLAYER_H 