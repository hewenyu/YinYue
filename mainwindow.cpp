#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
#include <QProgressDialog>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_player(new MusicPlayer(this))
    , m_playlist(new Playlist(this))
    , m_lyric(new Lyric(this))
    , m_isPlaying(false)
    , m_fileWatcher(new QFileSystemWatcher(this))
{
    ui->setupUi(this);
    setupConnections();
    
    // 设置初始音量
    ui->volumeSlider->setValue(m_player->volume());
    
    // 设置歌词显示格式
    ui->lyricEdit->setStyleSheet("QTextEdit { background-color: transparent; color: #333333; }");
    QFont lyricFont = ui->lyricEdit->font();
    lyricFont.setPointSize(12);
    ui->lyricEdit->setFont(lyricFont);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupConnections()
{
    // 连接播放器信号
    connect(m_player, &MusicPlayer::stateChanged, this, &MainWindow::updatePlaybackState);
    connect(m_player, &MusicPlayer::positionChanged, this, &MainWindow::updatePosition);
    connect(m_player, &MusicPlayer::durationChanged, this, &MainWindow::updateDuration);
    connect(m_player, &MusicPlayer::errorOccurred, this, &MainWindow::handleError);
    
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
        "   padding: 20px;"
        "}"
    );
    ui->lyricEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->lyricEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // 设置一个合适的字体
    QFont lyricFont = ui->lyricEdit->font();
    lyricFont.setFamily("Microsoft YaHei");
    lyricFont.setPointSize(12);
    ui->lyricEdit->setFont(lyricFont);
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
    // 添加到播放列表
    m_playlist->addFile(file);
    
    // 添加到播放队列显示
    QString displayText = file.title();
    if (!file.artist().isEmpty()) {
        displayText = file.artist() + " - " + file.title();
    }
    QListWidgetItem *item = new QListWidgetItem(displayText, ui->playlistWidget);
    item->setToolTip(file.filePath());
    
    // 如果是第一首添加的歌曲，自动选中
    if (ui->playlistWidget->count() == 1) {
        ui->playlistWidget->setCurrentItem(item);
        m_playlist->setCurrentIndex(0);
        updateCurrentSong(file);
    }
}

void MainWindow::updateCurrentSong(const MusicFile &file)
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
    
    // 加载歌词
    loadLyric(file.filePath());
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
        qint64 prevTime = position - 5000; // 前5秒
        qint64 nextTime = position + 5000; // 后5秒
        
        QString prevLyric = m_lyric->getLyricText(prevTime);
        QString nextLyric = m_lyric->getLyricText(nextTime);
        
        // 构建显示文本，使用更大的字体和更好的间距
        QString displayText;
        if (!prevLyric.isEmpty() && prevLyric != currentLyric) {
            displayText += QString("<p style='margin: 10px; color: #999999; font-size: 14px;'>%1</p>").arg(prevLyric);
        }
        displayText += QString("<p style='margin: 20px; color: #333333; font-size: 18px; font-weight: bold;'>%1</p>").arg(currentLyric);
        if (!nextLyric.isEmpty() && nextLyric != currentLyric) {
            displayText += QString("<p style='margin: 10px; color: #999999; font-size: 14px;'>%1</p>").arg(nextLyric);
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
    updateCurrentSong(currentFile);
    
    m_player->setSource(currentFile.fileUrl());
    m_player->play();
}

void MainWindow::on_clearPlaylistButton_clicked()
{
    ui->playlistWidget->clear();
    m_playlist->clear();
}

void MainWindow::on_removeSelectedButton_clicked()
{
    QList<QListWidgetItem*> items = ui->playlistWidget->selectedItems();
    for (QListWidgetItem *item : items) {
        int row = ui->playlistWidget->row(item);
        m_playlist->removeFile(row);
        delete item;
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
            updateCurrentSong(currentFile);
            m_player->setSource(currentFile.fileUrl());
        } else if (m_playlist->currentIndex() >= 0) {
            // 如果有选中的歌曲，更新当前歌曲信息
            MusicFile currentFile = m_playlist->at(m_playlist->currentIndex());
            updateCurrentSong(currentFile);
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
        updateCurrentSong(currentFile);
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
        updateCurrentSong(currentFile);
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

