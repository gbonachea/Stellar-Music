#include "language.h"
#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QLibraryInfo>
#include <QLocale>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

namespace {
QString folder() {
  return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
         "/languages";
}
bool readPack(const QString &path, LanguagePack &pack, QString &error) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    error = file.errorString();
    return false;
  }
  constexpr int limit = 2 * 1024 * 1024;
  const auto bytes = file.read(limit + 1);
  const auto obj = bytes.size() <= limit
                       ? QJsonDocument::fromJson(bytes).object()
                       : QJsonObject{};
  pack = {obj.value("locale").toString(),
          obj.value("name").toString().trimmed(),
          obj.value("messages").toObject()};
  static const QRegularExpression locale(
      "^[a-z]{2,3}(?:_[A-Z][a-z]{3})?(?:_[A-Z]{2}|_[0-9]{3})?$");
  bool valid = obj.value("format") == "hero-language-1" &&
               locale.match(pack.code).hasMatch() && pack.code != "es" &&
               pack.code != "pt_BR" && !pack.name.isEmpty() &&
               pack.name.size() <= 80 && !pack.messages.isEmpty();
  for (auto ctx = pack.messages.begin(); valid && ctx != pack.messages.end();
       ++ctx) {
    const auto strings = ctx.value().toObject();
    valid = !ctx.key().isEmpty() && !strings.isEmpty();
    for (auto m = strings.begin(); valid && m != strings.end(); ++m)
      valid = !m.key().isEmpty() && m.value().isString() &&
              !m.value().toString().trimmed().isEmpty();
  }
  if (!valid)
    error = QCoreApplication::translate(
        "LanguagePack", "Paquete no válido o idioma reservado.");
  return valid;
}
} // namespace
QMap<QString, QString> availableLanguages() {
  QMap<QString, QString> choices{{"es", "Español"},
                                 {"pt_BR", "Português (Brasil)"}};
  QDir dir(folder());
  for (auto &name : dir.entryList({"*.herolang"}, QDir::Files)) {
    LanguagePack p;
    QString error;
    if (readPack(dir.filePath(name), p, error) && name == p.code + ".herolang")
      choices.insert(p.code, p.name);
  }
  return choices;
}
QString preferredLanguage() {
  auto code = QSettings().value("language", "es").toString();
  return availableLanguages().contains(code) ? code : QStringLiteral("es");
}
bool installLanguagePack(const QString &path, LanguagePack &pack,
                         QString &error) {
  if (!readPack(path, pack, error))
    return false;
  QDir().mkpath(folder());
  QSaveFile out(folder() + "/" + pack.code + ".herolang");
  const auto bytes = QJsonDocument(QJsonObject{{"format", "hero-language-1"},
                                               {"locale", pack.code},
                                               {"name", pack.name},
                                               {"messages", pack.messages}})
                         .toJson();
  if (!out.open(QIODevice::WriteOnly) || out.write(bytes) != bytes.size() ||
      !out.commit()) {
    error = out.errorString();
    return false;
  }
  return true;
}
QString PackageTranslator::translate(const char *ctx, const char *src,
                                     const char *, int) const {
  return messages.value(QString::fromUtf8(ctx))
      .toObject()
      .value(QString::fromUtf8(src))
      .toString();
}
AppLanguage::AppLanguage() {
  const auto code = preferredLanguage();
  QLocale::setDefault(QLocale(code));
  if (qt.load(QLocale(code), "qtbase", "_",
              QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
    QCoreApplication::installTranslator(&qt);
  if (code == "pt_BR" && hero.load(":/i18n/hero_pt_BR.qm"))
    QCoreApplication::installTranslator(&hero);
  if (code != "es" && code != "pt_BR") {
    LanguagePack p;
    QString error;
    if (readPack(folder() + "/" + code + ".herolang", p, error)) {
      // Adapt branding in previously imported packages without modifying their files.
      for (auto ctx=p.messages.begin();ctx!=p.messages.end();++ctx) {
        QJsonObject updated;
        const auto strings=ctx.value().toObject();
        auto rename=[](QString s){return s.replace("Hero Music","Stellar Music").replace("HERO MUSIC","STELLAR MUSIC");};
        for(auto i=strings.begin();i!=strings.end();++i) updated.insert(rename(i.key()),rename(i.value().toString()));
        ctx.value()=updated;
      }
      package.messages = p.messages;
      QCoreApplication::installTranslator(&package);
    }
  }
}
