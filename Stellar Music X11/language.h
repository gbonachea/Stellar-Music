#pragma once
#include <QJsonObject>
#include <QMap>
#include <QTranslator>

struct LanguagePack {
  QString code, name;
  QJsonObject messages;
};
QMap<QString, QString> availableLanguages();
QString preferredLanguage();
bool installLanguagePack(const QString &file, LanguagePack &pack,
                         QString &error);

class PackageTranslator final : public QTranslator {
public:
  QJsonObject messages;
  bool isEmpty() const override { return messages.isEmpty(); }
  QString translate(const char *context, const char *source, const char *,
                    int) const override;
};
class AppLanguage {
  QTranslator qt, hero;
  PackageTranslator package;

public:
  AppLanguage();
};
