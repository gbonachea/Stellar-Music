#include "audioplayer.h"
#include "settings.h"
#include "language.h"
#include <QApplication>

int main(int argc, char *argv[]) {
#ifdef STELLAR_X11
  qputenv("QT_QPA_PLATFORM", "xcb");
#else
  // Native Wayland does not expose a toplevel's global position or allow
  // moving it to saved coordinates. Use XWayland when the session offers it.
  // An explicit QT_QPA_PLATFORM or Qt -platform argument still takes priority.
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") &&
      !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") &&
      !qEnvironmentVariableIsEmpty("DISPLAY"))
    qputenv("QT_QPA_PLATFORM", "xcb;wayland");
#endif
  QApplication app(argc, argv);
  // Stable storage ID: keep existing playlists, languages, EQ and library.
  app.setApplicationName("HeroMusic");
  app.setApplicationDisplayName("Stellar Music");
  app.setDesktopFileName("stellarmusic");
  app.setWindowIcon(QIcon(getIconPath()));
  app.setOrganizationName("BRComp");
  AppLanguage language;

  // Apply dark theme from CSS file
  QString style = loadStylesheet();
  if (!style.isEmpty())
    app.setStyleSheet(style);

  AudioPlayer player;
  player.show();
  if (argc > 1)
    player.openFiles(app.arguments().mid(1));

  return app.exec();
}
