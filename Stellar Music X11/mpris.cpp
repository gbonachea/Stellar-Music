#include "audioplayer.h"
#include "display.h"
#include "playlistwindow.h"
#include "track.h"
#include <QtDBus>

namespace {
constexpr auto service = "org.mpris.MediaPlayer2.StellarMusic";
constexpr auto object = "/org/mpris/MediaPlayer2";
constexpr auto iface = "org.mpris.MediaPlayer2.Player";
class Root final : public QDBusAbstractAdaptor {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
  Q_PROPERTY(bool CanQuit READ yes CONSTANT)
  Q_PROPERTY(bool CanRaise READ yes CONSTANT)
  Q_PROPERTY(bool HasTrackList READ no CONSTANT)
  Q_PROPERTY(QString Identity READ identity CONSTANT)
  Q_PROPERTY(QString DesktopEntry READ desktop CONSTANT)
  Q_PROPERTY(QStringList SupportedUriSchemes READ schemes CONSTANT)
  Q_PROPERTY(QStringList SupportedMimeTypes READ types CONSTANT)
public:
  explicit Root(AudioPlayer *p) : QDBusAbstractAdaptor(p), p(p) {}
  bool yes() const { return true; }
  bool no() const { return false; }
  QString identity() const { return "Stellar Music"; }
  QString desktop() const { return "stellarmusic"; }
  QStringList schemes() const { return {"file"}; }
  QStringList types() const {
    return {"audio/mpeg", "audio/flac", "audio/x-wav", "audio/ogg"};
  }
public slots:
  void Quit() { p->close(); }
  void Raise() {
    p->showNormal();
    p->raise();
    p->activateWindow();
  }

private:
  AudioPlayer *p;
};
} // namespace
class MediaKeys final : public QDBusAbstractAdaptor {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
  Q_PROPERTY(QString PlaybackStatus READ status)
  Q_PROPERTY(QString LoopStatus READ loop WRITE setLoop)
  Q_PROPERTY(bool Shuffle READ shuffled WRITE setShuffled)
  Q_PROPERTY(QVariantMap Metadata READ metadata)
  Q_PROPERTY(double Volume READ volume WRITE setVolume)
  Q_PROPERTY(qlonglong Position READ position)
  Q_PROPERTY(double Rate READ rate WRITE setRate)
  Q_PROPERTY(double MinimumRate READ rate CONSTANT)
  Q_PROPERTY(double MaximumRate READ rate CONSTANT)
  Q_PROPERTY(bool CanControl READ yes CONSTANT)
  Q_PROPERTY(bool CanGoNext READ hasQueue)
  Q_PROPERTY(bool CanGoPrevious READ hasQueue)
  Q_PROPERTY(bool CanPlay READ hasTrack)
  Q_PROPERTY(bool CanPause READ hasTrack)
  Q_PROPERTY(bool CanSeek READ seekable)
public:
  explicit MediaKeys(AudioPlayer *p) : QDBusAbstractAdaptor(p), p(p) {
    connect(p, &AudioPlayer::updated, this, [this] { notify(); });
    connect(p->volume, &QSlider::valueChanged, this, [this] { notify(); });
    connect(p->mute, &QPushButton::toggled, this, [this] { notify(); });
    connect(p->shuffle, &QPushButton::toggled, this, [this] { notify(); });
    connect(&p->engine, &Playback::seeked, this,
            [this](qint64 ms) { emit Seeked(ms * 1000); });
  }
  QString status() const {
    return QStringList{"Stopped", "Paused", "Playing"}[p->engine.state];
  }
  QString loop() const {
    return QStringList{"None", "Track", "Playlist"}[p->loop];
  }
  void setLoop(QString value) {
    int i = QStringList{"None", "Track", "Playlist"}.indexOf(value);
    if (i >= 0) {
      p->loop = i;
      p->updateControls();
    }
  }
  bool shuffled() const { return p->shuffle->isChecked(); }
  void setShuffled(bool on) {
    if (on != shuffled())
      p->shuffle->click();
  }
  bool hasTrack() const { return !p->current.isEmpty(); }
  bool hasQueue() const { return p->pl->playlist()->count() > 0; }
  bool yes() const { return true; }
  double rate() const { return 1.; }
  void setRate(double) {}
  bool seekable() const {
    return p->seek->isEnabled() && p->screen->length > 0;
  }
  qlonglong position() const { return p->engine.position() * 1000; }
  double volume() const {
    return p->mute->isChecked() ? 0. : p->volume->value() / 100.;
  }
  void setVolume(double v) {
    p->volume->setValue(qRound(100 * qBound(0., v, 1.)));
    if (p->mute->isChecked())
      p->mute->click();
  }
  QDBusObjectPath id() const {
    return QDBusObjectPath(
        "/org/stellarmusic/tracks/t" +
        QString::fromLatin1(QCryptographicHash::hash(p->current.toUtf8(),
                                                     QCryptographicHash::Sha256)
                                .toHex()));
  }
  QVariantMap metadata() const { return meta; }
public slots:
  void Play() { p->play->click(); }
  void Pause() { p->pause->click(); }
  void Stop() { p->stop->click(); }
  void PlayPause() {
    if (p->engine.state == Playback::Playing)
      Pause();
    else
      Play();
  }
  void Previous() { p->prev->click(); }
  void Next() { p->next->click(); }
  void OpenUri(QString uri) {
    QUrl u(uri);
    if (u.isLocalFile())
      p->openFiles({u.toLocalFile()});
  }
  void SetPosition(QDBusObjectPath track, qlonglong us) {
    if (track.path() == id().path() && seekable() && us >= 0 &&
        us / 1000 <= p->screen->length)
      p->engine.seek(us / 1000);
  }
  void Seek(qlonglong offset) {
    if (seekable()) {
      auto target = position() + offset;
      if (target / 1000 > p->screen->length)
        Next();
      else
        SetPosition(id(), qMax(0LL, target));
    }
  }
signals:
  void Seeked(qlonglong position);

private:
  void notify() {
    if (last != p->current) {
      last = p->current;
      meta.clear();
      if (hasTrack()) {
        Track t(last);
        meta = {{"mpris:trackid", QVariant::fromValue(id())},
                {"mpris:length", qint64(t.ms) * 1000},
                {"xesam:url", QUrl::fromLocalFile(last).toString()},
                {"xesam:title", t.title},
                {"xesam:artist", QStringList{t.artist}},
                {"xesam:album", t.album}};
      }
    }
    QVariantMap changed;
    for (auto key :
         {"PlaybackStatus", "LoopStatus", "Shuffle", "Metadata", "Volume",
          "CanPlay", "CanPause", "CanSeek", "CanGoNext", "CanGoPrevious"}) {
      auto v = property(key);
      if (cache.value(key) != v) {
        changed[key] = v;
        cache[key] = v;
      }
    }
    if (!changed.isEmpty()) {
      auto msg = QDBusMessage::createSignal(
          object, "org.freedesktop.DBus.Properties", "PropertiesChanged");
      msg << QString(iface) << changed << QStringList();
      QDBusConnection::sessionBus().send(msg);
    }
  }
  AudioPlayer *p;
  QString last;
  QVariantMap meta, cache;
};
void AudioPlayer::setupMpris() {
  auto bus = QDBusConnection::sessionBus();
  if (!bus.isConnected() || !bus.registerService(service))
    return;
  new Root(this);
  new MediaKeys(this);
  bus.registerObject(object, this, QDBusConnection::ExportAdaptors);
}
#include "mpris.moc"
