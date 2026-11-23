#!/usr/bin/env bash
# Instala sólo las dependencias que faltan para ejecutar Stellar Music en Debian/Ubuntu.
# Uso: ./instalar-dependencias.sh [--dry-run] [--yes] [--codecs] [--build]
set -euo pipefail

dry_run=0
assumir_si=0
codecs=0
compilar=0

ayuda() {
  cat <<'EOF'
Uso: ./instalar-dependencias.sh [opciones]

Instala únicamente los paquetes que no están instalados. Las dependencias ya
presentes se omiten. Por defecto instala lo necesario para EJECUTAR ambos
ediciones (X11/XWayland y Wayland nativo) con el binario incluido.

Opciones:
  -n, --dry-run   Mostrar qué se instalaría sin modificar el sistema.
  -y, --yes       No pedir confirmación a apt-get.
      --codecs    Añadir plugins adicionales (FLAC/Opus, Libav).
      --build     Añadir compilador y cabeceras para compilar desde el código.
  -h, --help      Mostrar esta ayuda.

Ejemplos:
  ./instalar-dependencias.sh --dry-run
  ./instalar-dependencias.sh --yes
  sudo ./instalar-dependencias.sh --yes --codecs
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -n|--dry-run) dry_run=1 ;;
    -y|--yes) assumir_si=1 ;;
    --codecs) codecs=1 ;;
    --build) compilar=1 ;;
    -h|--help) ayuda; exit 0 ;;
    *) echo "Opción desconocida: $1" >&2; ayuda >&2; exit 2 ;;
  esac
  shift
done

if ! command -v apt-get >/dev/null 2>&1; then
  echo "Este script está preparado para Debian/Ubuntu (apt)." >&2
  echo "En otras distribuciones instala equivalentes de Qt 6, GStreamer y TagLib." >&2
  exit 1
fi

instalado() {
  local estado
  estado="$(dpkg-query -W -f='${Status}' "$1" 2>/dev/null || true)"
  [[ "$estado" == "install ok installed" ]] && return 0

  # En Ubuntu recientes el plugin pulsesink viene de gstreamer1.0-plugins-good,
  # aunque el metapaquete gstreamer1.0-pulseaudio no figure como instalado.
  if [[ "$1" == "gstreamer1.0-pulseaudio" ]] && \
     command -v gst-inspect-1.0 >/dev/null 2>&1; then
    gst-inspect-1.0 pulsesink >/dev/null 2>&1 && return 0
  fi
  return 1
}

disponible() {
  apt-cache show "$1" >/dev/null 2>&1
}

# Cada grupo admite alternatives separadas por '|'. Se instala la primera
# disponible; las ya instaladas nunca se vuelven a instalar.
grupos=(
  "libqt6core6t64|libqt6core6"
  "libqt6gui6"
  "libqt6widgets6"
  "libqt6sql6"
  "libqt6sql6-sqlite"
  "libqt6concurrent6"
  "libqt6dbus6"
  "qt6-wayland|libqt6waylandclient6"
  "libgstreamer1.0-0"
  "libgstreamer-plugins-base1.0-0"
  "gstreamer1.0-plugins-base"
  "gstreamer1.0-plugins-good"
  "gstreamer1.0-pulseaudio"
  "libtag2"
  "libxcb-cursor0"
  "libxkbcommon-x11-0"
  "dbus"
)

if (( compilar )); then
  grupos+=(
    "build-essential"
    "cmake"
    "pkg-config"
    "qt6-base-dev"
    "libgstreamer1.0-dev"
    "libgstreamer-plugins-base1.0-dev"
    "libtag1-dev"
  )
fi

faltan=()

revisar() {
  local grupo="$1"
  local opcional="${2:-0}"
  local candidato instalado_alguno=0 elegido=""
  IFS='|' read -r -a opciones <<< "$grupo"

  for candidato in "${opciones[@]}"; do
    if instalado "$candidato"; then
      printf '  [YA]      %s\n' "$candidato"
      instalado_alguno=1
      break
    fi
  done
  (( instalado_alguno )) && return 0

  for candidato in "${opciones[@]}"; do
    if disponible "$candidato"; then
      elegido="$candidato"
      break
    fi
  done
  if [[ -z "$elegido" ]]; then
    elegido="${opciones[0]}"
    if (( opcional )); then
      printf '  [AVISO]   %s no está disponible en los repositorios; se omite.\n' "$grupo"
      return 0
    fi
    printf '  [AVISO]   %s no aparece en apt-cache; se intentará de todos modos.\n' "$grupo" >&2
  fi

  printf '  [FALTA]   %s\n' "$elegido"
  faltan+=("$elegido")
}

printf 'Comprobando dependencias de Stellar Music...\n'
for grupo in "${grupos[@]}"; do
  revisar "$grupo"
done

# Los códecs extra son opcionales: si el paquete no existe, no bloquean.
if (( codecs )); then
  for grupo in "gstreamer1.0-plugins-bad" "gstreamer1.0-libav"; do
    revisar "$grupo" 1
  done
fi

if (( ${#faltan[@]} == 0 )); then
  printf '\nTodas las dependencias necesarias ya están instaladas. No se instaló nada.\n'
  exit 0
fi

printf '\nFaltan %d paquete(s): %s\n' "${#faltan[@]}" "${faltan[*]}"

if (( dry_run )); then
  printf 'Modo --dry-run: no se ha modificado el sistema.\n'
  exit 0
fi

if (( EUID == 0 )); then
  apt=(apt-get)
elif command -v sudo >/dev/null 2>&1; then
  apt=(sudo apt-get)
else
  echo "Se necesitan permisos de root o sudo para instalar paquetes." >&2
  exit 1
fi

printf 'Actualizando índices de paquetes...\n'
"${apt[@]}" update

printf 'Instalando únicamente los paquetes ausentes...\n'
args=()
(( assumir_si )) && args+=(-y)
"${apt[@]}" install --no-install-recommends "${args[@]}" "${faltan[@]}"

printf '\nInstalación terminada. Paquetes instalados: %s\n' "${faltan[*]}"
