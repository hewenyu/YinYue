#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
#include <QProgressDialog>
#include <QDebug>
#include <QSettings>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_player(new MusicPlayer(this))
    , m_playlist(new Playlist(this))
{
    ui->setupUi(this);
    
    // 设置播放列表
    m_player->setPlaylist(m_playlist);
    
    // 设置初始音量
    ui->volumeSlider->setValue(m_player->getVolume());
    
    // 设置播放进度条范围
    ui->positionSlider->setRange(0, 0);
    
    // 设置播放列表视图
    ui->playlistView->setModel(m_playlist);
    
    // 设置播放模式按钮图标
    updatePlayModeButton(m_player->playMode());
    
    // 连接信号和槽
    setupConnections();
    
    // 更新界面状态
    updatePlaybackState(m_player->getPlaybackState());
    updatePosition(m_player->getPosition());
    updateDLNAStatus(m_player->isDeviceConnected());
}

MainWindow::~MainWindow()
{
    // 保存配置
    saveSettings();
    delete ui;
}

void MainWindow::setupConnections()
{
    // 播放控制按钮
    connect(ui->playButton, &QPushButton::clicked, this, &MainWindow::togglePlayback);
    connect(ui->stopButton, &QPushButton::clicked, m_player, &MusicPlayer::stop);
    connect(ui->previousButton, &QPushButton::clicked, m_player, &MusicPlayer::previous);
    connect(ui->nextButton, &QPushButton::clicked, m_player, &MusicPlayer::next);
    
    // 播放进度和音量控制
    connect(ui->positionSlider, &QSlider::sliderMoved, this, &MainWindow::seekPosition);
    connect(ui->volumeSlider, &QSlider::valueChanged, m_player, &MusicPlayer::setVolume);
    
    // 播放器状态更新
    connect(m_player, &MusicPlayer::playbackStateChanged, this, &MainWindow::updatePlaybackState);
    connect(m_player, &MusicPlayer::positionChanged, this, &MainWindow::updatePosition);
    connect(m_player, &MusicPlayer::durationChanged, this, &MainWindow::updateDuration);
    connect(m_player, &MusicPlayer::volumeChanged, ui->volumeSlider, &QSlider::setValue);
    connect(m_player, &MusicPlayer::currentSongChanged, this, &MainWindow::updateCurrentSong);
    connect(m_player, &MusicPlayer::error, this, &MainWindow::handleError);
    
    // 播放列表控制
    connect(ui->playlistView, &QListView::doubleClicked, this, &MainWindow::playSelectedTrack);
    connect(ui->addButton, &QPushButton::clicked, this, &MainWindow::addFiles);
    connect(ui->removeButton, &QPushButton::clicked, this, &MainWindow::removeSelectedTracks);
    connect(ui->clearButton, &QPushButton::clicked, this, &MainWindow::clearPlaylist);
    connect(ui->playModeButton, &QPushButton::clicked, this, &MainWindow::togglePlayMode);
    
    // DLNA设备控制
    connect(ui->dlnaButton, &QPushButton::clicked, this, &MainWindow::showDLNADialog);
    connect(m_player, &MusicPlayer::deviceConnectionChanged, this, &MainWindow::updateDLNAStatus);
}

void MainWindow::updatePlaybackState(const QString& state)
{
    if (state == "Playing") {
        ui->playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
        ui->playButton->setToolTip(tr("Pause"));
    } else {
        ui->playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        ui->playButton->setToolTip(tr("Play"));
    }
}

void MainWindow::updatePosition(qint64 position)
{
    if (!ui->positionSlider->isSliderDown()) {
        ui->positionSlider->setValue(position);
    }
    
    // 更新时间标签
    QTime currentTime = QTime(0, 0).addMSecs(position);
    ui->currentTimeLabel->setText(currentTime.toString("mm:ss"));
}

void MainWindow::updateDuration(qint64 duration)
{
    ui->positionSlider->setMaximum(duration);
    
    // 更新时间标签
    QTime totalTime = QTime(0, 0).addMSecs(duration);
    ui->totalTimeLabel->setText(totalTime.toString("mm:ss"));
}

void MainWindow::seekPosition(int position)
{
    m_player->seekTo(position);
}

void MainWindow::togglePlayback()
{
    if (m_player->getPlaybackState() == "Playing") {
        m_player->pause();
    } else {
        m_player->play();
    }
}

void MainWindow::updateCurrentSong(int index)
{
    if (index >= 0 && m_playlist) {
        MusicFile currentFile = m_playlist->at(index);
        QString displayName = currentFile.title();
        if (displayName.isEmpty()) {
            QFileInfo fileInfo(currentFile.filePath());
            displayName = fileInfo.fileName();
        }
        ui->currentSongLabel->setText(displayName);
        setWindowTitle(tr("%1 - YinYue").arg(displayName));
    } else {
        ui->currentSongLabel->setText(tr("No song playing"));
        setWindowTitle(tr("YinYue"));
    }
}

void MainWindow::handleError(const QString& message)
{
    QMessageBox::warning(this, tr("Error"), message);
}

void MainWindow::showDLNADialog()
{
    DLNADeviceDialog dialog(m_player, this);
    dialog.exec();
}

void MainWindow::updateDLNAStatus(bool connected)
{
    if (connected) {
        ui->dlnaButton->setIcon(QIcon(":/icons/dlna_connected.png"));
        ui->dlnaButton->setToolTip(tr("Connected to DLNA device"));
    } else {
        ui->dlnaButton->setIcon(QIcon(":/icons/dlna.png"));
        ui->dlnaButton->setToolTip(tr("Connect to DLNA device"));
    }
}

void MainWindow::loadSettings()
{
    QSettings settings("YinYue", "MusicPlayer");
    
    // 加载音量
    int volume = settings.value("volume", 50).toInt();
    ui->volumeSlider->setValue(volume);
    m_player->setVolume(volume);
    
    // 加载播放模式
    int playMode = settings.value("playMode", static_cast<int>(Playlist::Sequential)).toInt();
    m_player->setPlayMode(static_cast<Playlist::PlayMode>(playMode));
    updatePlayModeButton(static_cast<Playlist::PlayMode>(playMode));
    
    // 加载播放列表
    int size = settings.beginReadArray("playlist");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QString filePath = settings.value("filePath").toString();
        if (QFile::exists(filePath)) {
            m_playlist->addFile(MusicFile(filePath));
        }
    }
    settings.endArray();
    
    // 加载上次播放的歌曲索引和位置
    int lastIndex = settings.value("currentIndex", -1).toInt();
    qint64 lastPosition = settings.value("position", 0).toLongLong();
    bool wasPlaying = settings.value("isPlaying", false).toBool();
    
    if (lastIndex >= 0 && lastIndex < m_playlist->count()) {
        m_playlist->setCurrentIndex(lastIndex);
        if (wasPlaying) {
            m_player->play();
            m_player->seekTo(lastPosition);
        }
    }
}

void MainWindow::saveSettings()
{
    QSettings settings("YinYue", "MusicPlayer");
    
    // 保存音量
    settings.setValue("volume", m_player->getVolume());
    
    // 保存播放模式
    settings.setValue("playMode", static_cast<int>(m_player->playMode()));
    
    // 保存播放列表
    settings.beginWriteArray("playlist");
    for (int i = 0; i < m_playlist->count(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("filePath", m_playlist->at(i).filePath());
    }
    settings.endArray();
    
    // 保存当前播放的歌曲索引、位置和状态
    settings.setValue("currentIndex", m_playlist->currentIndex());
    settings.setValue("position", m_player->getPosition());
    settings.setValue("isPlaying", m_player->getPlaybackState() == "Playing");
    
    settings.sync();
}

void MainWindow::savePlaybackState()
{
    QSettings settings("YinYue", "MusicPlayer");
    settings.setValue("lastPosition", m_player->getPosition());
    settings.setValue("lastIndex", m_playlist->currentIndex());
    settings.setValue("wasPlaying", m_player->getPlaybackState() == "Playing");
}

void MainWindow::restorePlaybackState()
{
    QSettings settings("YinYue", "MusicPlayer");
    int lastIndex = settings.value("lastIndex", -1).toInt();
    qint64 lastPosition = settings.value("lastPosition", 0).toLongLong();
    bool wasPlaying = settings.value("wasPlaying", false).toBool();
    
    if (lastIndex >= 0 && lastIndex < m_playlist->count()) {
        m_playlist->setCurrentIndex(lastIndex);
        if (wasPlaying) {
            m_player->play();
            m_player->seekTo(lastPosition);
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    savePlaybackState();
    QMainWindow::closeEvent(event);
}

void MainWindow::on_playModeButton_clicked()
{
    togglePlayMode();
}

void MainWindow::updatePlayModeButton(Playlist::PlayMode mode)
{
    QString modeText = getPlayModeText(mode);
    ui->playModeButton->setText(modeText);
    ui->playModeButton->setToolTip(tr("当前播放模式：%1").arg(modeText));
}

QString MainWindow::getPlayModeText(Playlist::PlayMode mode)
{
    switch (mode) {
        case Playlist::Sequential:
            return tr("顺序播放");
        case Playlist::Random:
            return tr("随机播放");
        case Playlist::RepeatOne:
            return tr("单曲循环");
        case Playlist::RepeatAll:
            return tr("列表循环");
        default:
            return tr("顺序播放");
    }
}

void MainWindow::playSelectedTrack(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }
    
    m_playlist->setCurrentIndex(index.row());
    m_player->play();
}

void MainWindow::addFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("添加音乐文件"),
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
        tr("音乐文件 (*.mp3 *.wav *.flac *.m4a *.ogg)")
    );
    
    if (files.isEmpty()) {
        return;
    }
    
    for (const QString& file : files) {
        m_playlist->addFile(MusicFile(file));
    }
}

void MainWindow::removeSelectedTracks()
{
    QModelIndexList indexes = ui->playlistView->selectionModel()->selectedIndexes();
    if (indexes.isEmpty()) {
        return;
    }
    
    // 从大到小排序，这样删除时不会影响其他索引
    std::sort(indexes.begin(), indexes.end(), [](const QModelIndex& a, const QModelIndex& b) {
        return a.row() > b.row();
    });
    
    for (const QModelIndex& index : indexes) {
        m_playlist->removeFile(index.row());
    }
}

void MainWindow::clearPlaylist()
{
    m_playlist->clear();
}

void MainWindow::togglePlayMode()
{
    Playlist::PlayMode currentMode = m_player->playMode();
    Playlist::PlayMode newMode;
    
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
            break;
    }
    
    m_player->setPlayMode(newMode);
    updatePlayModeButton(newMode);
}

