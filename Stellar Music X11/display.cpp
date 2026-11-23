#include "display.h"
#include "track.h"
#include <QMouseEvent>
#include <QPainter>
#include <cmath>
ClassicDisplay::ClassicDisplay(Playback &p, QWidget *parent)
    : QWidget(parent), player(p), fft(gst_fft_f32_new(1024, FALSE)) {
  setObjectName("glass");
  setMinimumSize(320, 95);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setFont(QFont("Monospace", 12));
  setToolTip(tr("Pulsa el reloj para alternar el tiempo o el espectro para "
                "cambiar de vista"));
  connect(&timer, &QTimer::timeout, this, [this] {
    ++scroll;
    update();
  });
}
ClassicDisplay::~ClassicDisplay() { gst_fft_f32_free(fft); }
void ClassicDisplay::refresh() {
  bool run = isVisible() && player.state == Playback::Playing;
  player.capture = run && mode != 2;
  if (run) {
    int interval =
        mode != 2 || fontMetrics().horizontalAdvance(caption) > width() - 24
            ? 40
            : 250;
    if (timer.interval() != interval || !timer.isActive())
      timer.start(interval);
  } else
    timer.stop();
  update();
}
void ClassicDisplay::showEvent(QShowEvent *) { refresh(); }
void ClassicDisplay::hideEvent(QHideEvent *) {
  timer.stop();
  player.capture = false;
}
void ClassicDisplay::mousePressEvent(QMouseEvent *e) {
  if (e->pos().x() > width() * 0.64)
    emit modeToggled();
  else
    emit timeToggled();
}
void ClassicDisplay::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  QRectF frame = rect().adjusted(2, 2, -2, -2);
  int w = width(), h = height(), line = h * 0.66;
  QLinearGradient bg(0, 0, 0, h);
  bg.setColorAt(0, QColor("#071426"));
  bg.setColorAt(.065, glass.lighter(185));
  bg.setColorAt(.16, glass);
  bg.setColorAt(.65, glass.darker(125));
  bg.setColorAt(.66, glass.darker(225));
  bg.setColorAt(.94, glass.darker(255));
  bg.setColorAt(1, glass.lighter(180));
  p.setBrush(bg);
  QLinearGradient rim(0, 0, 0, h);
  rim.setColorAt(0, QColor("#263647"));
  rim.setColorAt(.5, QColor("#596474"));
  rim.setColorAt(1, QColor("#edf2f6"));
  p.setPen(QPen(rim, 4));
  p.drawRoundedRect(frame, 12, 12);
  static const QPixmap dots = [] {
    QPixmap tile(4, 4);
    tile.fill(Qt::transparent);
    QPainter dot(&tile);
    dot.setPen(QColor(115, 197, 255, 32));
    dot.drawPoint(1, 1);
    return tile;
  }();
  p.drawTiledPixmap(rect().adjusted(9, 9, -9, -9), dots);
  p.setPen(ink);
  QFont f = font();
  f.setPixelSize(qBound(21, line / 2, 42));
  p.setFont(f);
  qint64 pos = player.position();
  auto time =
      (remaining ? "−" : "") + clockText(remaining ? length - pos : pos);
  // Seven segments are drawn in logical coordinates, including the minus sign.
  p.save();
  const double scale = qMin((w / 3. - 24) / (time.size() * 13. - 6),
                            qMin(2.0, (line - 18) / 21.));
  p.translate(14, (line - 20 * scale) / 2. + 3);
  p.scale(scale, scale);
  const QLineF bars[] = {{2, 0, 10, 0},   {12, 2, 12, 8}, {12, 12, 12, 18},
                         {2, 20, 10, 20}, {0, 12, 0, 18}, {0, 2, 0, 8},
                         {2, 10, 10, 10}};
  constexpr int digits[] = {63, 6, 91, 79, 102, 109, 125, 7, 127, 111};
  p.setPen(QPen(ink, 1.65, Qt::SolidLine, Qt::RoundCap));
  for (auto c : time) {
    if (c == ':') {
      p.drawPoint(1, 6);
      p.drawPoint(1, 15);
      p.translate(6, 0);
      continue;
    }
    int bits = c.isDigit() ? digits[c.digitValue()] : 64;
    for (int i = 0; i < 7; i++)
      if (bits & (1 << i))
        p.drawLine(bars[i]);
    p.translate(16, 0);
  }
  p.restore();
  f.setPixelSize(11);
  f.setBold(true);
  p.setFont(f);
  const auto fields = details.split("   ", Qt::SkipEmptyParts);
  int chipX = w / 3, chipY = line / 2 - 15;
  for (int i = 0; i < fields.size(); ++i) {
    if (i == 2) {
      chipX = w / 3;
      chipY += 19;
    }
    QString value = fields.at(i);
    int cw = p.fontMetrics().horizontalAdvance(value) + 8;
    p.setPen(Qt::NoPen);
    p.setBrush(ink);
    p.drawRoundedRect(QRect(chipX, chipY, cw, 15), 3, 3);
    p.setPen(glass.darker(190));
    p.drawText(QRect(chipX, chipY, cw, 15), Qt::AlignCenter, value);
    chipX += cw + 5;
  }
  p.setPen(ink);
  if (!total.isEmpty())
    p.drawText(chipX, chipY + 12, total);
  int left = w * 0.66, n = w - left - 12;
  if (mode != 2 && player.state != Playback::Stopped) {
    auto wave = player.samples();
    if (mode == 1) {
      QPolygon points;
      for (int i = 0; i < n; i++)
        points << QPoint(left + i,
                         line / 2 - wave[i * 1024 / n] * (line / 2 - 6));
      p.drawPolyline(points);
    } else {
      std::array<GstFFTF32Complex, 513> bins;
      gst_fft_f32_window(fft, wave.data(), GST_FFT_WINDOW_HANN);
      gst_fft_f32_fft(fft, wave.data(), bins.data());
      const int bars = qBound(12, n / 5, 48);
      for (int b = 0; b < bars; b++) {
        int lo = std::pow(512., double(b) / bars),
            hi = qMax(lo + 1, int(std::pow(512., double(b + 1) / bars)));
        double peak = 0;
        for (int k = lo; k < hi; k++)
          peak = std::max(peak, double(std::hypot(bins[k].r, bins[k].i)) / 256);
        int level = qBound(0., (20 * std::log10(peak + 1e-9) + 65) / 65., 1.) *
                    (line - 12);
        for (int y = 0; y < level; y += 4)
          p.fillRect(left + b * n / bars, line - 4 - y, qMax(2, n / bars - 2),
                     2, y + 5 >= level ? ink : glass.lighter(235));
      }
    }
  }
  f = font();
  f.setPixelSize(qBound(13, h / 6, 22));
  f.setLetterSpacing(QFont::AbsoluteSpacing, 1.3);
  p.setFont(f);
  p.setPen(ink);
  auto text = caption.isEmpty() ? tr("STELLAR MUSIC   /   LISTO") : caption;
  QRect strip(12, line, w - 24, h - line - 3);
  p.setClipRect(strip);
  int tw = p.fontMetrics().horizontalAdvance(text);
  int off = tw > strip.width() ? scroll % (tw + 42) : 0;
  int y = strip.center().y() +
          (p.fontMetrics().ascent() - p.fontMetrics().descent()) / 2;
  // Perforated phosphor texture, anchored to the glyphs during scrolling.
  QPixmap matrix(3, 3);
  matrix.fill(ink);
  {
    QPainter dot(&matrix);
    dot.setPen(glass.darker(130));
    dot.drawPoint(2, 2);
  }
  p.setPen(QPen(QBrush(matrix), 1));
  p.setBrushOrigin(strip.left() - off, strip.top());
  p.drawText(strip.left() - off, y, text);
  if (off)
    p.drawText(strip.left() - off + tw + 42, y, text);
}
