#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QModelIndex>
#include <QFileSystemWatcher>
#include "src/core/musicplayer.h"
#include "src/models/playlist.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // 播放控制
    void on_playButton_clicked();
    void on_previousButton_clicked();
    void on_nextButton_clicked();
    void on_volumeSlider_valueChanged(int value);
    void on_progressSlider_sliderMoved(int position);
    
    // 文件菜单
    void on_actionOpenFile_triggered();
    void on_actionOpenFolder_triggered();
    void on_actionExit_triggered();
    
    // 播放列表
    void on_libraryWidget_doubleClicked(const QModelIndex &index);
    void on_playlistWidget_doubleClicked(const QModelIndex &index);
    void on_clearPlaylistButton_clicked();
    void on_removeSelectedButton_clicked();
    
    // 播放器状态更新
    void updatePlaybackState(QMediaPlayer::State state);
    void updatePosition(qint64 position);
    void updateDuration(qint64 duration);
    void handleError(const QString &error);
    
    // 文件监控
    void onDirectoryChanged(const QString &path);
    void onFileChanged(const QString &path);

private:
    void setupConnections();
    void updateTimeLabel(QLabel *label, qint64 time);
    void loadFile(const QString &filePath);
    void loadFolder(const QString &folderPath);
    void refreshMusicLibrary();
    void addToPlaylist(const MusicFile &file);
    void updateCurrentSong(const MusicFile &file);

private:
    Ui::MainWindow *ui;
    MusicPlayer *m_player;
    Playlist *m_playlist;
    bool m_isPlaying;
    QString m_currentMusicFolder;
    QFileSystemWatcher *m_fileWatcher;
    QMap<QString, MusicFile> m_musicLibrary; // 文件路径到音乐文件的映射
};

#endif // MAINWINDOW_H
