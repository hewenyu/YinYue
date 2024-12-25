#include "lyric.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <QFileInfo>

Lyric::Lyric(QObject *parent)
    : QObject(parent)
{
}

bool Lyric::loadFromFile(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        qDebug() << "歌词文件不存在:" << filePath;
        return false;
    }

    if (!fileInfo.isReadable()) {
        qDebug() << "歌词文件不可读:" << filePath;
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "无法打开歌词文件:" << filePath << "错误:" << file.errorString();
        return false;
    }
    
    // 清空现有歌词
    clear();
    
    QTextStream in(&file);
    // 尝试不同的编码
    QStringList codecs = {"UTF-8", "GBK", "GB18030", "System"};
    QString content;
    bool success = false;

    for (const QString &codec : codecs) {
        file.seek(0);  // 重置文件指针
        in.setCodec(codec.toUtf8());
        content = in.readAll();
        
        if (!content.isEmpty() && content.contains(QRegularExpression("\\[\\d{2}:\\d{2}\\.\\d{2}\\]"))) {
            qDebug() << "成功使用编码" << codec << "读取歌词文件";
            success = true;
            break;
        }
    }
    
    file.close();
    
    if (!success) {
        qDebug() << "无法正确解码歌词文件内容";
        return false;
    }

    bool result = parseLRC(content);
    if (!result) {
        qDebug() << "歌词解析失败，可能格式不正确";
    } else {
        qDebug() << "成功加载歌词，共" << m_lyrics.size() << "行";
    }
    return result;
}

QString Lyric::getLyricText(qint64 position) const
{
    if (m_lyrics.isEmpty()) {
        return QString();
    }
    
    // 找到第一个大于当前时间的歌词
    auto it = m_lyrics.upperBound(position);
    if (it == m_lyrics.begin()) {
        return m_lyrics.first();
    }
    
    // 返回前一句歌词
    --it;
    return it.value();
}

qint64 Lyric::getNextTimestamp(qint64 position) const
{
    if (m_lyrics.isEmpty()) {
        return -1;
    }
    
    // 找到下一个时间戳
    auto it = m_lyrics.upperBound(position);
    if (it == m_lyrics.end()) {
        return -1;
    }
    
    return it.key();
}

void Lyric::clear()
{
    m_lyrics.clear();
    m_title.clear();
    m_artist.clear();
    m_album.clear();
}

bool Lyric::isEmpty() const
{
    return m_lyrics.isEmpty();
}

bool Lyric::parseLRC(const QString &content)
{
    // 修改时间标签正则表达式以匹配更多格式
    QRegularExpression timeRegex("\\[(\\d{2}):(\\d{2})[.](\\d{1,2})\\]");
    QRegularExpression metaRegex("\\[([^:]+):([^\\]]+)\\]");
    
    const QStringList lines = content.split('\n');
    qDebug() << "开始解析歌词，共" << lines.size() << "行";
    
    for (const QString &line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty()) {
            continue;
        }
        
        // 解析元数据标签
        auto metaMatch = metaRegex.match(trimmedLine);
        if (metaMatch.hasMatch()) {
            QString key = metaMatch.captured(1).toLower();
            QString value = metaMatch.captured(2).trimmed();
            
            if (key == "ti") {
                m_title = value;
                qDebug() << "歌曲标题:" << value;
            } else if (key == "ar") {
                m_artist = value;
                qDebug() << "艺术家:" << value;
            } else if (key == "al") {
                m_album = value;
                qDebug() << "专辑:" << value;
            }
            continue;
        }
        
        // 解析时间标签
        auto timeMatchIt = timeRegex.globalMatch(trimmedLine);
        if (!timeMatchIt.hasNext()) {
            qDebug() << "忽略无时间标签的行:" << trimmedLine;
            continue;
        }
        
        // 获取歌词文本（移除所有时间标签后的内容）
        QString lyricText = trimmedLine;
        QRegularExpression allTimeTagsRegex("\\[\\d{2}:\\d{2}[.]\\d{1,2}\\]");
        lyricText.remove(allTimeTagsRegex);
        lyricText = lyricText.trimmed();
        
        if (lyricText.isEmpty()) {
            continue;
        }
        
        // 重新匹配所有时间标签
        timeMatchIt = timeRegex.globalMatch(trimmedLine);
        while (timeMatchIt.hasNext()) {
            auto match = timeMatchIt.next();
            int minutes = match.captured(1).toInt();
            int seconds = match.captured(2).toInt();
            int milliseconds = match.captured(3).toInt();
            
            // 处理毫秒部分，确保转换为3位数的毫秒值
            if (milliseconds < 10) {
                milliseconds *= 100;
            } else if (milliseconds < 100) {
                milliseconds *= 10;
            }
            
            qint64 timestamp = (minutes * 60 + seconds) * 1000 + milliseconds;
            
            m_lyrics[timestamp] = lyricText;
            qDebug() << "添加歌词:" << timestamp << lyricText;
        }
    }
    
    return !m_lyrics.isEmpty();
}

qint64 Lyric::parseTimeTag(const QString &tag) const
{
    // 移除方括号
    QString time = tag.mid(1, tag.length() - 2);
    QStringList parts = time.split(':');
    
    if (parts.size() != 2) {
        return -1;
    }
    
    int minutes = parts[0].toInt();
    double seconds = parts[1].toDouble();
    
    return (minutes * 60 + seconds) * 1000;
} 