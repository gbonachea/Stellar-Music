#include "audioplayer.h"
#include "configwindow.h"
#include "display.h"
#include "library.h"
#include "language.h"
#include "playlistwindow.h"
#include "track.h"
#include <QtTest>
#include <cmath>

class Checks : public QObject {
  Q_OBJECT
  QTemporaryDir dir;
  QString a, b;
  static double rms(Playback &p) {
    auto data = p.samples();
    double sum = 0;
    for (auto x : data)
      sum += x * x;
    return std::sqrt(sum / data.size());
  }
  QPushButton *button(QWidget &w, QString name) {
    for (auto b : w.findChildren<QPushButton *>())
      if (b->accessibleName() == name || b->text() == name)
        return b;
    return nullptr;
  }
private slots:
  void initTestCase() {
    QVERIFY(dir.isValid());
    qputenv("XDG_CONFIG_HOME", dir.filePath("config").toUtf8());
    qputenv("XDG_DATA_HOME", dir.filePath("data").toUtf8());
    qApp->setOrganizationName("BRComp");
    qApp->setApplicationName("HeroMusic");
    a = dir.filePath("01.wav");
    b = dir.filePath("02.wav");
    QFile wav(a);
    QVERIFY(wav.open(QIODevice::WriteOnly));
    QDataStream out(&wav);
    out.setByteOrder(QDataStream::LittleEndian);
    int rate = 48000, count = rate * 8;
    out.writeRawData("RIFF", 4);
    out << quint32(count * 2 + 36);
    out.writeRawData("WAVEfmt ", 8);
    out << quint32(16) << quint16(1) << quint16(1) << quint32(rate)
        << quint32(2 * rate) << quint16(2) << quint16(16);
    out.writeRawData("data", 4);
    out << quint32(count * 2);
    for (int i = 0; i < count; i++)
      out << qint16(8192 * std::sin(2 * 3.14159265358979 * i / 48));
    wav.close();
    {
      TagLib::FileRef f(QFile::encodeName(a).constData());
      QVERIFY(f.tag());
      f.tag()->setTitle("Signal One");
      f.tag()->setArtist("Hero Lab");
      f.tag()->setAlbum("Bench");
      QVERIFY(f.save());
    }
    QVERIFY(QFile::copy(a, b));
    {
      TagLib::FileRef f(QFile::encodeName(b).constData());
      f.tag()->setTitle("Signal Two");
      QVERIFY(f.save());
    }
  }
  void signalPath() {
    Playback p;
    QVERIFY2(p.failure.isEmpty(), qPrintable(p.failure));
    p.open(a);
    p.capture = true;
    p.volume(20, false);
    p.play();
    QTRY_VERIFY(rms(p) > 0.05);
    auto flat = rms(p);
    auto wave = p.samples();
    int crossings = 0;
    for (unsigned i = 1; i < wave.size(); i++)
      if (wave[i - 1] < 0 && wave[i] >= 0)
        ++crossings;
    QVERIFY(crossings >= 20 && crossings <= 22);
    p.equalize({{"1kHz", -12}}, true, 0);
    QTRY_VERIFY(rms(p) < flat * 0.32 && rms(p) > flat * .19);
    qInfo() << "RMS plano / banda 1 kHz a -12 dB:" << flat << rms(p);
    p.equalize({}, false, 0);
    QTRY_VERIFY(rms(p) > flat * .9);
    p.pause();
    QTest::qWait(100);
    auto paused = p.position();
    QTest::qWait(120);
    QVERIFY(std::abs(p.position() - paused) < 20);
    QVERIFY(p.seek(5200));
    QTRY_VERIFY(p.position() >= 5150);
    p.play();
    QTRY_VERIFY(p.position() > 5350);
    p.stop();
    QCOMPARE(p.position(), 0);
  }
  void interface() {
    AudioPlayer w;
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    w.openFiles({a, b});
    auto *d = w.findChild<ClassicDisplay *>();
    QVERIFY(d);
    auto *pl = w.findChild<PlaylistWindow *>();
    QCOMPARE(pl->paths(), QStringList({a, b}));
    QVERIFY(d->caption.contains("Hero Lab"));
    QVERIFY(d->caption.contains("Bench"));
    QVERIFY(d->details.contains("48 kHz"));
    QVERIFY(d->details.contains("MONO"));
    QVERIFY(d->details.contains("768 kbps"));
    QCOMPARE(d->total, QString("00:08"));
    auto click = [&](QString n) {
      auto *b = button(w, n);
      QVERIFY2(b, qPrintable(n));
      QTest::mouseClick(b, Qt::LeftButton);
    };
    QTest::qWait(400);
    if (qEnvironmentVariableIsSet("HERO_SCREENSHOT"))
      QVERIFY(w.grab().save(qEnvironmentVariable("HERO_SCREENSHOT")));
    click("Pausar");
    QVERIFY(button(w, "Reproducir")->isEnabled());
    click("Reproducir");
    QTest::mouseClick(d, Qt::LeftButton, {}, QPoint(d->width() - 20, 20));
    QCOMPARE(d->mode, 1);
    QTest::mouseClick(d, Qt::LeftButton, {}, QPoint(d->width() - 20, 20));
    QCOMPARE(d->mode, 2);
    QTest::mouseClick(d, Qt::LeftButton, {}, QPoint(d->width() - 20, 20));
    QCOMPARE(d->mode, 0);
    click("Siguiente");
    QVERIFY(d->caption.contains("Signal Two"));
    click("Anterior");
    QVERIFY(d->caption.contains("Signal One"));
    QSlider *seek = nullptr, *volume = nullptr;
    for (auto *s : w.findChildren<QSlider *>()) {
      if (s->accessibleName() == "Posición")
        seek = s;
      if (s->accessibleName() == "Volumen")
        volume = s;
    }
    QVERIFY(seek && volume);
    QTest::qWait(300);
    QTest::mouseClick(seek, Qt::LeftButton, {},
                      QPoint(seek->width() * 9 / 10, seek->height() / 2));
    QTRY_VERIFY_WITH_TIMEOUT(d->caption.contains("Signal Two"), 3500);
    volume->setValue(32);
    click("Silenciar");
    QVERIFY(button(w, "Silenciar")->isChecked());
    click("Silenciar");
    QCOMPARE(volume->value(), 32);
    click("Detener");
    QCOMPARE(seek->value(), 0);
    click("PL");
    QVERIFY(pl->isVisible());
    pl->playlist()->setCurrentRow(1);
    QVERIFY(QMetaObject::invokeMethod(pl, "moveUp"));
    QCOMPARE(pl->paths().first(), b);
    for (QString ext : {"m3u", "m3u8"}) {
      auto name = dir.filePath("lista." + ext);
      QVERIFY(pl->writeList(name));
      PlaylistWindow imported;
      connect(&imported, &PlaylistWindow::addFileSignal, &imported,
              &PlaylistWindow::append);
      QVERIFY(imported.readList(name));
      QCOMPARE(imported.paths(), pl->paths());
      imported.playlist()->setCurrentRow(0);
      QVERIFY(QMetaObject::invokeMethod(&imported, "removeAudio"));
      QCOMPARE(imported.paths(), QStringList{a});
      QMimeData mime;
      mime.setUrls({QUrl::fromLocalFile(b)});
      QDragEnterEvent enter(QPoint(3, 3), Qt::CopyAction, &mime, Qt::LeftButton,
                            Qt::NoModifier);
      QDropEvent drop(QPointF(3, 3), Qt::CopyAction, &mime, Qt::LeftButton,
                      Qt::NoModifier);
      QCoreApplication::sendEvent(imported.playlist()->viewport(), &enter);
      QCoreApplication::sendEvent(imported.playlist()->viewport(), &drop);
      QCOMPARE(imported.paths(), QStringList({a, b}));
    }
    click("PL");
    QVERIFY(!pl->isVisible());
    click("EQ");
    auto *cfg = w.findChild<ConfigWindow *>();
    QVERIFY(cfg->isVisible());
    click("Rock");
    QCOMPARE(QSettings("Player", "AudioPlayer")
                 .value("equalizer")
                 .toMap()
                 .value("31Hz")
                 .toInt(),
             4);
    click("EQ");
    QVERIFY(!cfg->isVisible());
    QSettings().setValue("roots", QStringList{dir.path()});
    click("ML");
    auto *lib = w.findChild<Library *>();
    QVERIFY(lib->isVisible());
    button(*lib, "Actualizar")->click();
    auto *view = lib->findChild<QTableView *>();
    QTRY_COMPARE(view->model()->rowCount(), 2);
    lib->findChild<QLineEdit *>()->setText("Two");
    QCOMPARE(view->model()->rowCount(), 1);
    pl->playlist()->setCurrentRow(0);
    QVERIFY(QMetaObject::invokeMethod(pl, "removeAudio"));
    view->selectRow(0);
    button(*lib, "+ Lista")->click();
    QCOMPARE(pl->paths(), QStringList({a, b}));
    pl->playlist()->setCurrentRow(1);
    QVERIFY(QMetaObject::invokeMethod(pl, "moveUp"));
    click("ML");
    click("SHF");
    click("SEQ");
    QTest::mouseClick(d, Qt::LeftButton, {}, QPoint(15, 15));
    QVERIFY(d->remaining);
    w.close();
    QCOMPARE(QSettings().value("volume").toInt(), 32);
    QCOMPARE(QSettings().value("playlist").toStringList(), QStringList({b, a}));
  }
  void languageChoice() {
    auto headings=[](AudioPlayer &w){ QStringList text; for(auto *a:w.findChild<QMenuBar*>()->actions())text<<a->text(); return text; };
    {
      AppLanguage language;
      AudioPlayer w;
      QCOMPARE(headings(w),QStringList({"Archivo","Reproducción","Opciones","Ver","Ayuda"}));
      for(auto *a:w.findChildren<QAction*>())if(a->text()=="Português (Brasil)")a->trigger();
      QCOMPARE(preferredLanguage(),QString("pt_BR"));
    }
    {
      AppLanguage language;
      AudioPlayer w;
      QCOMPARE(headings(w),QStringList({"Arquivo","Reprodução","Opções","Exibir","Ajuda"}));
      QCOMPARE(w.findChild<PlaylistWindow*>()->windowTitle(),QString("Lista de reprodução"));
      QCOMPARE(w.findChild<ConfigWindow*>()->windowTitle(),QString("Configurações"));
      for(auto *a:w.findChildren<QAction*>())if(a->text()=="Español")a->trigger();
      QCOMPARE(preferredLanguage(),QString("es"));
    }
  }
  void importLanguage() {
    const auto source=QFINDTESTDATA("../i18n/English.herolang");
    QVERIFY(!source.isEmpty());
    LanguagePack pack;QString error;
    QVERIFY2(installLanguagePack(source,pack,error),qPrintable(error));
    QCOMPARE(availableLanguages().value("en"),QString("English"));
    {
      AudioPlayer w;
      for(auto *a:w.findChildren<QAction*>())if(a->text()=="English")a->trigger();
      QCOMPARE(preferredLanguage(),QString("en"));
    }
    {
      AppLanguage lang;
      AudioPlayer w;
      QCOMPARE(w.findChild<QMenuBar*>()->actions().first()->text(),QString("File"));
      QCOMPARE(w.findChild<ConfigWindow*>()->windowTitle(),QString("Settings"));
    }
    QSettings().setValue("language","es");
    QVERIFY(!installLanguagePack(a,pack,error)); // a WAV is not a language pack
    QVERIFY(availableLanguages().contains("en"));
  }
  void closeExitsEvenWithTrayPreference() {
    QSettings("Player","AudioPlayer").setValue("minimize_to_tray",true);
    AudioPlayer w;
    w.show();
    auto *closeButton=button(w,"Cerrar");
    QVERIFY(closeButton);
    QCOMPARE(w.windowTitle(),QString("Stellar Music"));
    QVERIFY(!w.windowIcon().pixmap(32).isNull());
    // Even a desktop integration that holds the application open must not
    // keep the player running once Close is requested.
    const bool previous=qApp->quitOnLastWindowClosed();
    qApp->setQuitOnLastWindowClosed(false);
    QTimer watchdog;
    connect(&watchdog,&QTimer::timeout,[]{QCoreApplication::exit(9);});
    watchdog.start(2000);
    QTimer::singleShot(0,closeButton,&QPushButton::click);
    const int code=qApp->exec();
    qApp->setQuitOnLastWindowClosed(previous);
    QCOMPARE(code,0);
    if(auto *tray=w.findChild<QSystemTrayIcon*>())QVERIFY(!tray->isVisible());
  }
  void restore() {
    AudioPlayer w;
    w.show();
    QCOMPARE(w.findChild<PlaylistWindow *>()->paths(), QStringList({b, a}));
    QVERIFY(button(w, "SHF")->isChecked());
    QVERIFY(button(w, "R·1")->isChecked());
    QVERIFY(w.findChild<ClassicDisplay *>()->remaining);
    w.close();
  }
  void resizePlayer() {
    AudioPlayer w;
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    QCOMPARE(w.size(), QSize(620,285));
    auto *toggle = button(w,"Alternar tamaño");
    QVERIFY(toggle);
    toggle->click();
    QCOMPARE(w.size(), QSize(785,350));
    toggle->click();
    QVERIFY(w.width()<620 && w.height()<285);
    toggle->click();
    QCOMPARE(w.size(), QSize(620,285));
    auto moveTo = [&](QWidget *widget) {
      QTest::mouseMove(widget, widget->rect().center());
      return widget->cursor().shape();
    };
    auto *play = w.findChild<QPushButton*>("play");
    auto *config = button(w, "Configuración");
    auto *side = w.childAt(2, w.height()/2);
    auto *corner = w.childAt(w.width()-3,w.height()-3);
    QVERIFY(side && corner && play && config);
    QCOMPARE(moveTo(side), Qt::SizeHorCursor);
    QCOMPARE(moveTo(play), Qt::ArrowCursor);
    QCOMPARE(moveTo(corner), Qt::SizeFDiagCursor);
    QCOMPARE(moveTo(config), Qt::ArrowCursor);
    QCOMPARE(moveTo(w.findChild<ClassicDisplay*>()), Qt::ArrowCursor);
    if (QGuiApplication::platformName()=="xcb") {
      const QPoint local = corner->rect().center();
      QTest::mousePress(corner, Qt::LeftButton, {}, local);
      QTest::mouseMove(corner, local-QPoint(44,20));
      QTest::mouseRelease(corner,Qt::LeftButton,{},local);
      QCOMPARE(w.size(),QSize(576,265));
      QCOMPARE(moveTo(play), Qt::ArrowCursor);
    }
    const QSize chosen=w.size();
    w.close();
    AudioPlayer reopened;
    reopened.show();
    QCOMPARE(reopened.size(),chosen);
  }
};
QTEST_MAIN(Checks)
#include "check.moc"
