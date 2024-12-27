#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QModelIndex>
#include <QMessageBox>
#include <QTime>
#include <QCloseEvent>
#include <QFileDialog>
#include <QStandardPaths>
#include "core/musicplayer.h"
#include "models/playlist.h"
#include "gui/dlnadevicedialog.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    // 播放控制
    void togglePlayback();
    void seekPosition(int position);
    
    // 播放器状态更新
    void updatePlaybackState(const QString& state);
    void updatePosition(qint64 position);
    void updateDuration(qint64 duration);
    void updateCurrentSong(int index);
    void handleError(const QString& message);
    
    // 播放列表控制
    void playSelectedTrack(const QModelIndex& index);
    void addFiles();
    void removeSelectedTracks();
    void clearPlaylist();
    void togglePlayMode();
    void on_playModeButton_clicked();
    
    // DLNA 相关
    void showDLNADialog();
    void updateDLNAStatus(bool connected);

private:
    void setupConnections();
    void loadSettings();
    void saveSettings();
    void savePlaybackState();
    void restorePlaybackState();
    void updatePlayModeButton(Playlist::PlayMode mode);
    QString getPlayModeText(Playlist::PlayMode mode);

private:
    Ui::MainWindow *ui;
    MusicPlayer *m_player;
    Playlist *m_playlist;
    QString m_currentMusicFolder;
};

#endif // MAINWINDOW_H
