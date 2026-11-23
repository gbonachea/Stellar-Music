#pragma once
#include <QFileInfo>
#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
inline QStringList audioGlobs() {
  return {"*.mp3", "*.wav", "*.flac", "*.ogg",  "*.opus",
          "*.m4a", "*.aac", "*.aif",  "*.aiff", "*.wma"};
}
inline QString clockText(qint64 ms) {
  auto s = qMax(qint64(0), ms) / 1000;
  return QString("%1:%2")
      .arg(s / 60, 2, 10, QChar('0'))
      .arg(s % 60, 2, 10, QChar('0'));
}
struct Track {
  QString title, artist, album;
  int bitrate = 0, rate = 0, channels = 0, ms = 0;
  explicit Track(const QString &path) {
    TagLib::FileRef file(QFile::encodeName(path).constData());
    if (!file.isNull()) {
      if (auto *t = file.tag()) {
        title = QString::fromStdString(t->title().to8Bit(true));
        artist = QString::fromStdString(t->artist().to8Bit(true));
        album = QString::fromStdString(t->album().to8Bit(true));
      }
      if (auto *a = file.audioProperties()) {
        bitrate = a->bitrate();
        rate = a->sampleRate();
        channels = a->channels();
        ms = a->lengthInMilliseconds();
      }
    }
    if (title.isEmpty())
      title = QFileInfo(path).completeBaseName();
  }
  QString caption() const {
    QStringList fields{artist, title, album};
    fields.removeAll("");
    return fields.join("  —  ");
  }
};
