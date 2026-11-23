#include "audioplayer.h"
#include "language.h"
#include "configwindow.h"
#include "display.h"
#include "library.h"
#include "playlistwindow.h"
#include "settings.h"
#include "track.h"
#include "transportbutton.h"
#include <QWindow>
namespace {
constexpr QSize defaultPlayerSize(620, 285);
}

// Qt inherits a widget's cursor in its children. Give each strip its own
// cursor instead of setting a resize cursor on the entire player.
class AudioPlayer::ResizeGrip final : public QWidget {
public:
  ResizeGrip(QWidget *player, Qt::Edges edges) : QWidget(player), edges(edges) {
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);
    const bool horizontal = edges & (Qt::LeftEdge | Qt::RightEdge);
    const bool vertical = edges & (Qt::TopEdge | Qt::BottomEdge);
    if (horizontal && vertical)
      setCursor(edges.testFlag(Qt::LeftEdge) == edges.testFlag(Qt::TopEdge)
                    ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor);
    else
      setCursor(horizontal ? Qt::SizeHorCursor : Qt::SizeVerCursor);
    layoutStrip();
  }

  void layoutStrip() {
    const QSize s = parentWidget()->size();
    constexpr int side = 6, top = 4, bottom = 6;
    int x = side, y = top, w = s.width()-2*side, h = s.height()-top-bottom;
    if (edges & Qt::LeftEdge) { x = 0; w = side; }
    if (edges & Qt::RightEdge) { x = s.width()-side; w = side; }
    if (edges & Qt::TopEdge) { y = 0; h = top; }
    if (edges & Qt::BottomEdge) { y = s.height()-bottom; h = bottom; }
    setGeometry(x,y,w,h);
  }

protected:
  void mousePressEvent(QMouseEvent *e) override {
    if (e->button() != Qt::LeftButton) return QWidget::mousePressEvent(e);
    if (QGuiApplication::platformName() == "xcb") {
      dragging = true;
      origin = e->globalPosition().toPoint();
      initial = window()->geometry();
    } else if (auto *handle = window()->windowHandle()) {
      handle->startSystemResize(edges);
    }
    e->accept();
  }
  void mouseMoveEvent(QMouseEvent *e) override {
    if (!dragging || !(e->buttons() & Qt::LeftButton)) return;
    auto *player = window();
    const QPoint delta = e->globalPosition().toPoint() - origin;
    const QSize minimum = player->minimumSizeHint().expandedTo(player->minimumSize());
    QRect next = initial;
    if (edges & Qt::LeftEdge)
      next.setLeft(qMin(next.left()+delta.x(), next.right()-minimum.width()+1));
    if (edges & Qt::TopEdge)
      next.setTop(qMin(next.top()+delta.y(), next.bottom()-minimum.height()+1));
    if (edges & Qt::RightEdge)
      next.setWidth(qMax(minimum.width(), next.width()+delta.x()));
    if (edges & Qt::BottomEdge)
      next.setHeight(qMax(minimum.height(), next.height()+delta.y()));
    player->setGeometry(next);
  }
  void mouseReleaseEvent(QMouseEvent *e) override {
    if (e->button() == Qt::LeftButton) dragging = false;
  }
private:
  Qt::Edges edges;
  bool dragging = false;
  QPoint origin;
  QRect initial;
};
class SeekBar final : public QSlider {
public:
  SeekBar() : QSlider(Qt::Horizontal) {}

protected:
  void mousePressEvent(QMouseEvent *e) override {
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    auto *s = style();
    auto handle = s->subControlRect(QStyle::CC_Slider, &opt,
                                    QStyle::SC_SliderHandle, this);
    if (e->button() == Qt::LeftButton && !handle.contains(e->pos())) {
      auto groove = s->subControlRect(QStyle::CC_Slider, &opt,
                                      QStyle::SC_SliderGroove, this);
      int span = qMax(1, groove.width() - handle.width());
      setValue(QStyle::sliderValueFromPosition(
          minimum(), maximum(), e->pos().x() - groove.x() - handle.width() / 2,
          span, opt.upsideDown));
    }
    QSlider::mousePressEvent(e);
  }
};
class Title : public QLabel {
public:
  using QLabel::QLabel;

protected:
  void mousePressEvent(QMouseEvent *e) override {
    if (e->button() == Qt::LeftButton)
      window()->windowHandle()->startSystemMove();
  }
};
class ControlDeck final : public QWidget {
protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    double x = width() * .43, h = height();
    QPainterPath path;
    path.moveTo(1, 1);
    path.lineTo(width() - 1, 1);
    path.lineTo(width() - 1, 5);
    path.lineTo(x + 18, 5);
    path.cubicTo(x - 5, 5, x + 5, h - 1, x - 17, h - 1);
    path.lineTo(1, h - 1);
    path.closeSubpath();
    QLinearGradient g(0, 0, 0, h);
    g.setColorAt(0, QColor("#c1c8d0"));
    g.setColorAt(.6, QColor("#8b99a9"));
    g.setColorAt(1, QColor("#556475"));
    p.setBrush(g);
    p.setPen(QColor("#788696"));
    p.drawPath(path);
  }
};
void AudioPlayer::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  QStyleOption opt;
  opt.initFrom(this);
  style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
void AudioPlayer::mousePressEvent(QMouseEvent *event) {
  if (event->pos().y() < 32 && event->button() == Qt::LeftButton)
    windowHandle()->startSystemMove();
  else
    QWidget::mousePressEvent(event);
}
void AudioPlayer::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  for (auto *grip : resizeGrips) grip->layoutStrip();
}
AudioPlayer::AudioPlayer(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint) {
  setObjectName("chassis");
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_StyledBackground);
  setWindowTitle("Stellar Music");
  setWindowIcon(QIcon(getIconPath()));
  setAcceptDrops(true);
  pl = new PlaylistWindow(this);
  pl->setWindowFlag(Qt::Window);
  cfg = new ConfigWindow(this);
  setupUi();
  const Qt::Edges borders[] = {Qt::LeftEdge, Qt::RightEdge, Qt::TopEdge, Qt::BottomEdge,
      Qt::TopEdge | Qt::LeftEdge, Qt::TopEdge | Qt::RightEdge,
      Qt::BottomEdge | Qt::LeftEdge, Qt::BottomEdge | Qt::RightEdge};
  for (auto edges : borders) resizeGrips.append(new ResizeGrip(this, edges));
  setupMpris();
  connect(pl->playlist()->model(), &QAbstractItemModel::rowsRemoved, this,
          [this](QModelIndex, int first, int) {
            auto tracks = pl->paths();
            if (!current.isEmpty() && !tracks.contains(current)) {
              if (!tracks.isEmpty()) {
                select(tracks.at(qMin(first, int(tracks.size() - 1))),
                       engine.state == Playback::Playing);
                return;
              }
              engine.stop();
              current.clear();
              screen->caption.clear();
              screen->details.clear();
              screen->total.clear();
              screen->length = 0;
            }
            updateControls();
          });
  skin(prefs.value("skin").toString());
  connect(pl, &PlaylistWindow::addFileSignal, this, [this](QString path) {
    pl->append(path);
    if (current.isEmpty() && pl->playlist()->count())
      select(pl->playlist()->item(0)->toolTip(), false);
    updateControls();
  });
  connect(pl, &PlaylistWindow::playSignal, this, [this](QString index) {
    if (auto *i = pl->playlist()->item(index.toInt()))
      select(i->toolTip(), true);
  });
  connect(cfg, &ConfigWindow::equalizerChanged, this, &AudioPlayer::applyEq);
  applyEq();
  connect(&engine, &Playback::ended, this, [this] { step(1, true); });
  connect(&engine, &Playback::error, this, [this](QString e) {
    screen->caption = tr("Error: ") + e;
    updateControls();
  });
  connect(&tick, &QTimer::timeout, this, [this] {
    qint64 d = engine.duration();
    if (d > 0) {
      screen->length = d;
      screen->total = clockText(d);
    }
    if (!seek->isSliderDown()) {
      QSignalBlocker b(seek);
      seek->setValue(screen->length ? engine.position() * 10000 / screen->length
                                    : 0);
    }
  });
  if (QSystemTrayIcon::isSystemTrayAvailable()) {
    tray = new QSystemTrayIcon(windowIcon(), this);
    tray->setToolTip(windowTitle());
    auto *menu = new QMenu(this);
    menu->addAction(tr("Mostrar"), this, &QWidget::showNormal);
    menu->addAction(tr("Salir"), this, &QWidget::close);
    tray->setContextMenu(menu);
    connect(tray, &QSystemTrayIcon::activated, this, [this](auto reason) {
      if (reason == QSystemTrayIcon::Trigger ||
          reason == QSystemTrayIcon::DoubleClick) {
        showNormal();
        raise();
        activateWindow();
      }
    });
    tray->show();
  }
  for (auto &p : prefs.value("playlist").toStringList())
    pl->append(p);
  if (pl->playlist()->count()) {
    auto p = prefs.value("current").toString();
    select(pl->paths().contains(p) ? p : pl->playlist()->item(0)->toolTip(),
           false);
  }
  auto ws = loadWindowState("main_player", 100, 100, defaultPlayerSize.width(), defaultPlayerSize.height());
  QRect saved(ws.x, ws.y, ws.width, ws.height);
  // Keep negative coordinates on monitors left of the primary display.
  // Recover the title bar if its previous monitor has been disconnected.
  bool reachable = false;
  for (auto *screen : QGuiApplication::screens()) {
    const QRect title(saved.x(), saved.y(), saved.width(), 24);
    if (title.intersected(screen->availableGeometry()).width() >= 48 &&
        title.intersected(screen->availableGeometry()).height() >= 12) {
      reachable = true;
      break;
    }
  }
  if (!reachable && QGuiApplication::primaryScreen())
    saved.moveTopLeft(QGuiApplication::primaryScreen()->availableGeometry().topLeft() + QPoint(20, 20));
  setGeometry(saved);
  updateControls();
  if (!engine.failure.isEmpty())
    screen->caption = engine.failure;
}
AudioPlayer::~AudioPlayer() {
  screen->hide();
  engine.stop();
}
void AudioPlayer::setupUi() {
  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(6, 4, 6, 6);
  outer->setSpacing(1);
  auto *titleRow = new QHBoxLayout;
  outer->addLayout(titleRow);
  auto button = [this](QString text, QString tip, auto fn) {
    auto *b = new QPushButton(text);
    b->setToolTip(tip);
    b->setAccessibleName(tip);
    b->setFocusPolicy(Qt::NoFocus);
    connect(b, &QPushButton::clicked, this, fn);
    return b;
  };
  auto *brand = new QLabel;
  brand->setPixmap(windowIcon().pixmap(18));
  brand->setFixedWidth(62);
  titleRow->addWidget(brand);
  auto rail = [&] {
    auto *r = new QFrame;
    r->setObjectName("rail");
    r->setFixedHeight(4);
    titleRow->addWidget(r, 1);
  };
  rail();
  auto *title = new Title("STELLAR MUSIC");
  title->setObjectName("title");
  title->setAlignment(Qt::AlignCenter);
  title->setMinimumHeight(24);
  titleRow->addWidget(title, 2);
  rail();
  auto *mini = button("−", tr("Minimizar"), [this] { showMinimized(); });
  auto *size = button("▭", tr("Alternar tamaño"), [this] {
    if (this->size() == defaultPlayerSize)
      resize(785, 350);
    else if (this->size() == QSize(785,350))
      resize(520,250);
    else
      resize(defaultPlayerSize);
  });
  auto *resetSize = new QShortcut(QKeySequence("Ctrl+0"), this);
  connect(resetSize, &QShortcut::activated, this, [this]{ resize(defaultPlayerSize); });
  auto *closeBtn = button("×", tr("Cerrar"), [this] { close(); });
  for (auto *b : {mini, size, closeBtn}) {
    b->setFixedSize(19, 18);
    b->setObjectName("windowButton");
    titleRow->addWidget(b);
  }
  titleRow->setSpacing(3);
  auto *body = new QFrame;
  body->setObjectName("alloy");
  outer->addWidget(body, 1);
  auto *v = new QVBoxLayout(body);
  v->setContentsMargins(9, 5, 9, 8);
  v->setSpacing(7);
  auto *menus = new QMenuBar;
  menus->setNativeMenuBar(false);
  v->addWidget(menus);
  auto *file = menus->addMenu(tr("Archivo"));
  auto *transport = menus->addMenu(tr("Reproducción"));
  auto *options = menus->addMenu(tr("Opciones"));
  auto *view = menus->addMenu(tr("Ver"));
  auto *help = menus->addMenu(tr("Ayuda"));
  file->addAction(tr("Abrir audio…"), QKeySequence::Open, this,
                  &AudioPlayer::loadFile);
  file->addAction(tr("Añadir carpeta…"), pl, &PlaylistWindow::addFolder);
  file->addAction(tr("Importar lista…"), pl, &PlaylistWindow::importList);
  file->addAction(tr("Guardar lista…"), pl, &PlaylistWindow::exportList);
  file->addSeparator();
  file->addAction(tr("Salir"), QKeySequence::Quit, this, &QWidget::close);
  auto *displayRow = new QHBoxLayout;
  v->addLayout(displayRow, 1);
  screen = new ClassicDisplay(engine);
  displayRow->addWidget(screen, 1);
  auto *modes = new QVBoxLayout;
  displayRow->addLayout(modes);
  loop = qBound(0, prefs.value("repeat", 0).toInt(), 2);
  repeat = button("", tr("Secuencial / repetir pista / repetir lista"), [this] {
    loop = (loop + 1) % 3;
    updateControls();
  });
  repeat->setCheckable(true);
  shuffle = button("SHF", tr("Reproducción aleatoria"), [this] {
    visited.clear();
    history.clear();
  });
  shuffle->setCheckable(true);
  shuffle->setChecked(prefs.value("shuffle").toBool());
  modes->addStretch();
  modes->addWidget(repeat);
  modes->addWidget(shuffle);
  modes->addStretch();
  auto *progress = new QHBoxLayout;
  v->addLayout(progress);
  seek = new SeekBar;
  seek->setAccessibleName(tr("Posición"));
  seek->setRange(0, 10000);
  seek->setTracking(false);
  progress->addWidget(seek, 1);
  connect(seek, &QSlider::valueChanged, this,
          [this](int value) { engine.seek(screen->length * value / 10000); });
  auto togglePl = [this] { pl->setVisible(!pl->isVisible()); };
  progress->addWidget(button("⏏", tr("Abrir archivo"), [this] { loadFile(); }));
  progress->addWidget(button("PL", tr("Lista de reproducción"), togglePl));
  progress->addWidget(
      button("ML", tr("Biblioteca"), [this] { toggleLibrary(); }));
  auto *shelf = new ControlDeck;
  auto *deck = new QHBoxLayout(shelf);
  deck->setContentsMargins(3, 4, 3, 3);
  v->addWidget(shelf);
  auto go = [this] {
    if (current.isEmpty()) {
      loadFile();
      return;
    }
    engine.play();
    updateControls();
  };
  auto hold = [this] {
    engine.pause();
    updateControls();
  };
  auto halt = [this] {
    engine.stop();
    updateControls();
  };
  auto round = [&](TransportButton::Symbol symbol, QString tip, auto fn) {
    auto *b = new TransportButton(symbol, tip);
    connect(b, &QPushButton::clicked, this, fn);
    deck->addWidget(b);
    return b;
  };
  prev = round(TransportButton::Previous, tr("Anterior"), [this] { step(-1); });
  play = round(TransportButton::Play, tr("Reproducir"), go);
  pause = round(TransportButton::Pause, tr("Pausar"), hold);
  stop = round(TransportButton::Stop, tr("Detener"), halt);
  next = round(TransportButton::Next, tr("Siguiente"), [this] { step(1); });
  play->setObjectName("play");
  deck->addSpacing(5);
  mute = button("VOL", tr("Silenciar"),
                [this] { engine.volume(volume->value(), mute->isChecked()); });
  mute->setCheckable(true);
  deck->addWidget(mute);
  volume = new QSlider(Qt::Horizontal);
  volume->setAccessibleName(tr("Volumen"));
  volume->setRange(0, 100);
  volume->setValue(prefs.value("volume", 75).toInt());
  engine.volume(volume->value(), false);
  deck->addWidget(volume, 1);
  connect(volume, &QSlider::valueChanged, this,
          [this](int v) { engine.volume(v, mute->isChecked()); });
  deck->addWidget(button("EQ", tr("Ecualizador"), [this] { showConfig(1); }));
  deck->addWidget(
      button("CONFIG", tr("Configuración"), [this] { showConfig(0); }));
  transport->addAction(tr("Reproducir / Pausar"), QKeySequence(Qt::Key_Space),
                       this, [this, go, hold] {
                         if (engine.state == Playback::Playing)
                           hold();
                         else
                           go();
                       });
  transport->addAction(tr("Detener"), QKeySequence("S"), this, halt);
  transport->addAction(tr("Anterior"), QKeySequence("Z"), prev,
                       &QPushButton::click);
  transport->addAction(tr("Siguiente"), QKeySequence("B"), next,
                       &QPushButton::click);
  options->addAction(tr("Configuración"), this, [this] { showConfig(0); });
  auto *languages = options->addMenu(tr("Idioma (al volver a abrir)"));
  auto *choices = new QActionGroup(languages);
  auto *separator = languages->addSeparator();
  auto addChoice = [this, languages, choices, separator](const QString &code, const QString &name) {
    for (auto *a : choices->actions()) if (a->data().toString()==code) {a->setText(name);return a;}
    auto *choice = new QAction(name, languages);
    languages->insertAction(separator, choice);
    choice->setData(code);
    choices->addAction(choice);
    choice->setCheckable(true);
    connect(choice, &QAction::triggered, this, [this, code]{ prefs.setValue("language", code); });
    return choice;
  };
  const auto installed=availableLanguages();
  const auto selected=prefs.value("language","es").toString();
  for(auto i=installed.begin();i!=installed.end();++i)addChoice(i.key(),i.value())->setChecked(i.key()==(installed.contains(selected)?selected:"es"));
  languages->addAction(tr("Agregar idioma…"),this,[this,addChoice]{
    auto path=QFileDialog::getOpenFileName(this,tr("Agregar idioma…"),{},tr("Paquetes de idioma (*.herolang)"));
    if(path.isEmpty())return;
    LanguagePack pack;QString error;
    if(!installLanguagePack(path,pack,error)){QMessageBox::warning(this,tr("Agregar idioma…"),error);return;}
    addChoice(pack.code,pack.name)->trigger();
  });
  options->addAction(tr("Cargar tema QSS…"), this, [this] {
    auto path =
        QFileDialog::getOpenFileName(this, tr("Tema"), {}, "QSS (*.qss)");
    if (!path.isEmpty())
      skin(path);
  });
  options->addAction("Stellar Classic Blue", this, [this] { skin({}); });
  auto *notify = options->addAction(tr("Notificar cambios de canción"));
  notify->setCheckable(true);
  notify->setChecked(prefs.value("notify").toBool());
  connect(notify, &QAction::toggled, this,
          [this](bool on) { prefs.setValue("notify", on); });
  view->addAction(tr("Lista"), QKeySequence("Alt+P"), this, togglePl);
  view->addAction(tr("Biblioteca"), QKeySequence("Alt+L"), this,
                  &AudioPlayer::toggleLibrary);
  view->addAction(tr("Ecualizador"), QKeySequence("Alt+E"), this,
                  [this] { showConfig(1); });
  screen->mode = qBound(0, prefs.value("visual", 0).toInt(), 2);
  screen->remaining = prefs.value("remaining").toBool();
  auto cycle = [this] {
    screen->mode = (screen->mode + 1) % 3;
    screen->refresh();
  };
  auto clock = [this] {
    screen->remaining = !screen->remaining;
    screen->update();
  };
  view->addAction(tr("Espectro / onda / apagado"), QKeySequence("V"), this,
                  cycle);
  view->addAction(tr("Tiempo transcurrido / restante"), QKeySequence("T"), this,
                  clock);
  connect(screen, &ClassicDisplay::modeToggled, this, cycle);
  connect(screen, &ClassicDisplay::timeToggled, this, clock);
  help->addAction(tr("Acerca de Stellar Music"), this, [this] { showConfig(2); });
}
void AudioPlayer::skin(QString path) {
  QString style = loadStylesheet();
  if (!path.isEmpty()) {
    QFile f(path);
    if (f.open(QIODevice::ReadOnly))
      style += '\n' + QString::fromUtf8(f.readAll());
    else
      path.clear();
  }
  qApp->setStyleSheet(style);
  prefs.setValue("skin", path);
}
void AudioPlayer::applyEq() {
  QSettings s("Player", "AudioPlayer");
  auto m = s.value("equalizer").toMap();
  engine.equalize(m, s.value("eqEnabled", true).toBool(),
                  m.value("Pre").toInt());
}
void AudioPlayer::toggleLibrary() {
  if (!library) {
    library = new Library(this);
    connect(library, &Library::add, pl, &PlaylistWindow::addFileSignal);
  }
  library->setVisible(!library->isVisible());
}
void AudioPlayer::showConfig(int index) {
  auto *tabs = cfg->findChild<QTabWidget *>();
  bool hide = cfg->isVisible() && tabs->currentIndex() == index;
  tabs->setCurrentIndex(index);
  cfg->setVisible(!hide);
}
void AudioPlayer::loadFile() {
  openFiles(QFileDialog::getOpenFileNames(this, tr("Abrir audio"), {},
                                          tr("Audio (%1);;Todos (*)").arg(audioGlobs().join(' '))));
}
void AudioPlayer::openFiles(const QStringList &paths) {
  QString first;
  for (auto &p : paths)
    if (QFileInfo(p).isFile()) {
      auto path = QFileInfo(p).absoluteFilePath();
      pl->append(path);
      if (first.isEmpty())
        first = path;
    }
  if (!first.isEmpty())
    select(first, true);
}
void AudioPlayer::select(const QString &path, bool start) {
  current = path;
  engine.open(path);
  Track t(path);
  screen->caption = t.caption();
  screen->length = t.ms;
  screen->total = t.ms ? clockText(t.ms) : QString();
  QStringList info;
  if (t.bitrate)
    info << QString::number(t.bitrate) + " kbps";
  if (t.rate)
    info << QString::number(t.rate / 1000., 'g', 4) + " kHz";
  if (t.channels)
    info << (t.channels == 1   ? "MONO"
             : t.channels == 2 ? tr("ESTÉREO")
                               : QString::number(t.channels) + " CH");
  screen->details = info.join("   ");
  pl->highlight(path);
  if (start) {
    engine.play();
    if (tray && prefs.value("notify").toBool())
      tray->showMessage("Stellar Music", screen->caption);
  }
  updateControls();
}
void AudioPlayer::step(int dir, bool automatic) {
  auto paths = pl->paths();
  if (paths.isEmpty()) {
    engine.stop();
    updateControls();
    return;
  }
  int ix = paths.indexOf(current);
  QString target;
  if (automatic && loop == 1 && ix >= 0)
    target = current;
  else if (shuffle->isChecked()) {
    while (!history.isEmpty() && !paths.contains(history.last()))
      history.removeLast();
    if (dir < 0 && !history.isEmpty())
      target = history.takeLast();
    else {
      if (!visited.contains(current))
        visited << current;
      auto pool = paths;
      for (auto &p : visited)
        pool.removeAll(p);
      if (pool.isEmpty()) {
        if (automatic && !loop) {
          visited.clear();
          updateControls();
          return;
        }
        visited.clear();
        pool = paths;
        if (pool.size() > 1)
          pool.removeAll(current);
      }
      target = pool.at(QRandomGenerator::global()->bounded(int(pool.size())));
      if (history.size() >= paths.size())
        history.removeFirst();
      history << current;
    }
  } else {
    int row = ix + dir;
    if (automatic && row >= paths.size() && !loop) {
      updateControls();
      return;
    }
    target = paths.at((row + paths.size()) % paths.size());
  }
  select(target, true);
}
void AudioPlayer::updateControls() {
  repeat->setText(QStringList{"SEQ", "R·1", "R·ALL"}.at(loop));
  repeat->setChecked(loop != 0);
  bool ready = !current.isEmpty() && engine.failure.isEmpty();
  play->setEnabled(ready && engine.state != Playback::Playing);
  pause->setEnabled(engine.state == Playback::Playing);
  stop->setEnabled(engine.state != Playback::Stopped);
  seek->setEnabled(engine.state != Playback::Stopped);
  prev->setEnabled(pl->playlist()->count() > 0);
  next->setEnabled(prev->isEnabled());
  if (engine.state == Playback::Playing)
    tick.start(150);
  else
    tick.stop();
  if (engine.state == Playback::Stopped) {
    QSignalBlocker block(seek);
    seek->setValue(0);
  }
  screen->refresh();
  emit updated();
}
void AudioPlayer::persist() {
  prefs.setValue("playlist", pl->paths());
  prefs.setValue("current", current);
  prefs.setValue("volume", volume->value());
  prefs.setValue("shuffle", shuffle->isChecked());
  prefs.setValue("repeat", loop);
  prefs.setValue("visual", screen->mode);
  prefs.setValue("remaining", screen->remaining);
  QRect saved = geometry();
  if ((isMinimized() || isMaximized() || isFullScreen()) && normalGeometry().isValid())
    saved = normalGeometry();
  // A launch explicitly using native Wayland should not overwrite a usable
  // X11 position with the synthetic (0, 0) that Wayland clients usually see.
  if (QGuiApplication::platformName().startsWith("wayland")) {
    const auto previous = loadWindowState("main_player",100,100,width(),height());
    saved.moveTopLeft(QPoint(previous.x,previous.y));
  }
  saveWindowState("main_player", saved, Qt::WindowNoState);
  pl->close();
  cfg->close();
  if (library)
    library->close();
}
void AudioPlayer::closeEvent(QCloseEvent *e) {
  e->accept();
  if (closing) return;
  closing = true;
  if (tray) tray->hide();
  tick.stop();
  engine.stop();
  persist();
  QCoreApplication::exit(0);
}
void AudioPlayer::changeEvent(QEvent *e) {
  if (!closing && e->type() == QEvent::WindowStateChange && isMinimized() && tray &&
      QSettings("Player", "AudioPlayer").value("minimize_to_tray").toBool())
    hide();
  QWidget::changeEvent(e);
}
void AudioPlayer::dragEnterEvent(QDragEnterEvent *e) {
  if (e->mimeData()->hasUrls())
    e->acceptProposedAction();
}
void AudioPlayer::dropEvent(QDropEvent *e) {
  QStringList files;
  for (auto &u : e->mimeData()->urls())
    if (QDir::match(audioGlobs(), u.fileName()))
      files << u.toLocalFile();
  openFiles(files);
  e->acceptProposedAction();
}
