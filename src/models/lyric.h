#ifndef LYRIC_H
#define LYRIC_H

#include <QObject>
#include <QMap>
#include <QString>

// 单条歌词结构
struct LyricLine {
    qint64 time;     // 时间戳（毫秒）
    QString text;    // 歌词文本
};

class Lyric : public QObject
{
    Q_OBJECT
public:
    explicit Lyric(QObject *parent = nullptr);
    
    // 加载歌词文件
    bool loadFromFile(const QString &filePath);
    
    // 根据时间获取歌词
    QString getLyricText(qint64 position) const;
    
    // 获取下一句歌词的时间戳
    qint64 getNextTimestamp(qint64 position) const;
    
    // 清空歌词
    void clear();
    
    // 是否有歌词
    bool isEmpty() const;

private:
    // 解析LRC格式歌词
    bool parseLRC(const QString &content);
    
    // 解析时间标签 [mm:ss.xx]
    qint64 parseTimeTag(const QString &tag) const;

private:
    QMap<qint64, QString> m_lyrics;  // 时间戳到歌词的映射
    QString m_title;                 // 歌曲标题
    QString m_artist;                // 艺术家
    QString m_album;                 // 专辑
};

#endif // LYRIC_H 