#pragma once
#include "playback.h"
#include <QtWidgets>
class PlaylistWindow;
class ConfigWindow;
class Library;
class ClassicDisplay;
class AudioPlayer : public QWidget {
  Q_OBJECT
public:
  explicit AudioPlayer(QWidget *parent = nullptr);
  ~AudioPlayer();
  void openFiles(const QStringList &paths);

protected:
  void paintEvent(QPaintEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void resizeEvent(QResizeEvent *) override;
  void closeEvent(QCloseEvent *) override;
  void changeEvent(QEvent *) override;
  void dragEnterEvent(QDragEnterEvent *) override;
  void dropEvent(QDropEvent *) override;

signals:
  void updated();

private:
  friend class MediaKeys;
  void setupMpris();
  void setupUi();
  void loadFile();
  void select(const QString &, bool start);
  void step(int dir, bool automatic = false);
  void updateControls();
  void applyEq();
  void persist();
  void skin(QString path);
  void showConfig(int tab);
  void toggleLibrary();
  Playback engine;
  QSettings prefs;
  PlaylistWindow *pl;
  ConfigWindow *cfg;
  Library *library = nullptr;
  ClassicDisplay *screen;
  QSystemTrayIcon *tray = nullptr;
  QSlider *seek, *volume;
  QPushButton *repeat, *shuffle, *mute, *play, *pause, *stop, *prev, *next;
  QTimer tick;
  QString current;
  QStringList visited, history;
  int loop = 0;
  bool closing = false;
  class ResizeGrip;
  QList<ResizeGrip *> resizeGrips;
};
