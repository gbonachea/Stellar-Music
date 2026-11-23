#include "playback.h"
#include <QUrl>
#include <QVariant>
#include <cmath>

Playback::Playback() {
  gst_init(nullptr, nullptr);
  player = gst_element_factory_make("playbin", nullptr);
  GError *err = nullptr;
  filter = gst_parse_bin_from_description(
      "audioconvert ! audio/x-raw,format=F32LE,channels=2,layout=interleaved ! "
      "volume name=pre ! equalizer-nbands name=eq num-bands=10 ! audiodynamic "
      "threshold=0.95 ratio=0 characteristics=soft-knee ! identity "
      "name=tap ! audioconvert",
      TRUE, &err);
  if (err || !player || !filter) {
    failure = err ? QString::fromUtf8(err->message) : tr("Falta playbin");
    g_clear_error(&err);
    return;
  }
  gst_object_ref_sink(filter);
  eq = gst_bin_get_by_name(GST_BIN(filter), "eq");
  gain = gst_bin_get_by_name(GST_BIN(filter), "pre");
  tap = gst_bin_get_by_name(GST_BIN(filter), "tap");
  for (unsigned i = 0; i < centers.size(); ++i) {
    auto band = gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(eq), i);
    g_object_set(band, "freq", double(centers[i]), "bandwidth",
                 centers[i] * 0.7071, nullptr);
    g_object_unref(band);
  }
  g_signal_connect(tap, "handoff", G_CALLBACK(handoff), this);
  guint flags = 0;
  g_object_get(player, "flags", &flags, nullptr);
  // Enable audio filtering, disable video and subtitles (including cover art).
  g_object_set(player, "flags", (flags | 0x802u) & ~5u, "audio-filter", filter,
               nullptr);
  bus = gst_element_get_bus(player);
  connect(&timer, &QTimer::timeout, this, &Playback::drain);
}
Playback::~Playback() {
  if (player)
    gst_element_set_state(player, GST_STATE_NULL);
  for (auto *obj : {tap, eq, gain, filter, player})
    if (obj)
      gst_object_unref(obj);
  if (bus)
    gst_object_unref(bus);
}
void Playback::open(const QString &path) {
  stop();
  if (!failure.isEmpty())
    return;
  g_object_set(player, "uri", QUrl::fromLocalFile(path).toEncoded().constData(),
               nullptr);
}
void Playback::play() {
  if (failure.isEmpty()) {
    state = Playing;
    gst_element_set_state(player, GST_STATE_PLAYING);
    timer.start(50);
  }
}
void Playback::pause() {
  if (state == Playing) {
    gst_element_set_state(player, GST_STATE_PAUSED);
    state = Paused;
  }
}
void Playback::stop() {
  timer.stop();
  if (player)
    gst_element_set_state(player, GST_STATE_NULL);
  state = Stopped;
  std::lock_guard<std::mutex> g(lock);
  ring.fill(0);
  cursor = 0;
}
bool Playback::seek(qint64 ms) {
  bool ok = state != Stopped &&
            gst_element_seek_simple(
                player, GST_FORMAT_TIME,
                GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                ms * GST_MSECOND);
  if (ok)
    emit seeked(ms);
  return ok;
}
qint64 Playback::position() const {
  gint64 t;
  return state != Stopped &&
                 gst_element_query_position(player, GST_FORMAT_TIME, &t)
             ? t / GST_MSECOND
             : 0;
}
qint64 Playback::duration() const {
  gint64 t;
  return state != Stopped &&
                 gst_element_query_duration(player, GST_FORMAT_TIME, &t)
             ? t / GST_MSECOND
             : 0;
}
void Playback::volume(int v, bool muted) {
  if (player)
    g_object_set(player, "volume", v / 100.0, "mute", gboolean(muted), nullptr);
}
void Playback::equalize(const QVariantMap &values, bool enabled, int preamp) {
  if (!eq)
    return;
  double boost = 0;
  for (unsigned i = 0; i < centers.size(); ++i) {
    double db = enabled
                    ? qBound(-12, values.value(bandKey(centers[i])).toInt(), 12)
                    : 0;
    auto band = gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(eq), i);
    g_object_set(band, "gain", db, nullptr);
    g_object_unref(band);
    boost = std::max(boost, db);
  }
  g_object_set(gain, "volume",
               std::pow(10., (enabled ? (preamp - boost) : 0.) / 20.), nullptr);
}
void Playback::drain() {
  while (auto *msg = gst_bus_pop(bus)) {
    auto type = GST_MESSAGE_TYPE(msg);
    QString text;
    if (type == GST_MESSAGE_ERROR) {
      GError *e = nullptr;
      gst_message_parse_error(msg, &e, nullptr);
      text = QString::fromUtf8(e->message);
      g_error_free(e);
    }
    gst_message_unref(msg);
    if (type == GST_MESSAGE_EOS) {
      stop();
      emit ended();
      break;
    }
    if (type == GST_MESSAGE_ERROR) {
      stop();
      emit error(text);
      break;
    }
  }
}
void Playback::handoff(GstElement *, GstBuffer *buf, gpointer ptr) {
  auto &self = *static_cast<Playback *>(ptr);
  if (!self.capture.load(std::memory_order_relaxed))
    return;
  GstMapInfo m;
  if (!gst_buffer_map(buf, &m, GST_MAP_READ))
    return;
  auto *v = reinterpret_cast<const float *>(m.data);
  size_t n = m.size / (2 * sizeof(float));
  {
    std::lock_guard<std::mutex> g(self.lock);
    for (size_t i = n > 1024 ? n - 1024 : 0; i < n; ++i)
      self.ring[self.cursor++ % 1024] = (v[2 * i] + v[2 * i + 1]) * 0.5f;
  }
  gst_buffer_unmap(buf, &m);
}
std::array<float, 1024> Playback::samples() {
  std::array<float, 1024> out;
  std::lock_guard<std::mutex> g(lock);
  for (size_t i = 0; i < out.size(); ++i)
    out[i] = ring[(cursor + i) % 1024];
  return out;
}
