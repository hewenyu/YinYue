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
    , m_lyric(new Lyric(this))
    , m_isPlaying(false)
    , m_fileWatcher(new QFileSystemWatcher(this))
    , m_progressTimer(new QTimer(this))
    , m_lastPosition(0)
    , m_isUserSeeking(false)
{
    ui->setupUi(this);
    
    // 设置播放器的播放列表
    m_player->setPlaylist(m_playlist);
    
    setupConnections();
    
    // 设置进度条更新定时器
    m_progressTimer->setInterval(100);  // 100ms更新一次
    connect(m_progressTimer, &QTimer::timeout, this, [this]() {
        if (!m_isUserSeeking) {
            qint64 position = m_player->position();
            updatePosition(position);
        }
    });
    
    // 设置初始音量
    ui->volumeSlider->setValue(m_player->volume());
    
    // 设置歌词显示格式
    ui->lyricEdit->setStyleSheet("QTextEdit { background-color: transparent; color: #333333; }");
    QFont lyricFont = ui->lyricEdit->font();
    lyricFont.setPointSize(12);
    ui->lyricEdit->setFont(lyricFont);
    
    // 加载配置
    loadSettings();
    restorePlaybackState();
}

MainWindow::~MainWindow()
{
    // 保存配置
    saveSettings();
    delete ui;
}

void MainWindow::setupConnections()
{
    // 连接播放器信号
    connect(m_player, &MusicPlayer::stateChanged, this, &MainWindow::updatePlaybackState);
    connect(m_player, &MusicPlayer::positionChanged, this, &MainWindow::updatePosition);
    connect(m_player, &MusicPlayer::durationChanged, this, &MainWindow::updateDuration);
    connect(m_player, &MusicPlayer::errorOccurred, this, &MainWindow::handleError);
    
    // 连接播放列表信号
    connect(m_playlist, &Playlist::playlistChanged, m_player, &MusicPlayer::onPlaylistChanged);
    
    // 连接文件监控信号
    connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged,
            this, &MainWindow::onDirectoryChanged);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged,
            this, &MainWindow::onFileChanged);
    
    // 连接位置更新信号到歌词更新槽
    connect(m_player, &MusicPlayer::positionChanged, this, &MainWindow::updateLyric);
    
    // 设置歌词显示区域的样式
    ui->lyricEdit->setStyleSheet(
        "QTextEdit {"
        "   background-color: transparent;"
        "   border: none;"
        "   padding: 30px;"
        "}"
    );
    ui->lyricEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->lyricEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // 设置一个合适的字体
    QFont lyricFont = ui->lyricEdit->font();
    lyricFont.setFamily("Microsoft YaHei");
    ui->lyricEdit->setFont(lyricFont);
    
    // 初始调整字体大小
    adjustLyricFontSize();
    
    // 进度条相关连接
    connect(ui->progressSlider, &QSlider::sliderPressed, this, [this]() {
        m_isUserSeeking = true;
        stopProgressTimer();
    });
    
    connect(ui->progressSlider, &QSlider::sliderReleased, this, [this]() {
        m_isUserSeeking = false;
        m_player->setPosition(ui->progressSlider->value());
        if (m_isPlaying) {
            startProgressTimer();
        }
    });
    
    // 连接播放模式信号
    connect(m_player, &MusicPlayer::playModeChanged, this, &MainWindow::updatePlayModeButton);
}

void MainWindow::onDirectoryChanged(const QString &path)
{
    qDebug() << "目录发生变化:" << path;
    if (path == m_currentMusicFolder) {
        refreshMusicLibrary();
    }
}

void MainWindow::onFileChanged(const QString &path)
{
    qDebug() << "文件发生变化:" << path;
    if (m_musicLibrary.contains(path)) {
        // 重新加载文件元数据
        MusicFile musicFile(path);
        m_musicLibrary[path] = musicFile;
        refreshMusicLibrary();
    }
}

void MainWindow::refreshMusicLibrary()
{
    if (m_currentMusicFolder.isEmpty()) {
        return;
    }
    
    // 保存当前选中的项
    QString currentItem;
    if (ui->libraryWidget->currentItem()) {
        currentItem = ui->libraryWidget->currentItem()->text();
    }
    
    // 清空列表
    ui->libraryWidget->clear();
    
    // 重新扫描目录
    QDir dir(m_currentMusicFolder);
    QStringList filters;
    filters << "*.mp3" << "*.wav" << "*.flac";
    dir.setNameFilters(filters);
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::Readable);
    QFileInfoList fileList = dir.entryInfoList();
    
    // 新音乐库
    for (const QFileInfo &fileInfo : fileList) {
        QString filePath = fileInfo.absoluteFilePath();
        if (!m_musicLibrary.contains(filePath)) {
            MusicFile musicFile(filePath);
            m_musicLibrary[filePath] = musicFile;
        }
        
        // 添加到列表显示
        const MusicFile &musicFile = m_musicLibrary[filePath];
        QString displayText = musicFile.title();
        if (!musicFile.artist().isEmpty()) {
            displayText = musicFile.artist() + " - " + musicFile.title();
        }
        QListWidgetItem *item = new QListWidgetItem(displayText, ui->libraryWidget);
        item->setToolTip(filePath);
    }
    
    // 恢复选中状态
    if (!currentItem.isEmpty()) {
        QList<QListWidgetItem*> items = ui->libraryWidget->findItems(currentItem, Qt::MatchExactly);
        if (!items.isEmpty()) {
            ui->libraryWidget->setCurrentItem(items.first());
        }
    }
}

void MainWindow::loadFolder(const QString &folderPath)
{
    qDebug() << "正在加载文件夹:" << folderPath;
    
    QDir dir(folderPath);
    if (!dir.exists()) {
        QMessageBox::warning(this, tr("错误"), tr("文件夹不存在"));
        return;
    }
    
    // 更新当前音乐文件夹
    m_currentMusicFolder = folderPath;
    
    // 设置文件监控
    if (!m_fileWatcher->directories().isEmpty()) {
        m_fileWatcher->removePaths(m_fileWatcher->directories());
    }
    m_fileWatcher->addPath(folderPath);
    
    // 清空并重新加载音乐库
    m_musicLibrary.clear();
    refreshMusicLibrary();
}

void MainWindow::loadFile(const QString &filePath)
{
    qDebug() << "正在加载文件:" << filePath;
    
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isReadable() || fileInfo.size() == 0) {
        qDebug() << "文件无效:" << filePath;
        return;
    }
    
    // 添加到音乐库
    MusicFile musicFile(filePath);
    m_musicLibrary[filePath] = musicFile;
    
    // 添加到文件监控
    if (!m_fileWatcher->files().contains(filePath)) {
        m_fileWatcher->addPath(filePath);
    }
    
    // 添加到音乐库列表
    QString displayText = musicFile.title();
    if (!musicFile.artist().isEmpty()) {
        displayText = musicFile.artist() + " - " + musicFile.title();
    }
    QListWidgetItem *item = new QListWidgetItem(displayText, ui->libraryWidget);
    item->setToolTip(filePath);
    
    qDebug() << "文件加载成功:" << displayText;
}

void MainWindow::addToPlaylist(const MusicFile &file)
{
    // 检查是否已存在
    for (int i = 0; i < ui->playlistWidget->count(); ++i) {
        QListWidgetItem *item = ui->playlistWidget->item(i);
        if (item->toolTip() == file.filePath()) {
            // 文件已存在，不重复添加
            return;
        }
    }
    
    // 添加到播放列表
    m_playlist->addFile(file);
    
    // 添加到播放队列显示
    QString displayText = file.title();
    if (!file.artist().isEmpty()) {
        displayText = file.artist() + " - " + file.title();
    }
    QListWidgetItem *item = new QListWidgetItem(displayText, ui->playlistWidget);
    item->setToolTip(file.filePath());
    
    // 仅当播放列表为空且没有正在播放的音乐时，才自动选中并播放
    if (ui->playlistWidget->count() == 1 && !m_isPlaying) {
        ui->playlistWidget->setCurrentItem(item);
        m_playlist->setCurrentIndex(0);
        updateCurrentSong(file, true);  // 第一首歌时完整更新
        m_player->setSource(file.fileUrl());
        m_player->play();
    }
}

void MainWindow::updateCurrentSong(const MusicFile &file, bool updatePlayer)
{
    // 更新标题和艺术家信息
    QString title = file.title();
    QString artist = file.artist();
    
    ui->titleLabel->setText(title.isEmpty() ? tr("未知歌曲") : title);
    ui->artistLabel->setText(artist.isEmpty() ? tr("未知艺术家") : artist);
    
    // 更新窗口标题
    setWindowTitle(QString("%1 - %2").arg(title).arg(artist));
    
    // 高亮显示当前播放的歌曲
    for (int i = 0; i < ui->playlistWidget->count(); ++i) {
        QListWidgetItem *item = ui->playlistWidget->item(i);
        QFont font = item->font();
        if (item->toolTip() == file.filePath()) {
            font.setBold(true);
            item->setFont(font);
            ui->playlistWidget->setCurrentItem(item);
        } else {
            font.setBold(false);
            item->setFont(font);
        }
    }
    
    // 只在需要时加载歌词
    if (updatePlayer) {
        loadLyric(file.filePath());
    }
}

void MainWindow::loadLyric(const QString &musicFilePath)
{
    // 清空当前歌词
    m_lyric->clear();
    ui->lyricEdit->clear();
    
    // 尝试加载同名的.lrc文件
    QFileInfo musicFile(musicFilePath);
    QString baseName = musicFile.completeBaseName();
    QString lrcPath = musicFile.absolutePath() + QDir::separator() + baseName + ".lrc";
    
    qDebug() << "尝试加载歌词文件:" << lrcPath;
    QFileInfo lrcFile(lrcPath);
    
    if (lrcFile.exists()) {
        qDebug() << "歌词文件存在，大小:" << lrcFile.size() << "字节";
        if (m_lyric->loadFromFile(lrcPath)) {
            qDebug() << "歌词加载成功";
            // 显示第一句歌词
            updateLyric(0);
        } else {
            qDebug() << "歌词文件加载失败";
            ui->lyricEdit->setText(tr("歌词文件格式错误"));
        }
    } else {
        qDebug() << "未找到歌词文件";
        ui->lyricEdit->setText(tr("暂无歌词"));
        
        // 尝试其他可能的歌词文件名
        QStringList possibleNames;
        possibleNames << baseName.toLower() + ".lrc"
                     << baseName.toUpper() + ".lrc"
                     << musicFile.fileName().toLower().replace(musicFile.suffix(), "lrc")
                     << musicFile.fileName().toUpper().replace(musicFile.suffix(), "lrc");
        
        qDebug() << "尝试其他可能的歌词文件名:";
        for (const QString &name : possibleNames) {
            QString path = musicFile.absolutePath() + QDir::separator() + name;
            qDebug() << "  检查:" << path;
            if (QFile::exists(path)) {
                qDebug() << "找到替代歌词文件:" << path;
                if (m_lyric->loadFromFile(path)) {
                    qDebug() << "替代歌词文件加载成功";
                    updateLyric(0);
                    return;
                }
            }
        }
    }
}

void MainWindow::updateLyric(qint64 position)
{
    if (m_lyric->isEmpty()) {
        return;
    }
    
    QString currentLyric = m_lyric->getLyricText(position);
    if (!currentLyric.isEmpty()) {
        // 获取当前歌词的前后歌词
        qint64 prevTime1 = position - 10000; // 前10秒
        qint64 prevTime2 = position - 5000;  // 前5秒
        qint64 nextTime1 = position + 5000;  // 后5秒
        qint64 nextTime2 = position + 10000; // 后10秒
        
        QString prevLyric1 = m_lyric->getLyricText(prevTime1);
        QString prevLyric2 = m_lyric->getLyricText(prevTime2);
        QString nextLyric1 = m_lyric->getLyricText(nextTime1);
        QString nextLyric2 = m_lyric->getLyricText(nextTime2);
        
        // 获取基础字体大小
        int baseFontSize = ui->lyricEdit->font().pointSize();
        
        // 构建显示文本，使用对字体大小
        QString displayText;
        
        // 前两行歌词（较暗）
        if (!prevLyric1.isEmpty() && prevLyric1 != prevLyric2) {
            displayText += QString("<p style='margin: %1px; color: #BBBBBB; font-size: %2px;'>%3</p>")
                .arg(baseFontSize * 0.6)
                .arg(baseFontSize * 0.8)
                .arg(prevLyric1);
        }
        if (!prevLyric2.isEmpty() && prevLyric2 != currentLyric) {
            displayText += QString("<p style='margin: %1px; color: #999999; font-size: %2px;'>%3</p>")
                .arg(baseFontSize * 0.6)
                .arg(baseFontSize * 0.9)
                .arg(prevLyric2);
        }
        
        // 当前词（大号加粗）
        displayText += QString("<p style='margin: %1px; color: #333333; font-size: %2px; font-weight: bold;'>%3</p>")
            .arg(baseFontSize)
            .arg(baseFontSize * 1.5)
            .arg(currentLyric);
        
        // 后两行歌词（较暗）
        if (!nextLyric1.isEmpty() && nextLyric1 != currentLyric) {
            displayText += QString("<p style='margin: %1px; color: #999999; font-size: %2px;'>%3</p>")
                .arg(baseFontSize * 0.6)
                .arg(baseFontSize * 0.9)
                .arg(nextLyric1);
        }
        if (!nextLyric2.isEmpty() && nextLyric2 != nextLyric1) {
            displayText += QString("<p style='margin: %1px; color: #BBBBBB; font-size: %2px;'>%3</p>")
                .arg(baseFontSize * 0.6)
                .arg(baseFontSize * 0.8)
                .arg(nextLyric2);
        }
        
        ui->lyricEdit->setHtml(displayText);
        
        // 将当前歌词滚动到中间
        QTextCursor cursor = ui->lyricEdit->textCursor();
        cursor.movePosition(QTextCursor::Start);
        ui->lyricEdit->setTextCursor(cursor);
        ui->lyricEdit->ensureCursorVisible();
        
        qDebug() << "更新歌词:" << position << "ms -" << currentLyric;
    }
}

void MainWindow::on_libraryWidget_doubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }
    
    QListWidgetItem *item = ui->libraryWidget->item(index.row());
    QString filePath = item->toolTip();
    if (m_musicLibrary.contains(filePath)) {
        addToPlaylist(m_musicLibrary[filePath]);
    }
}

void MainWindow::on_playlistWidget_doubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }
    
    int row = index.row();
    if (row < 0 || row >= m_playlist->count()) {
        return;
    }
    
    m_playlist->setCurrentIndex(row);
    MusicFile currentFile = m_playlist->at(row);
    updateCurrentSong(currentFile, true);  // 双击播放时完整更新
    
    m_player->setSource(currentFile.fileUrl());
    m_player->play();
}

void MainWindow::on_clearPlaylistButton_clicked()
{
    ui->playlistWidget->clear();
    m_playlist->clear();
    
    // 清除当前播放信息
    ui->titleLabel->setText(tr("未知歌曲"));
    ui->artistLabel->setText(tr("未知艺术家"));
    setWindowTitle(tr("音乐播放器"));
    
    // 清除歌词显示
    ui->lyricEdit->clear();
    m_lyric->clear();
    
    // 重置进度条和时间标签
    ui->progressSlider->setValue(0);
    ui->progressSlider->setMaximum(0);
    updateTimeLabel(ui->currentTimeLabel, 0);
    updateTimeLabel(ui->totalTimeLabel, 0);
}

void MainWindow::on_removeSelectedButton_clicked()
{
    QList<QListWidgetItem*> items = ui->playlistWidget->selectedItems();
    bool removedCurrentSong = false;
    int currentIndex = m_playlist->currentIndex();
    
    for (QListWidgetItem *item : items) {
        int row = ui->playlistWidget->row(item);
        // 检查是否移除了当前播放的歌曲
        if (row == currentIndex) {
            removedCurrentSong = true;
        }
        m_playlist->removeFile(row);
        delete item;
    }
    
    // 如果移除了当前播放的歌曲，或播放列表已空
    if (removedCurrentSong || m_playlist->count() == 0) {
        // 停止播放
        m_player->stop();
        m_player->setSource(QUrl());  // 清除当前媒体
        
        // 清除当前播放信息
        ui->titleLabel->setText(tr("未知歌曲"));
        ui->artistLabel->setText(tr("未知艺术家"));
        setWindowTitle(tr("音乐播放器"));
        
        // 清除歌词显示
        ui->lyricEdit->clear();
        m_lyric->clear();
        
        // 重置进度条和时间标签
        ui->progressSlider->setValue(0);
        ui->progressSlider->setMaximum(0);
        updateTimeLabel(ui->currentTimeLabel, 0);
        updateTimeLabel(ui->totalTimeLabel, 0);
    }
    // 如果还有歌曲，且移除了当前播放的歌曲，播放下一首
    else if (removedCurrentSong && m_playlist->count() > 0) {
        int nextIndex = m_playlist->nextIndex();
        if (nextIndex != -1) {
            m_playlist->setCurrentIndex(nextIndex);
            MusicFile currentFile = m_playlist->at(nextIndex);
            updateCurrentSong(currentFile, true);  // 切换到下一首时完整更新
            m_player->setSource(currentFile.fileUrl());
            m_player->play();
        }
    }
}

void MainWindow::on_playButton_clicked()
{
    if (m_isPlaying) {
        m_player->pause();
    } else {
        if (m_playlist->currentIndex() == -1 && m_playlist->count() > 0) {
            // 如果没有选中的歌曲但播放列表不为空，播放第一首
            m_playlist->setCurrentIndex(0);
            MusicFile currentFile = m_playlist->at(0);
            updateCurrentSong(currentFile, true);  // 开始播放时完整更新
            m_player->setSource(currentFile.fileUrl());
        } else if (m_playlist->currentIndex() >= 0) {
            // 如果有选中的歌曲，更新当前歌曲信息
            MusicFile currentFile = m_playlist->at(m_playlist->currentIndex());
            updateCurrentSong(currentFile, true);  // 继续播放时完整更新
        }
        m_player->play();
    }
}

void MainWindow::on_previousButton_clicked()
{
    int prevIndex = m_playlist->previousIndex();
    if (prevIndex != -1) {
        m_playlist->setCurrentIndex(prevIndex);
        MusicFile currentFile = m_playlist->at(prevIndex);
        updateCurrentSong(currentFile, true);  // 切换到上一首时完整更新
        m_player->setSource(currentFile.fileUrl());
        m_player->play();
    }
}

void MainWindow::on_nextButton_clicked()
{
    int nextIndex = m_playlist->nextIndex();
    if (nextIndex != -1) {
        m_playlist->setCurrentIndex(nextIndex);
        MusicFile currentFile = m_playlist->at(nextIndex);
        updateCurrentSong(currentFile, true);  // 切换到下一首时完整更新
        m_player->setSource(currentFile.fileUrl());
        m_player->play();
    }
}

void MainWindow::on_volumeSlider_valueChanged(int value)
{
    m_player->setVolume(value);
}

void MainWindow::on_progressSlider_sliderMoved(int position)
{
    m_player->setPosition(position);
}

void MainWindow::on_actionOpenFile_triggered()
{
    QString filePath = QFileDialog::getOpenFileName(this,
        tr("打开音乐文件"),
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
        tr("音乐文件 (*.mp3 *.wav *.flac)"));
        
    if (!filePath.isEmpty()) {
        loadFile(filePath);
    }
}

void MainWindow::on_actionOpenFolder_triggered()
{
    QString folderPath = QFileDialog::getExistingDirectory(this,
        tr("打开音乐文件夹"),
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
        
    if (!folderPath.isEmpty()) {
        loadFolder(folderPath);
    }
}

void MainWindow::on_actionExit_triggered()
{
    close();
}

void MainWindow::updatePlaybackState(QMediaPlayer::State state)
{
    m_isPlaying = (state == QMediaPlayer::PlayingState);
    ui->playButton->setText(m_isPlaying ? tr("暂停") : tr("播放"));
    
    if (m_isPlaying) {
        startProgressTimer();
    } else {
        stopProgressTimer();
    }
}

void MainWindow::updatePosition(qint64 position)
{
    ui->progressSlider->setValue(position);
    updateTimeLabel(ui->currentTimeLabel, position);
}

void MainWindow::updateDuration(qint64 duration)
{
    ui->progressSlider->setMaximum(duration);
    updateTimeLabel(ui->totalTimeLabel, duration);
}

void MainWindow::handleError(const QString &error)
{
    QMessageBox::warning(this, tr("错误"), error);
}

void MainWindow::updateTimeLabel(QLabel *label, qint64 time)
{
    int seconds = time / 1000;
    int minutes = seconds / 60;
    seconds %= 60;
    label->setText(QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0')));
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    adjustLyricFontSize();
}

void MainWindow::adjustLyricFontSize()
{
    // 获取歌词显示区域的大小
    int width = ui->lyricEdit->width();
    int height = ui->lyricEdit->height();
    
    // 根据窗口大小计算基础字体大小
    int baseFontSize = qMin(width / 30, height / 15);
    baseFontSize = qBound(12, baseFontSize, 32); // 限制字体大小范围
    
    // 更新字体大小
    QFont lyricFont = ui->lyricEdit->font();
    lyricFont.setPointSize(baseFontSize);
    ui->lyricEdit->setFont(lyricFont);
    
    // 更新歌词显示
    updateLyric(m_player->position());
}

void MainWindow::loadSettings()
{
    QSettings settings("YinYue", "MusicPlayer");
    
    // 加载音乐文件夹路径
    QString musicFolder = settings.value("musicFolder").toString();
    if (!musicFolder.isEmpty() && QDir(musicFolder).exists()) {
        loadFolder(musicFolder);
    }
    
    // 加载播放列表
    int size = settings.beginReadArray("playlist");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QString filePath = settings.value("filePath").toString();
        if (QFile::exists(filePath)) {
            MusicFile musicFile(filePath);
            addToPlaylist(musicFile);
        }
    }
    settings.endArray();
    
    // 加载音量
    int volume = settings.value("volume", 50).toInt();
    ui->volumeSlider->setValue(volume);
    m_player->setVolume(volume);
    
    // 加载上次播放的歌曲索引和位置
    int lastIndex = settings.value("currentIndex", -1).toInt();
    qint64 lastPosition = settings.value("position", 0).toLongLong();
    bool wasPlaying = settings.value("isPlaying", false).toBool();
    
    if (lastIndex >= 0 && lastIndex < m_playlist->count()) {
        m_playlist->setCurrentIndex(lastIndex);
        MusicFile currentFile = m_playlist->at(lastIndex);
        updateCurrentSong(currentFile);
        
        // 设置音乐源并恢复位置
        m_player->setSource(currentFile.fileUrl());
        m_player->setPosition(lastPosition);
        
        // 如果之前在播放，则自动开始播放
        if (wasPlaying) {
            m_player->play();
        }
    }
    
    // 加载播放模式
    int playMode = settings.value("playMode", static_cast<int>(Playlist::Sequential)).toInt();
    m_player->setPlayMode(static_cast<Playlist::PlayMode>(playMode));
}

void MainWindow::saveSettings()
{
    QSettings settings("YinYue", "MusicPlayer");
    
    // 保存音乐文件夹路径
    if (!m_currentMusicFolder.isEmpty()) {
        settings.setValue("musicFolder", m_currentMusicFolder);
    }
    
    // 保存播放列表
    settings.beginWriteArray("playlist");
    for (int i = 0; i < m_playlist->count(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("filePath", m_playlist->at(i).filePath());
    }
    settings.endArray();
    
    // 保存音量
    settings.setValue("volume", m_player->volume());
    
    // 保存当前播放的歌曲索引、位置和状态
    settings.setValue("currentIndex", m_playlist->currentIndex());
    settings.setValue("position", m_player->position());
    settings.setValue("isPlaying", m_isPlaying);
    
    // 保存播放模式
    settings.setValue("playMode", static_cast<int>(m_player->playMode()));
    
    settings.sync();
}

void MainWindow::startProgressTimer()
{
    if (!m_progressTimer->isActive()) {
        m_progressTimer->start();
    }
}

void MainWindow::stopProgressTimer()
{
    if (m_progressTimer->isActive()) {
        m_progressTimer->stop();
    }
}

void MainWindow::savePlaybackState()
{
    QSettings settings;
    settings.setValue("lastPosition", m_player->position());
    
    // 使用currentIndex和at方法获取当前文件
    int currentIndex = m_playlist->currentIndex();
    if (currentIndex >= 0 && currentIndex < m_playlist->count()) {
        settings.setValue("lastFile", m_playlist->at(currentIndex).filePath());
    }
}

void MainWindow::restorePlaybackState()
{
    QSettings settings;
    m_lastPosition = settings.value("lastPosition", 0).toLongLong();
    QString lastFile = settings.value("lastFile").toString();
    
    if (!lastFile.isEmpty() && QFile::exists(lastFile)) {
        // 找到对应的播放列表项并设置
        for (int i = 0; i < ui->playlistWidget->count(); ++i) {
            QListWidgetItem *item = ui->playlistWidget->item(i);
            if (item->toolTip() == lastFile) {
                ui->playlistWidget->setCurrentItem(item);
                m_playlist->setCurrentIndex(i);
                m_player->setPosition(m_lastPosition);
                updatePosition(m_lastPosition);
                break;
            }
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
    m_player->togglePlayMode();
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
            return tr("顺序");
        case Playlist::Random:
            return tr("随机");
        case Playlist::RepeatOne:
            return tr("单曲");
        case Playlist::RepeatAll:
            return tr("循环");
        default:
            return tr("顺序");
    }
}

