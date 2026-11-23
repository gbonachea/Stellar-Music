#include "playlistwindow.h"
#include "settings.h"
#include "track.h"
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QIcon>
#include <QMimeData>
#include <QtConcurrent>
#include <QtWidgets>
class NumberedTrack : public QStyledItemDelegate {
  void initStyleOption(QStyleOptionViewItem *o,
                       const QModelIndex &i) const override {
    QStyledItemDelegate::initStyleOption(o, i);
    o->text =
        QString("%1.   %2").arg(i.row() + 1, 2, 10, QChar('0')).arg(o->text);
  }
};

PlaylistWindow::PlaylistWindow(QWidget *parent) : QWidget(parent) {
  setWindowTitle(tr("Lista de reproducción"));

  QString iconPath = getIconPath();
  if (!iconPath.isEmpty())
    setWindowIcon(QIcon(iconPath));

  m_btnAdd = new QPushButton();
  m_btnUp = new QPushButton();
  m_btnDown = new QPushButton();
  m_btnRemove = new QPushButton();
  m_playlist = new QListWidget();

  setupUi();
  loadSavedGeometry();
}

void PlaylistWindow::setupUi() {
  const int btnSize = 24;
  const int iconSize = 16;

  m_btnAdd->setIcon(QIcon::fromTheme("list-add"));
  m_btnAdd->setToolTip(tr("Agregar audio"));
  connect(m_btnAdd, &QPushButton::clicked, this, &PlaylistWindow::addAudio);

  m_btnUp->setIcon(QIcon::fromTheme("go-up"));
  m_btnUp->setToolTip(tr("Mover arriba"));
  connect(m_btnUp, &QPushButton::clicked, this, &PlaylistWindow::moveUp);

  m_btnDown->setIcon(QIcon::fromTheme("go-down"));
  m_btnDown->setToolTip(tr("Mover abajo"));
  connect(m_btnDown, &QPushButton::clicked, this, &PlaylistWindow::moveDown);

  m_btnRemove->setIcon(QIcon::fromTheme("list-remove"));
  m_btnRemove->setToolTip(tr("Eliminar audio"));
  connect(m_btnRemove, &QPushButton::clicked, this,
          &PlaylistWindow::removeAudio);

  for (QPushButton *btn : {m_btnAdd, m_btnUp, m_btnDown, m_btnRemove}) {
    btn->setFixedSize(btnSize, btnSize);
    btn->setIconSize(QSize(iconSize, iconSize));
  }

  m_playlist->setDragDropMode(QListWidget::InternalMove);
  m_playlist->viewport()->installEventFilter(this);
  connect(m_playlist->model(), &QAbstractItemModel::rowsAboutToBeRemoved, this,
          [this](QModelIndex, int first, int last) {
            for (int i = first; i <= last; ++i)
              indexed.remove(m_playlist->item(i)->toolTip());
          });
  m_playlist->setItemDelegate(new NumberedTrack);
  m_playlist->itemDelegate()->setParent(m_playlist);
  m_playlist->setDefaultDropAction(Qt::MoveAction);
  connect(m_playlist, &QListWidget::itemDoubleClicked, this,
          &PlaylistWindow::playItem);

  auto *btnLayout = new QHBoxLayout();
  for (QPushButton *btn : {m_btnAdd, m_btnUp, m_btnDown, m_btnRemove})
    btnLayout->addWidget(btn);
  btnLayout->addStretch();
  for (auto pair : {qMakePair(tr("+ Carpeta"), &PlaylistWindow::addFolder),
                    qMakePair(tr("Abrir M3U"), &PlaylistWindow::importList),
                    qMakePair(tr("Guardar"), &PlaylistWindow::exportList)}) {
    auto *b = new QPushButton(pair.first);
    btnLayout->addWidget(b);
    connect(b, &QPushButton::clicked, this, pair.second);
  }

  auto *layout = new QVBoxLayout(this);
  layout->addLayout(btnLayout);
  layout->addWidget(m_playlist);

  setAcceptDrops(true);
}

void PlaylistWindow::loadSavedGeometry() {
  auto ws = loadWindowState("playlist", 200, 200, 300, 400);
  setGeometry(ws.x, ws.y, ws.width, ws.height);
  if (ws.state)
    setWindowState(Qt::WindowStates(ws.state));
}

void PlaylistWindow::closeEvent(QCloseEvent *event) {
  saveWindowState("playlist", geometry(), windowState());
  event->accept();
}

void PlaylistWindow::dragEnterEvent(QDragEnterEvent *event) {
  // Only accept drops from outside (source == nullptr)
  if (!event->source() && event->mimeData()->hasUrls())
    event->acceptProposedAction();
  else
    event->ignore();
}

void PlaylistWindow::dropEvent(QDropEvent *event) {
  for (const QUrl &url : event->mimeData()->urls()) {
    QString path = url.toLocalFile();
    if (QDir::match(audioGlobs(), QFileInfo(path).fileName())) {
      emit addFileSignal(path);
    }
  }
  event->acceptProposedAction();
}
bool PlaylistWindow::eventFilter(QObject *o, QEvent *e) {
  if (o == m_playlist->viewport() &&
      (e->type() == QEvent::Drop || e->type() == QEvent::DragMove ||
       e->type() == QEvent::DragEnter)) {
    auto *drag = static_cast<QDropEvent *>(e);
    if (drag->mimeData()->hasUrls()) {
      if (e->type() == QEvent::Drop)
        dropEvent(drag);
      else
        drag->acceptProposedAction();
      return true;
    }
  }
  return QWidget::eventFilter(o, e);
}

void PlaylistWindow::playItem(QListWidgetItem *item) {
  int index = m_playlist->row(item);
  emit playSignal(QString::number(index));
}

void PlaylistWindow::addAudio() {
  for (auto &p : QFileDialog::getOpenFileNames(
           this, tr("Añadir audio"), {},
           tr("Audio") + " (" + audioGlobs().join(' ') + ")"))
    emit addFileSignal(p);
}

void PlaylistWindow::moveUp() {
  int current = m_playlist->currentRow();
  if (current > 0) {
    m_playlist->model()->moveRow({}, current, {}, current - 1);
    m_playlist->setCurrentRow(current - 1);
  }
}

void PlaylistWindow::moveDown() {
  int current = m_playlist->currentRow();
  if (current >= 0 && current < m_playlist->count() - 1) {
    m_playlist->model()->moveRow({}, current, {}, current + 2);
    m_playlist->setCurrentRow(current + 1);
  }
}

QStringList PlaylistWindow::paths() const {
  QStringList list;
  for (int i = 0; i < m_playlist->count(); ++i)
    list << m_playlist->item(i)->toolTip();
  return list;
}
void PlaylistWindow::append(const QString &path) {
  QFileInfo f(path);
  if (!f.isFile())
    return;
  auto abs = f.absoluteFilePath();
  if (indexed.contains(abs))
    return;
  indexed.insert(abs);
  auto *item = new QListWidgetItem(f.completeBaseName(), m_playlist);
  item->setToolTip(abs);
  auto *watch = new QFutureWatcher<Track>(this);
  QPersistentModelIndex ix =
      m_playlist->model()->index(m_playlist->row(item), 0);
  connect(watch, &QFutureWatcher<Track>::finished, this, [this, watch, ix] {
    if (ix.isValid()) {
      auto t = watch->result();
      m_playlist->item(ix.row())->setText(
          t.caption() + (t.ms ? "   ·   " + clockText(t.ms) : ""));
    }
    watch->deleteLater();
  });
  watch->setFuture(QtConcurrent::run([abs] { return Track(abs); }));
}
void PlaylistWindow::highlight(const QString &path) {
  for (int i = 0; i < m_playlist->count(); i++) {
    auto *item = m_playlist->item(i);
    auto f = item->font();
    f.setBold(item->toolTip() == path);
    item->setFont(f);
    if (f.bold())
      m_playlist->setCurrentItem(item);
  }
}
void PlaylistWindow::addFolder() {
  auto dir = QFileDialog::getExistingDirectory(this);
  if (dir.isEmpty())
    return;
  auto *watch = new QFutureWatcher<QStringList>(this);
  connect(watch, &QFutureWatcher<QStringList>::finished, this, [this, watch] {
    for (auto &p : watch->result())
      emit addFileSignal(p);
    watch->deleteLater();
  });
  watch->setFuture(QtConcurrent::run([dir] {
    QStringList list;
    QDirIterator it(dir, audioGlobs(), QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext())
      list << it.next();
    list.sort(Qt::CaseInsensitive);
    return list;
  }));
}
void PlaylistWindow::importList() {
  auto name = QFileDialog::getOpenFileName(this, tr("Añadir lista"), {},
                                           "M3U (*.m3u *.m3u8)");
  if (!name.isEmpty() && !readList(name))
    QMessageBox::warning(this, tr("Lista"), tr("No se pudo leer la lista."));
}
bool PlaylistWindow::readList(const QString &name) {
  QFile f(name);
  if (!f.open(QIODevice::ReadOnly))
    return false;
  QTextStream in(&f);
  while (!in.atEnd()) {
    auto line = in.readLine().trimmed();
    if (!line.isEmpty() && !line.startsWith('#'))
      emit addFileSignal(QFileInfo(name).dir().absoluteFilePath(line));
  }
  return true;
}
void PlaylistWindow::exportList() {
  auto name = QFileDialog::getSaveFileName(this, tr("Guardar lista"),
                                           "Stellar.m3u8", "M3U (*.m3u8 *.m3u)");
  if (name.isEmpty())
    return;
  if (!writeList(name))
    QMessageBox::warning(this, tr("Guardar lista"),
                         tr("No se pudo guardar la lista."));
}
bool PlaylistWindow::writeList(const QString &name) const {
  QSaveFile f(name);
  if (f.open(QIODevice::WriteOnly)) {
    QTextStream out(&f);
    out << "#EXTM3U\n";
    for (auto &p : paths())
      out << QFileInfo(name).dir().relativeFilePath(p) << '\n';
    out.flush();
    return out.status() == QTextStream::Ok && f.commit();
  }
  return false;
}

void PlaylistWindow::removeAudio() {
  int current = m_playlist->currentRow();
  if (current >= 0)
    delete m_playlist->takeItem(current);
}
