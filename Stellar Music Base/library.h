#pragma once
#include <QDialog>
#include <QFutureWatcher>
#include <QSqlDatabase>
#include <atomic>
class QSqlQueryModel;
class QLineEdit;
class QPushButton;
class QLabel;
class Library : public QDialog {
  Q_OBJECT
public:
  explicit Library(QWidget *parent);
  ~Library();
signals:
  void add(QString path);

private:
  void scan();
  void query();
  QString dbPath;
  QSqlDatabase db;
  QSqlQueryModel *model;
  QLineEdit *search;
  QLabel *status;
  QFutureWatcher<QString> worker;
  std::atomic<bool> cancel{false};
  int sort = 0;
  bool ascending = true;
};
