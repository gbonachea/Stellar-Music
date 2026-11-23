#pragma once
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <array>
#include <atomic>
#include <gst/gst.h>
#include <mutex>

inline constexpr std::array<int, 10> centers{31,   62,   125,  250,  500,
                                             1000, 2000, 4000, 8000, 16000};
inline QString bandKey(int hz) {
  return hz < 1000 ? QString::number(hz) + "Hz"
                   : QString::number(hz / 1000) + "kHz";
}
class Playback : public QObject {
  Q_OBJECT
public:
  Playback();
  ~Playback();
  void open(const QString &path);
  void play();
  void pause();
  void stop();
  bool seek(qint64 ms);
  qint64 position() const;
  qint64 duration() const;
  void volume(int percent, bool muted);
  void equalize(const QVariantMap &bands, bool enabled, int preamp);
  enum State { Stopped, Paused, Playing };
  State state = Stopped;
  QString failure;
  std::atomic<bool> capture{false};
  std::array<float, 1024> samples();
signals:
  void seeked(qint64 ms);
  void ended();
  void error(QString message);

private:
  void drain();
  static void handoff(GstElement *, GstBuffer *, gpointer);
  GstElement *player = nullptr, *filter = nullptr, *eq = nullptr,
             *gain = nullptr, *tap = nullptr;
  GstBus *bus = nullptr;
  QTimer timer;
  std::mutex lock;
  std::array<float, 1024> ring{};
  size_t cursor = 0;
};
