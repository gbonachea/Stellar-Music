# Validación de Stellar Music

**Entorno:** GNU/Linux x86-64 basado en Ubuntu 26.04, Qt 6.10.2, GStreamer 1.28.2, TagLib 2.2.1, compilación CMake Release. Se usó una salida virtual independiente de PipeWire con protocolo PulseAudio; los ensayos no requirieron escuchar los altavoces ni capturar un micrófono.

| Prueba ejecutada | Resultado |
|---|---|
| Compilar, instalar en un prefijo temporal y lanzar desde allí | Correcto. El binario instalado abrió un MP3 de 44,1 kHz, avanzó a 754 ms y cerró mediante MPRIS. |
| Arranque con Qt `xcb` y `wayland` | Ventana expuesta en X11/XWayland; proceso Wayland reproduciendo y controlado por D-Bus. Captura del frontal en `hero-classic-blue.png`. |
| Reproducir, pausar, reanudar, detener y buscar | La pausa mantuvo la posición; búsqueda a 5200 ms, reanudación y retorno a cero al detener. Un clic cerca del final de la barra hizo avanzar automáticamente a la segunda pista. |
| Anterior y siguiente | Los botones seleccionaron los títulos esperados de dos WAV etiquetados. |
| Metadatos | Título, artista y álbum recuperados de etiquetas; el visor indicó 00:08, 768 kbps, 48 kHz y MONO para el WAV de prueba. |
| PCM y ecualización | El buffer empleado por el visor contenía 20–22 ciclos ascendentes en 1024 muestras del tono de 1 kHz a 48 kHz. Al aplicar −12 dB a 1 kHz, el RMS pasó de aproximadamente 0,177 a 0,0445; el bypass recuperó el nivel plano. Se cambió entre FFT, osciloscopio y apagado. |
| Salida, volumen y silencio | En el monitor del dispositivo, con PCM de 16 bits: RMS 5656,49 → 2828,45 → 0 → 5656,49 al pasar por 100 %, 50 %, 0 % y 100 %. El botón de silencio también conservó el valor del deslizador. |
| Cola y listas | Reordenación, eliminación, recepción de eventos de arrastre de URL en el viewport, importación/exportación M3U y M3U8 con rutas relativas y conservación del orden. |
| Ventanas y biblioteca | Apertura/cierre de PL, EQ y ML desde sus botones; aplicación del preset Rock; indexación SQLite de dos archivos, filtro por título y adición desde la selección al reproductor. |
| Persistencia | Tras cerrar y crear otro reproductor con la misma configuración aislada se recuperaron el orden de la cola, repetición, aleatorio y tiempo restante. Se comprobó el volumen guardado. |
| Control del escritorio | Vía MPRIS: lectura de estado y posición, volumen, pausa, búsqueda, reproducción, parada y cierre. Los portales de escritorio del bus de pruebas emitieron avisos; no impidieron la reproducción. |
| Recursos y memoria | Se recompiló `hero-check` con AddressSanitizer (`-fsanitize=address -fno-omit-frame-pointer`) y se ejecutó con Qt offscreen. Terminó con cinco resultados satisfactorios y sin informes del sanitizador. El ensayo incluye destruir y recrear el reproductor después de usar audio, lista y biblioteca. |

**Medición acotada:** durante cinco segundos de reproducción de un WAV con el frontal en Wayland, el proceso consumió **2,4 % de un núcleo** y mostró **141 012 KiB de RSS** al final de la muestra. Se leyó `/proc/PID/stat` y `/proc/PID/status`; el RSS incluye bibliotecas compartidas. Es una observación puntual, no un límite ni una comparación controlada con el reproductor original.

## Repetir

Además de las dependencias de construcción del README, se necesitan Qt Test (incluido en `qt6-base-dev` de Debian/Ubuntu), `python3-dbus`, `dbus-daemon` y `pulseaudio-utils`. En una sesión gráfica con PulseAudio o PipeWire-Pulse:

```bash
./tests/run.sh
```

`tests/check.cpp` crea sus WAV etiquetados y sus preferencias en un directorio temporal. `tests/output.py` lanza el ejecutable de producción bajo otro perfil, mide el monitor con `parec` y libera la salida de prueba. La variable opcional `HERO_SCREENSHOT` en el test Qt guarda una captura PNG. Para ejecutar el test Qt sin escritorio, usar `QT_QPA_PLATFORM=offscreen`; esto no prueba la integración con un compositor.

## Alcance pendiente

- **Qt 5:** no hay un perfil de compilación mantenido. Se retiró el fallback de CMake de la base: ya usaba APIs exclusivas de Qt 6. Esta entrega requiere Qt 6.4 o posterior; la versión ejecutada fue 6.10.2.
- **Dependencias del escritorio:** no se verificaron visualmente las notificaciones de un gestor de bandeja real ni el inicio tras iniciar sesión en el sistema. Bajo Wayland la colocación absoluta de ventanas depende del compositor.
- **Cobertura:** se probaron WAV y MP3, no todos los códecs opcionales. No se midieron bibliotecas de cientos de miles de pistas ni sesiones de varios días. No se hizo una evaluación auditiva con altavoces físicos.
- **Opcionales no implementados:** acoplamiento de ventanas y lectura de skins propietarios de Winamp. La personalización soportada es el sistema QSS de Stellar Music.
