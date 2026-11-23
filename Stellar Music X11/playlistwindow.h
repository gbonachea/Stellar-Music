#pragma once
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>
#include <QWidget>

class PlaylistWindow : public QWidget {
  Q_OBJECT
public:
  explicit PlaylistWindow(QWidget *parent = nullptr);
  QListWidget *playlist() { return m_playlist; }
  void append(const QString &path);
  QStringList paths() const;
  void highlight(const QString &path);
  void addFolder();
  void importList();
  void exportList();
  bool readList(const QString &name);
  bool writeList(const QString &name) const;

signals:
  void playSignal(const QString &index);
  void addFileSignal(const QString &filepath);

protected:
  void closeEvent(QCloseEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;
  bool eventFilter(QObject *, QEvent *) override;

private slots:
  void addAudio();
  void moveUp();
  void moveDown();
  void removeAudio();
  void playItem(QListWidgetItem *item);

private:
  void setupUi();
  void loadSavedGeometry();

  QPushButton *m_btnAdd;
  QPushButton *m_btnUp;
  QPushButton *m_btnDown;
  QPushButton *m_btnRemove;
  QListWidget *m_playlist;
  QSet<QString> indexed;
};
