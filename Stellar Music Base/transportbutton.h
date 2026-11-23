#pragma once
#include <QPainter>
#include <QPushButton>

// Original vector hardware. Symbol geometry is independent of the desktop font.
class TransportButton final : public QPushButton {
  Q_OBJECT
  Q_PROPERTY(QColor metal MEMBER metal)
  Q_PROPERTY(QColor accent MEMBER accent)
public:
  enum Symbol { Previous, Play, Pause, Stop, Next };
  TransportButton(Symbol symbol, const QString &tip) : symbol(symbol) {
    setAccessibleName(tip);
    setToolTip(tip);
    setFixedSize(46, 46);
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::NoFocus);
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto d = qMin(width(), height());
    p.translate((width() - d) / 2., (height() - d) / 2.);
    p.scale(d / 46., d / 46.);
    auto ring = [&](int inset, QColor top, QColor bottom) {
      QLinearGradient g(8, inset, 34, 46 - inset);
      g.setColorAt(0, top);
      g.setColorAt(1, bottom);
      p.setBrush(g);
      p.setPen(Qt::NoPen);
      p.drawEllipse(
          QRectF(inset + .5, inset + .5, 45 - 2 * inset, 45 - 2 * inset));
    };
    ring(0, QColor("#6d7780"), QColor("#f1f4f5"));
    ring(2, QColor("#101e2e"), QColor("#617386"));
    ring(4, isDown() ? metal.darker(145) : Qt::white, metal.darker(145));
    ring(6,
         underMouse() && isEnabled() ? accent.lighter(170) : metal.lighter(116),
         metal.darker(isDown() ? 131 : 110));
    if (isDown())
      p.translate(.7, 1.);
    p.setBrush(symbol == Play ? accent
                              : QColor(isEnabled() ? "#112136" : "#536374"));
    p.setPen(Qt::NoPen);
    auto triangle = [&](int x, int y, int width, int height) {
      p.drawPolygon(QPolygonF{QPointF(x, y),
                              QPointF(x + width, y + height / 2.),
                              QPointF(x, y + height)});
    };
    switch (symbol) {
    case Play:
      triangle(18, 14, 15, 18);
      break;
    case Pause:
      p.drawRect(17, 16, 4, 14);
      p.drawRect(25, 16, 4, 14);
      break;
    case Stop:
      p.drawRect(16, 16, 14, 14);
      break;
    case Previous:
      triangle(28, 16, -10, 14);
      p.drawRect(15, 16, 3, 14);
      break;
    case Next:
      triangle(17, 16, 10, 14);
      p.drawRect(28, 16, 3, 14);
      break;
    }
  }

private:
  Symbol symbol;
  QColor metal{"#e0e5eb"}, accent{"#0079cf"};
};
