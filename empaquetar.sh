#!/usr/bin/env bash
# Compila gcmusic y genera el paquete de ESTA distribución:
#   Fedora / RHEL / openSUSE            ->  gcmusic-<versión>.x86_64.rpm
#   Debian (13 o similar) / Ubuntu / Mint ->  gcmusic_<versión>_amd64.deb
#
# Uso:  ./empaquetar.sh            compila y empaqueta
#       ./empaquetar.sh --deps     primero instala las dependencias de compilación (pide contraseña)
set -euo pipefail
cd "$(dirname "$0")"

. /etc/os-release
case " ${ID:-} ${ID_LIKE:-} " in
    *" fedora "*|*" rhel "*|*" suse "*)
        GEN=RPM; NECESARIO="rpmbuild"
        PAQUETES="gcc-c++ cmake pkgconf-pkg-config gtkmm4.0-devel gstreamer1-devel taglib-devel rpm-build gstreamer1-plugins-base gstreamer1-plugins-good gstreamer1-plugin-mpg123"
        INSTALAR="dnf install -y" ;;
    *" debian "*|*" ubuntu "*)
        GEN=DEB; NECESARIO="dpkg-shlibdeps"
        PAQUETES="g++ cmake pkg-config dpkg-dev libgtkmm-4.0-dev libgstreamer1.0-dev libtag1-dev gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly"
        INSTALAR="apt-get install -y" ;;
    *) echo "Distribución no reconocida (${ID:-?}). Usa: cmake --install build --prefix ~/.local"; exit 1 ;;
esac

if [ "${1:-}" = "--deps" ]; then
    SUDO=""; [ "$(id -u)" -ne 0 ] && SUDO="sudo"
    echo "Instalando dependencias: $PAQUETES"
    # shellcheck disable=SC2086
    $SUDO $INSTALAR $PAQUETES
fi

if ! command -v "$NECESARIO" >/dev/null; then
    echo "Falta '$NECESARIO'. Instala las dependencias con:  ./empaquetar.sh --deps"
    exit 1
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
(cd build && cpack -G "$GEN")

echo
echo "Listo:"
ls -1 build/*.rpm build/*.deb 2>/dev/null || true
