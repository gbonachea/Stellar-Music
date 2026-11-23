#include "configwindow.h"
#include "playback.h"
#include "settings.h"
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QPixmap>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTabWidget>
#include <QVBoxLayout>

ConfigWindow::ConfigWindow(QWidget *parent)
    : QDialog(parent), m_settings("Player", "AudioPlayer") {
  setWindowTitle(tr("Configuración"));

  QString iconPath = getIconPath();
  if (!iconPath.isEmpty())
    setWindowIcon(QIcon(iconPath));
  else
    setWindowIcon(QIcon::fromTheme("audio-x-generic"));

  setupUi();
  loadSavedGeometry();
  loadEqSettings();
  if (m_startupCheck->isChecked() && QFile::exists(
      QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart/heromusic.desktop"))
    saveSettings();
}

void ConfigWindow::setupUi() {
  auto *tabWidget = new QTabWidget(this);

  // ── General tab ──────────────────────────────────────────────────────────
  auto *generalTab = new QWidget();
  auto *generalLayout = new QVBoxLayout(generalTab);

  m_startupCheck = new QCheckBox(tr("Iniciar con el sistema"));
  m_minimizeCheck =
      new QCheckBox(tr("Minimizar a la bandeja del sistema al minimizar"));

  m_startupCheck->setChecked(m_settings.value("startup", false).toBool());
  m_minimizeCheck->setChecked(
      m_settings.value("minimize_to_tray", false).toBool());

  connect(m_startupCheck, &QCheckBox::toggled, this,
          &ConfigWindow::saveSettings);
  connect(m_minimizeCheck, &QCheckBox::toggled, this,
          &ConfigWindow::saveSettings);

  generalLayout->addWidget(m_startupCheck);
  generalLayout->addWidget(m_minimizeCheck);
  generalLayout->addStretch();

  // ── Equalizer tab ─────────────────────────────────────────────────────────
  auto *eqTab = new QWidget();
  auto *eqLayout = new QVBoxLayout(eqTab);

  auto *enabled = new QCheckBox(tr("Activar ecualizador"));
  enabled->setChecked(m_settings.value("eqEnabled", true).toBool());
  eqLayout->addWidget(enabled);
  connect(enabled, &QCheckBox::toggled, this, [this](bool on) {
    m_settings.setValue("eqEnabled", on);
    saveEqSettings();
  });
  QStringList frequencies{"Pre"};
  for (int hz : centers)
    frequencies << bandKey(hz);

  auto *sliderLayout = new QHBoxLayout();
  for (const QString &freq : frequencies) {
    auto *container = new QVBoxLayout();
    auto *slider = new QSlider(Qt::Vertical);
    auto *freqLabel = new QLabel(freq);
    auto *valueLabel = new QLabel("0 dB");

    slider->setRange(-12, 12);
    slider->setValue(0);
    slider->setTickPosition(QSlider::TicksBothSides);
    slider->setTickInterval(3);

    valueLabel->setAlignment(Qt::AlignCenter);
    freqLabel->setAlignment(Qt::AlignCenter);

    connect(slider, &QSlider::valueChanged, this, [valueLabel](int v) {
      valueLabel->setText(QString("%1 dB").arg(v));
    });
    connect(slider, &QSlider::valueChanged, this,
            &ConfigWindow::saveEqSettings);

    m_eqSliders[freq] = slider;

    container->addWidget(valueLabel);
    container->addWidget(slider, 1, Qt::AlignHCenter);
    container->addWidget(freqLabel);
    sliderLayout->addLayout(container);
  }

  // Preset buttons
  auto *presetLayout = new QHBoxLayout();
  const QStringList presets = {"Plano", "Rock", "Pop", "Jazz", "Clásica"};
  for (const QString &p : presets) {
    auto *btn = new QPushButton(p == "Clásica" ? tr("Clásica") : p);
    connect(btn, &QPushButton::clicked, this, [this, p] { applyPreset(p); });
    presetLayout->addWidget(btn);
  }

  eqLayout->addLayout(sliderLayout);
  eqLayout->addLayout(presetLayout);
  auto *custom = new QComboBox;
  custom->addItems(m_settings.value("presetNames").toStringList());
  auto *save = new QPushButton(tr("Guardar como…"));
  auto *read = new QPushButton(tr("Aplicar"));
  auto *customRow = new QHBoxLayout;
  customRow->addWidget(custom);
  customRow->addWidget(read);
  customRow->addWidget(save);
  eqLayout->addLayout(customRow);
  connect(save, &QPushButton::clicked, this, [this, custom] {
    auto name =
        QInputDialog::getText(this, tr("Preajuste"), tr("Nombre")).trimmed();
    if (name.isEmpty())
      return;
    auto presets = m_settings.value("customPresets").toMap();
    presets[name] = m_settings.value("equalizer");
    m_settings.setValue("customPresets", presets);
    m_settings.setValue("presetNames", presets.keys());
    custom->clear();
    custom->addItems(presets.keys());
    custom->setCurrentText(name);
  });
  connect(read, &QPushButton::clicked, this, [this, custom] {
    auto values = m_settings.value("customPresets")
                      .toMap()
                      .value(custom->currentText())
                      .toMap();
    for (auto it = values.begin(); it != values.end(); ++it)
      if (m_eqSliders.contains(it.key()))
        m_eqSliders[it.key()]->setValue(it.value().toInt());
  });

  // ── About tab ─────────────────────────────────────────────────────────────
  auto *aboutTab = new QWidget();
  auto *aboutLayout = new QVBoxLayout(aboutTab);

  QString iconPath = getIconPath();
  if (!iconPath.isEmpty()) {
    auto *iconLabel = new QLabel();
    QPixmap pix(iconPath);
    if (!pix.isNull()) {
      iconLabel->setPixmap(
          pix.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
      iconLabel->setAlignment(Qt::AlignCenter);
      aboutLayout->addWidget(iconLabel);
    }
  }

  auto *appName = new QLabel("Stellar Music");
  appName->setStyleSheet("font-size: 16px; font-weight: bold;");
  auto *version = new QLabel(tr("Versión 2.0"));
  auto *author = new QLabel(tr("Desarrollado por: B&R.Comp"));
  auto *description =
      new QLabel(tr("Reproductor de audio para GNU/Linux"));

  for (QLabel *lbl : {appName, version, author, description}) {
    lbl->setAlignment(Qt::AlignCenter);
    aboutLayout->addWidget(lbl);
  }
  aboutLayout->addStretch();

  // ── Assemble tabs ─────────────────────────────────────────────────────────
  tabWidget->addTab(generalTab, tr("General"));
  tabWidget->addTab(eqTab, tr("Ecualización"));
  tabWidget->addTab(aboutTab, tr("Acerca de"));

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->addWidget(tabWidget);
}

void ConfigWindow::loadSavedGeometry() {
  auto ws = loadWindowState("config", 150, 150, 439, 267);
  setGeometry(ws.x, ws.y, ws.width, ws.height);
  if (ws.state)
    setWindowState(Qt::WindowStates(ws.state));
}

void ConfigWindow::closeEvent(QCloseEvent *event) {
  saveWindowState("config", geometry(), windowState());
  event->accept();
}

void ConfigWindow::saveSettings() {
  m_settings.setValue("startup", m_startupCheck->isChecked());
  m_settings.setValue("minimize_to_tray", m_minimizeCheck->isChecked());

  // Linux autostart via .desktop file
  QString autostartDir =
      QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
      "/autostart/";
  QString desktopFile = autostartDir + "stellarmusic.desktop";
  QString legacyFile = autostartDir + "heromusic.desktop";

  if (m_startupCheck->isChecked()) {
    QDir().mkpath(autostartDir);
    QSaveFile f(desktopFile);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QTextStream out(&f);
      out << "[Desktop Entry]\n"
          << "Type=Application\n"
          << "Name=Stellar Music\n"
          << "Exec=\"" <<
#ifdef STELLAR_PORTABLE
          qEnvironmentVariable("STELLAR_PORTABLE_LAUNCHER")
#else
          QCoreApplication::applicationFilePath()
#endif
          << "\"\n"
          << "Hidden=false\n"
          << "NoDisplay=false\n"
          << "X-GNOME-Autostart-enabled=true\n";
      out.flush();
      if (out.status()==QTextStream::Ok && f.commit()) QFile::remove(legacyFile);
    }
  } else {
    QFile::remove(desktopFile);
    QFile::remove(legacyFile);
  }
}

void ConfigWindow::saveEqSettings() {
  QMap<QString, QVariant> eqValues;
  for (auto it = m_eqSliders.constBegin(); it != m_eqSliders.constEnd(); ++it)
    eqValues[it.key()] = it.value()->value();

  m_settings.setValue("equalizer", eqValues);

  // Notify parent AudioPlayer
  QMap<QString, int> intValues;
  for (auto it = m_eqSliders.constBegin(); it != m_eqSliders.constEnd(); ++it)
    intValues[it.key()] = it.value()->value();

  emit equalizerChanged(intValues);
}

void ConfigWindow::loadEqSettings() {
  QMap<QString, QVariant> eqValues = m_settings.value("equalizer").toMap();
  if (eqValues.contains("60Hz")) {
    m_settings.setValue("legacyEqualizer", eqValues);
    const QStringList old{"60Hz", "60Hz", "170Hz", "310Hz", "600Hz",
                          "1kHz", "3kHz", "3kHz",  "6kHz",  "16kHz"};
    QVariantMap migrated;
    for (int i = 0; i < 10; ++i)
      migrated[bandKey(centers[i])] = eqValues.value(old[i], 0);
    eqValues = migrated;
    m_settings.setValue("equalizer", eqValues);
  }
  for (auto it = eqValues.constBegin(); it != eqValues.constEnd(); ++it) {
    if (m_eqSliders.contains(it.key()))
      m_eqSliders[it.key()]->setValue(it.value().toInt());
  }
}

void ConfigWindow::applyPreset(const QString &presetName) {
  using Map = QMap<QString, int>;
  static const QMap<QString, Map> presets = {{"Plano",
                                              {{"31Hz", 0},
                                               {"62Hz", 0},
                                               {"125Hz", 0},
                                               {"250Hz", 0},
                                               {"500Hz", 0},
                                               {"1kHz", 0},
                                               {"2kHz", 0},
                                               {"4kHz", 0},
                                               {"8kHz", 0},
                                               {"16kHz", 0}}},
                                             {"Rock",
                                              {{"31Hz", 4},
                                               {"62Hz", 3},
                                               {"125Hz", -2},
                                               {"250Hz", -3},
                                               {"500Hz", 2},
                                               {"1kHz", 4},
                                               {"2kHz", 3},
                                               {"4kHz", 4},
                                               {"8kHz", 4},
                                               {"16kHz", 4}}},
                                             {"Pop",
                                              {{"31Hz", -1},
                                               {"62Hz", -1},
                                               {"125Hz", 0},
                                               {"250Hz", 2},
                                               {"500Hz", 3},
                                               {"1kHz", 2},
                                               {"2kHz", 1},
                                               {"4kHz", 1},
                                               {"8kHz", 2},
                                               {"16kHz", 2}}},
                                             {"Jazz",
                                              {{"31Hz", 2},
                                               {"62Hz", 1},
                                               {"125Hz", 1},
                                               {"250Hz", 2},
                                               {"500Hz", -1},
                                               {"1kHz", -1},
                                               {"2kHz", 0},
                                               {"4kHz", 1},
                                               {"8kHz", 2},
                                               {"16kHz", 3}}},
                                             {"Clásica",
                                              {{"31Hz", 3},
                                               {"62Hz", 2},
                                               {"125Hz", 1},
                                               {"250Hz", 0},
                                               {"500Hz", 0},
                                               {"1kHz", 0},
                                               {"2kHz", -1},
                                               {"4kHz", -1},
                                               {"8kHz", -2},
                                               {"16kHz", -2}}}};

  if (presets.contains(presetName)) {
    m_eqSliders["Pre"]->setValue(0);
    const Map &vals = presets[presetName];
    for (auto it = vals.constBegin(); it != vals.constEnd(); ++it) {
      if (m_eqSliders.contains(it.key()))
        m_eqSliders[it.key()]->setValue(it.value());
    }
    saveEqSettings();
  }
}
