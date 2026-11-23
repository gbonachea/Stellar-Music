# Stellar Music · Stellar Classic Blue

Revisión de **Hero Music Base** (C++17, Qt Widgets) para GNU/Linux. Conserva la lista independiente, el diálogo de configuración, los presets, el inicio automático y la bandeja. El nuevo frontal tiene controles vectoriales interactivos, no una captura del reproductor de referencia.

<img width="620" height="285" alt="hero-classic-blue" src="https://github.com/user-attachments/assets/39281316-0360-45b6-8bc7-3d908a63a6eb" />


## Compilar y ejecutar

En Debian/Ubuntu con Qt ≥ 6.4:

```bash
# Para ejecutar el binario incluido: instala únicamente lo que falta.
./instalar-dependencias.sh --yes

# Para compilar desde el código, añade también las cabeceras:
./instalar-dependencias.sh --yes --build
./build.sh
./out/StellarMusic                    # también acepta una o varias rutas de audio
```

`./instalar-dependencias.sh --dry-run` muestra qué se instalaría sin tocar el sistema. `build.sh` y `run.sh` no instalan paquetes. Instalación opcional: `cmake --install out --prefix "$HOME/.local"`; el lanzador de escritorio necesita `~/.local/bin` en `PATH`. Para probar otro protocolo de ventanas: `QT_QPA_PLATFORM=xcb` o `QT_QPA_PLATFORM=wayland` en una sesión que lo ofrezca.

GStreamer usa los decodificadores y la salida instalados. MP3, WAV, FLAC, Ogg, Opus y otros dependen de esos complementos; para ampliar la cobertura se puede instalar `gstreamer1.0-libav`. Si un archivo no se puede reproducir, el mensaje del backend aparece en el visor. TagLib obtiene las etiquetas y propiedades del archivo; los datos desconocidos se omiten. El bitrate puede ser el promedio del archivo.

## Manejo

**Opciones → Idioma (al volver a abrir)** permite elegir **Español** (predeterminado) o **Português (Brasil)**. La selección se guarda y se aplica al cerrar y abrir Stellar Music de nuevo.

**Agregar idioma…** importa un archivo `.herolang`, lo selecciona y guarda una copia en `AppDataLocation/languages`. Para probarlo, importa **`i18n/English.herolang`**. Para crear otro idioma, duplica ese ejemplo (JSON UTF-8), cambia `locale` (p. ej. `fr` o `fr_CA`), `name` (p. ej. `Français`) y traduce los **valores** de `messages`. Conserva los nombres de contexto, las claves en español y los marcadores como `%1`. Puedes omitir mensajes: se mostrarán en español. Importar de nuevo el mismo código actualiza ese paquete; `es` y `pt_BR` están reservados. Los controles estándar de Qt usan los catálogos de Qt disponibles en el sistema.

- **Archivo / Ctrl+O** abre archivos y reproduce el primero. Arrastrar sobre el frontal hace lo mismo. La bandeja **PL** añade sin interrumpir; el doble clic reproduce. Las listas M3U/M3U8 se añaden a la cola, se guardan en UTF-8 con rutas relativas y no duplican archivos ya presentes.
- **Espacio** alterna reproducción/pausa, **S** detiene, **Z/B** retroceden/avanzan. La barra de posición admite clic o arrastre. **VOL** silencia sin mover el deslizador.
- **SEQ → R·1 → R·ALL** cambia el comportamiento al final de la pista. **SHF** elige pistas sin repetir dentro de un recorrido. En secuencial se detiene al final de la cola; «Siguiente» manual permite volver al principio.
- **T** o clic en el reloj: tiempo restante/transcurrido. **V** o clic en el analizador: espectro, osciloscopio y apagado. Una banda estrecha al reproducir un tono puro es el resultado esperado.
- **PL / Alt+P**, **ML / Alt+L** y **EQ / Alt+E** muestran u ocultan sus ventanas. **CONFIG** abre «General» en el diálogo original. Arrastra el título para mover el frontal; arrastra un borde o una esquina para reducirlo o ampliarlo. **▭** alterna entre el tamaño predeterminado (620 × 285), el ampliado y el compacto. **Ctrl+0** recupera el tamaño predeterminado sin mover la ventana. Se recuerda el tamaño elegido al cerrar.
- En **ML**, añade carpetas, usa **Actualizar** tras cambiar la colección, filtra por texto y pulsa una cabecera para ordenar. Selecciona filas y usa **+ Lista** o doble clic. El índice SQLite se reconstruye en un hilo de trabajo, en una transacción; la vista consulta las filas por lotes.
- En **EQ** están la activación, el preamplificador, las diez bandas, los cinco presets y el guardado por nombre. **Plano** restaura todas las ganancias a 0 dB. En **Opciones** se activan las notificaciones. La bandeja sólo se crea cuando Qt detecta soporte. MPRIS usa `org.mpris.MediaPlayer2.StellarMusic` en el bus de sesión.

## Audio y visualización

La base enlazaba `QMediaPlayer` con `QAudioOutput`, sin insertar los filtros del diálogo de EQ. Para que el visor y la ecualización trabajen **sobre el mismo audio que se escucha**, esta versión usa GStreamer directamente. No hay un segundo decodificador ni captura del micrófono. Qt sigue gestionando ventanas, eventos, ajustes y escritorio.

`playbin → conversión estéreo F32 → preamplificador con reserva de ganancia → equalizer-nbands → compresor de picos → toma PCM → salida`. Las bandas centrales son 31, 62, 125, 250, 500, 1000, 2000, 4000, 8000 y 16000 Hz. La ganancia previa se compensa con el mayor realce seleccionado; el control de picos tras el filtro limita las sobrecargas. La etiqueta MONO/STEREO describe **el archivo**, no la conversión interna.

El hilo de audio conserva hasta 1024 muestras mono para la pantalla. Qt pinta como máximo a 25 fps, con ventana Hann y FFT de GStreamer, o dibuja esa misma señal en el osciloscopio. La captura se desactiva al ocultar el visor, al parar o al seleccionar «apagado». La pausa conserva el último cuadro. No se usan datos aleatorios para animar la pantalla.

## Temas y rutas de usuario

**Stellar Classic Blue** se incrusta con el icono. **Opciones → Cargar tema QSS…** superpone una hoja de estilo al tema base. `skins/Graphite.qss` es un ejemplo editable: `#chassis`, `#alloy`, `TransportButton` y `#glass` controlan carcasa, superficie, metal y pantalla. Para esta última, `qproperty-glass` y `qproperty-ink` determinan los colores de la señal. La selección persiste por ruta; si falta el archivo se recupera Classic Blue.

- `QSettings("BRComp","HeroMusic")` (identificador conservado por compatibilidad): cola, pista, volumen, modos, skin, visor y carpetas.
- `QSettings("Player","AudioPlayer")`: preferencias heredadas de bandeja, inicio automático y EQ. Se conserva una copia del mapa antiguo al adaptar las frecuencias.
- `QStandardPaths::AppConfigLocation/setting.json`: geometrías. Si existe un `setting.json` legado junto al ejecutable se importa la primera vez; también puedes copiar el antiguo a esta ruta. En una sesión Wayland con XWayland disponible, el inicio automático del backend prefiere XWayland para poder recuperar la posición al abrir. Si se fuerza Wayland nativo o no hay XWayland, el compositor decide la posición; se conserva la última ubicación X11 para el próximo inicio compatible.
- `QStandardPaths::AppDataLocation/library.sqlite`: índice de biblioteca, creado al abrir ML. No se copian los archivos de música.

## Mapa del código

| Archivos | Responsabilidad |
|---|---|
| `main.cpp`, `audioplayer.*` | Entrada, frontal, transporte, navegación, cola y bandeja |
| `playlistwindow.*`, `configwindow.*`, `settings.*` | Ventanas y persistencia adaptadas de Stellar Music Base |
| `playback.*` | GStreamer, estados, posición, DSP y ventana de muestras |
| `display.*`, `transportbutton.h` | Cristal digital, FFT, marquesina y controles vectoriales |
| `track.h`, `library.*` | Propiedades de los archivos e índice SQLite |
| `mpris.cpp` | Adaptadores D-Bus de control y metadatos |
| `assets.qrc`, `classic.qss`, `skins/`, `icons/` | Recursos e identidad visual |
| `CMakeLists.txt`, `build.sh`, `run.sh`, `instalar-dependencias.sh`, `stellarmusic.desktop` | Construcción e instalación |
| `tests/`, `docs/` | Validación, mediciones y captura de la aplicación |

Pruebas ejecutadas, instrucciones para repetirlas y límites de la entrega: **[docs/VALIDACION.md](docs/VALIDACION.md)**.
