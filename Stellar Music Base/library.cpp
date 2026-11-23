#include "library.h"
#include "track.h"
#include <QtConcurrent>
#include <QtSql>
#include <QtWidgets>
Library::Library(QWidget *parent) : QDialog(parent) {
  setWindowTitle(tr("Stellar Music · Biblioteca"));
  resize(620, 350);
  QString dir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir().mkpath(dir);
  dbPath = dir + "/library.sqlite";
  db = QSqlDatabase::addDatabase("QSQLITE", "library-view");
  db.setDatabaseName(dbPath);
  db.open();
  QSqlQuery init(db);
  init.exec("PRAGMA journal_mode=WAL");
  init.exec("CREATE TABLE IF NOT EXISTS tracks(path TEXT PRIMARY KEY,title "
            "TEXT,artist TEXT,album TEXT)");
  for (QString field : {"title", "artist", "album"})
    init.exec("CREATE INDEX IF NOT EXISTS sort_" + field + " ON tracks(" +
              field + " COLLATE NOCASE)");
  model = new QSqlQueryModel(this);
  search = new QLineEdit;
  search->setPlaceholderText(tr("Buscar título, artista o álbum…"));
  auto *view = new QTableView;
  view->setModel(model);
  view->setSelectionBehavior(QAbstractItemView::SelectRows);
  view->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  auto *layout = new QVBoxLayout(this);
  layout->addWidget(search);
  layout->addWidget(view);
  auto *row = new QHBoxLayout;
  layout->addLayout(row);
  status = new QLabel;
  if (!db.isOpen())
    status->setText(db.lastError().text());
  auto button = [&](QString t, auto fn) {
    auto *b = new QPushButton(t);
    row->addWidget(b);
    connect(b, &QPushButton::clicked, this, fn);
  };
  button(tr("+ Carpeta"), [this] {
    auto folder = QFileDialog::getExistingDirectory(this);
    if (folder.isEmpty())
      return;
    QSettings s;
    auto roots = s.value("roots").toStringList();
    if (!roots.contains(folder))
      roots << folder;
    s.setValue("roots", roots);
    scan();
  });
  button(tr("Actualizar"), [this] { scan(); });
  auto append = [this, view] {
    for (auto ix : view->selectionModel()->selectedRows())
      emit add(model->record(ix.row()).value(3).toString());
  };
  button(tr("+ Lista"), append);
  connect(view, &QTableView::doubleClicked, this, append);
  row->addWidget(status);
  connect(search, &QLineEdit::textChanged, this, &Library::query);
  connect(view->horizontalHeader(), &QHeaderView::sectionClicked, this,
          [this](int i) {
            ascending = i != sort || !ascending;
            sort = i;
            query();
          });
  connect(&worker, &QFutureWatcher<QString>::finished, this, [this] {
    status->setText(worker.result());
    query();
  });
  query();
  view->hideColumn(3);
  restoreGeometry(QSettings().value("libraryGeometry").toByteArray());
}
void Library::query() {
  QStringList fields{"title", "artist", "album"};
  QSqlQuery q(db);
  q.prepare("SELECT title,artist,album,path FROM tracks WHERE title LIKE ? OR "
            "artist LIKE ? OR album LIKE ? ORDER BY " +
            fields.value(sort, "title") + " COLLATE NOCASE " +
            (ascending ? "ASC" : "DESC"));
  for (int i = 0; i < 3; i++)
    q.addBindValue("%" + search->text() + "%");
  q.exec();
  model->setQuery(std::move(q));
  model->setHeaderData(0, Qt::Horizontal, tr("Título"));
  model->setHeaderData(1, Qt::Horizontal, tr("Artista"));
  model->setHeaderData(2, Qt::Horizontal, tr("Álbum"));
}
void Library::scan() {
  if (worker.isRunning())
    return;
  auto roots = QSettings().value("roots").toStringList();
  status->setText(tr("Indexando…"));
  worker.setFuture(QtConcurrent::run([this, roots]() -> QString {
    QString result;
    {
      auto conn = QSqlDatabase::addDatabase("QSQLITE", "library-index");
      conn.setDatabaseName(dbPath);
      if (!conn.open() || !conn.transaction())
        result = conn.lastError().text();
      else {
        QSqlQuery q(conn);
        q.exec("DELETE FROM tracks");
        q.prepare("INSERT OR IGNORE INTO tracks VALUES(?,?,?,?)");
        for (auto &root : roots) {
          QDirIterator it(root, audioGlobs(), QDir::Files,
                          QDirIterator::Subdirectories);
          while (!cancel && it.hasNext()) {
            auto path = it.next();
            Track t(path);
            q.bindValue(0, path);
            q.bindValue(1, t.title);
            q.bindValue(2, t.artist);
            q.bindValue(3, t.album);
            if (!q.exec()) {
              result = q.lastError().text();
              break;
            }
          }
          if (cancel || !result.isEmpty())
            break;
        }
        if (cancel || !result.isEmpty())
          conn.rollback();
        else if (!conn.commit())
          result = conn.lastError().text();
      }
    }
    QSqlDatabase::removeDatabase("library-index");
    return result.isEmpty() ? tr("Índice actualizado") : result;
  }));
}
Library::~Library() {
  cancel = true;
  worker.waitForFinished();
  QSettings().setValue("libraryGeometry", saveGeometry());
  delete model;
  db = {};
  QSqlDatabase::removeDatabase("library-view");
}
