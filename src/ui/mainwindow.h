#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QModelIndex>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QCloseEvent>
#include "core/musicplayer.h"
#include "models/playlist.h"
#include "models/lyric.h"

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
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

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
    
    // 歌词更新
    void updateLyric(qint64 position);

private:
    void setupConnections();
    void updateTimeLabel(QLabel *label, qint64 time);
    void loadFile(const QString &filePath);
    void loadFolder(const QString &folderPath);
    void refreshMusicLibrary();
    void addToPlaylist(const MusicFile &file);
    void updateCurrentSong(const MusicFile &file, bool updatePlayer = true);
    void loadLyric(const QString &musicFilePath);
    void adjustLyricFontSize();
    void loadSettings();
    void saveSettings();
    
    // 新增：进度条相关
    QTimer *m_progressTimer;        // 用于平滑更新进度条
    qint64 m_lastPosition;          // 上次播放位置
    bool m_isUserSeeking;           // 用户是否正在拖动进度条
    
    void savePlaybackState();       // 保存播放状态
    void restorePlaybackState();    // 恢复播放状态
    void startProgressTimer();      // 启动进度条更新定时器
    void stopProgressTimer();       // 停止进度条更新定时器

private:
    Ui::MainWindow *ui;
    MusicPlayer *m_player;
    Playlist *m_playlist;
    Lyric *m_lyric;
    bool m_isPlaying;
    QFileSystemWatcher *m_fileWatcher;
    QString m_currentMusicFolder;
    QMap<QString, MusicFile> m_musicLibrary;
};

#endif // MAINWINDOW_H
