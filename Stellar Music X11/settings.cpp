#include "settings.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

static QString configFilePath() {
  auto dir =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  QDir().mkpath(dir);
  auto path = dir + "/setting.json";
  if (!QFile::exists(path))
    QFile::copy(QCoreApplication::applicationDirPath() + "/setting.json", path);
  return path;
}

WindowState loadWindowState(const QString &windowName, int defX, int defY,
                            int defW, int defH) {
  WindowState ws{defX, defY, defW, defH, 0};
  QString path = configFilePath();
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    return ws;

  QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  f.close();
  if (!doc.isObject())
    return ws;

  QJsonObject root = doc.object();
  if (!root.contains(windowName))
    return ws;

  QJsonObject obj = root[windowName].toObject();
  ws.x = obj.value("x").toInt(defX);
  ws.y = obj.value("y").toInt(defY);
  ws.width = obj.value("width").toInt(defW);
  ws.height = obj.value("height").toInt(defH);
  ws.state = obj.value("state").toInt(0);
  qDebug() << "Config loaded for:" << windowName;
  return ws;
}

void saveWindowState(const QString &windowName, const QRect &geometry,
                     Qt::WindowStates state) {
  QString path = configFilePath();
  QJsonObject root;

  QFile f(path);
  if (f.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (doc.isObject())
      root = doc.object();
  }

  QJsonObject obj;
  obj["x"] = geometry.x();
  obj["y"] = geometry.y();
  obj["width"] = geometry.width();
  obj["height"] = geometry.height();
  obj["state"] = static_cast<int>(state);
  root[windowName] = obj;

  QSaveFile out(path);
  if (out.open(QIODevice::WriteOnly) &&
      out.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0 &&
      out.commit()) {
    qDebug() << "Config saved for:" << windowName;
  } else {
    qWarning() << "Could not save config:" << path;
  }
}

QString loadStylesheet() {
  QString path = ":/classic.qss";
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning() << "Could not load stylesheet:" << path;
    return {};
  }
  return QString::fromUtf8(f.readAll());
}

QString getIconPath() {
  if (QFile::exists(":/icons/stellarmusic.png"))
    return ":/icons/stellarmusic.png";
  QStringList candidates = {
      QDir(QCoreApplication::applicationDirPath()).filePath("icons/stellarmusic.png"),
      QDir(QCoreApplication::applicationDirPath())
          .filePath("../icons/stellarmusic.png"),
      "icons/stellarmusic.png"};
  for (const QString &p : candidates) {
    if (QFile::exists(p)) {
      qDebug() << "Icon found at:" << p;
      return p;
    }
  }
  qDebug() << "Icon not found";
  return {};
}
