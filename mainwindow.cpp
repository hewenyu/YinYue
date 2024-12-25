#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>

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
    if (index.isValid()) {
        m_playlist->setCurrentIndex(index.row());
        m_player->setSource(m_playlist->at(index.row()).fileUrl());
        m_player->play();
    }
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
    MusicFile musicFile(filePath);
    m_playlist->addFile(musicFile);
    ui->playlistWidget->addItem(musicFile.title());
}

void MainWindow::loadFolder(const QString &folderPath)
{
    QDirIterator it(folderPath, QStringList() << "*.mp3" << "*.wav" << "*.flac",
        QDir::Files, QDirIterator::Subdirectories);
        
    while (it.hasNext()) {
        loadFile(it.next());
    }
}

