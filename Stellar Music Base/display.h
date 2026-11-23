#pragma once
#include "playback.h"
#include <QTimer>
#include <QWidget>
#include <gst/fft/gstfftf32.h>
class ClassicDisplay : public QWidget {
  Q_OBJECT
  Q_PROPERTY(QColor ink MEMBER ink)
  Q_PROPERTY(QColor glass MEMBER glass)
public:
  explicit ClassicDisplay(Playback &p, QWidget *parent = nullptr);
  ~ClassicDisplay();
  void refresh();
  QString caption, details, total;
  qint64 length = 0;
  bool remaining = false;
  int mode = 0;

protected:
  void paintEvent(QPaintEvent *) override;
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
signals:
  void timeToggled();
  void modeToggled();

private:
  Playback &player;
  QTimer timer;
  GstFFTF32 *fft;
  QColor ink{"#d4f4ff"}, glass{"#073774"};
  int scroll = 0;
};
