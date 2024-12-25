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
    , m_isPlaying(false)
{
    ui->setupUi(this);
    setupConnections();
    
    // 设置初始音量
    ui->volumeSlider->setValue(m_player->volume());
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
}

void MainWindow::on_playButton_clicked()
{
    if (m_isPlaying) {
        m_player->pause();
    } else {
        if (m_playlist->currentIndex() == -1 && m_playlist->count() > 0) {
            m_playlist->setCurrentIndex(0);
            m_player->setSource(m_playlist->at(0).fileUrl());
        }
        m_player->play();
    }
}

void MainWindow::on_previousButton_clicked()
{
    int prevIndex = m_playlist->previousIndex();
    if (prevIndex != -1) {
        m_playlist->setCurrentIndex(prevIndex);
        m_player->setSource(m_playlist->at(prevIndex).fileUrl());
        m_player->play();
    }
}

void MainWindow::on_nextButton_clicked()
{
    int nextIndex = m_playlist->nextIndex();
    if (nextIndex != -1) {
        m_playlist->setCurrentIndex(nextIndex);
        m_player->setSource(m_playlist->at(nextIndex).fileUrl());
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

void MainWindow::on_playlistWidget_doubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }
    
    int row = index.row();
    if (row < 0 || row >= m_playlist->count()) {
        qDebug() << "无效的播放列表索引:" << row;
        return;
    }
    
    m_playlist->setCurrentIndex(row);
    MusicFile currentFile = m_playlist->at(row);
    qDebug() << "正在播放:" << currentFile.title() << "路径:" << currentFile.filePath();
    
    m_player->setSource(currentFile.fileUrl());
    m_player->play();
    
    // 更新界面显示
    ui->titleLabel->setText(currentFile.title());
    ui->artistLabel->setText(currentFile.artist());
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

void MainWindow::loadFile(const QString &filePath)
{
    qDebug() << "正在加载文件:" << filePath;
    
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        qDebug() << "文件不存在:" << filePath;
        return;
    }
    
    if (!fileInfo.isReadable()) {
        qDebug() << "文件不可读:" << filePath;
        return;
    }
    
    // 检查文件大小
    if (fileInfo.size() == 0) {
        qDebug() << "文件大小为0:" << filePath;
        return;
    }
    
    MusicFile musicFile(filePath);
    
    // 检查文件是否成功加载
    if (musicFile.title().isEmpty()) {
        qDebug() << "使用文件名作为标题:" << filePath;
        QString title = fileInfo.baseName();
        musicFile.setTitle(title);
    }
    
    // 添加到播放列表
    m_playlist->addFile(musicFile);
    
    // 添加到播放列表显示
    QListWidgetItem *item = new QListWidgetItem(ui->playlistWidget);
    QString displayText = musicFile.title();
    if (!musicFile.artist().isEmpty()) {
        displayText = musicFile.artist() + " - " + musicFile.title();
    }
    item->setText(displayText);
    item->setToolTip(fileInfo.absoluteFilePath()); // 显示完整路径作为提示
    
    qDebug() << "文件加载成功:" << displayText;
    
    // 如果是第一个加载的文件，自动选中
    if (ui->playlistWidget->count() == 1) {
        ui->playlistWidget->setCurrentRow(0);
        ui->titleLabel->setText(musicFile.title());
        ui->artistLabel->setText(musicFile.artist());
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
    
    // 先获取文件列表
    QStringList filters;
    filters << "*.mp3" << "*.wav" << "*.flac";
    dir.setNameFilters(filters);
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::Readable);
    QFileInfoList fileList = dir.entryInfoList();
    
    if (fileList.isEmpty()) {
        QMessageBox::information(this, tr("提示"), 
            tr("在文件夹 %1 中未找到支持的音乐文件").arg(folderPath));
        return;
    }

    // 创建进度对话框
    QProgressDialog progress(tr("正在加载音乐文件..."), tr("取消"), 0, fileList.size(), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0); // 立即显示进度对话框
    
    // 清空当前播放列表
    ui->playlistWidget->clear();
    m_playlist->clear();
    
    // 加载文件
    int count = 0;
    int failedCount = 0;
    
    for (const QFileInfo &fileInfo : fileList) {
        if (progress.wasCanceled()) {
            qDebug() << "用户取消加载";
            break;
        }
        
        try {
            loadFile(fileInfo.absoluteFilePath());
            count++;
        } catch (const std::exception &e) {
            qDebug() << "加载文件失败:" << fileInfo.absoluteFilePath() << e.what();
            failedCount++;
        }
        
        progress.setValue(count + failedCount);
        QApplication::processEvents(); // 处理事件，保持界面响应
    }
    
    // 更新状态栏
    QString statusMessage;
    if (failedCount > 0) {
        statusMessage = tr("已加载 %1 个文件，%2 个文件加载失败").arg(count).arg(failedCount);
    } else {
        statusMessage = tr("已加载 %1 个音乐文件").arg(count);
    }
    statusBar()->showMessage(statusMessage, 3000);
    
    qDebug() << "文件夹加载完成:" << statusMessage;
}

