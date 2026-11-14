cmake_minimum_required(VERSION 3.16)
project(gcmusic VERSION 1.1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Prefijo por defecto /usr (así los paquetes .deb/.rpm instalan en /usr y no en /usr/local).
# Para instalar solo en tu usuario:  cmake --install build --prefix ~/.local
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Prefijo de instalación" FORCE)
endif()
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0)
pkg_check_modules(GST   REQUIRED IMPORTED_TARGET gstreamer-1.0)
pkg_check_modules(TAG   REQUIRED IMPORTED_TARGET taglib)

add_executable(gcmusic
    src/main.cpp
    src/ventana.cpp
    src/motor.cpp
    src/biblioteca.cpp
    src/mpris.cpp)

target_compile_definitions(gcmusic PRIVATE GCMUSIC_VERSION="${PROJECT_VERSION}")
target_link_libraries(gcmusic PRIVATE PkgConfig::GTKMM PkgConfig::GST PkgConfig::TAG)

# --- Instalación: ejecutable, entrada del menú de aplicaciones e ícono
include(GNUInstallDirs)
install(TARGETS gcmusic RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
# El .desktop se genera al instalar, para que Exec apunte al prefijo realmente usado (--prefix)
install(CODE "
    set(CMAKE_INSTALL_FULL_BINDIR \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/data/cl.pipe.gcmusic.desktop.in
        \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/applications/cl.pipe.gcmusic.desktop\" @ONLY)
")
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/gcmusic)
install(FILES data/cl.pipe.gcmusic.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

# ----------------------------------------------------------------------------
# Paquetes instalables:  cpack -G DEB   (Debian/Ubuntu)   |   cpack -G RPM   (Fedora/openSUSE)
# Un paquete binario solo sirve en la distribución donde se compiló (las bibliotecas cambian
# de versión entre distribuciones), por eso se genera con ./empaquetar.sh en cada una.
# Las dependencias quedan declaradas: apt / dnf las instalan antes de instalar gcmusic.
# ----------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "gcmusic")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_CONTACT "Pipe <pipe@example.com>")   # <- cambia por tu nombre y correo
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Reproductor de música con GTK")
set(CPACK_PACKAGE_DESCRIPTION
    "GC Music (GTK Claude Music): reproductor de música en C++ con GTK4. Biblioteca por canciones, artistas, álbumes y géneros, cola de reproducción, búsqueda, carátulas y control desde el applet de volumen (MPRIS).")
set(CPACK_PACKAGE_HOMEPAGE_URL "")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_SET_DESTDIR ON)
set(CPACK_STRIP_FILES ON)

# .deb: las bibliotecas se detectan solas (dpkg-shlibdeps); aquí van los plugins de audio
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# mp3 y m4a se decodifican con plugins-ugly (mpg123) o con gstreamer1.0-libav: se exige uno de los dos
set(CPACK_DEBIAN_PACKAGE_DEPENDS "gstreamer1.0-plugins-base, gstreamer1.0-plugins-good, gstreamer1.0-plugins-ugly | gstreamer1.0-libav")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "gstreamer1.0-plugins-ugly, gstreamer1.0-libav")

# .rpm: las bibliotecas se detectan solas (rpmbuild); aquí van los plugins de audio
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE "MIT")                    # <- cambia si eliges otra licencia
set(CPACK_RPM_PACKAGE_REQUIRES "gstreamer1-plugins-base, gstreamer1-plugins-good, gstreamer1-plugin-mpg123")
set(CPACK_RPM_PACKAGE_SUGGESTS "gstreamer1-libav")
# No declarar como propias las carpetas del sistema (las posee el paquete 'filesystem')
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    "/usr/share/applications;/usr/share/icons;/usr/share/icons/hicolor;/usr/share/icons/hicolor/scalable;/usr/share/icons/hicolor/scalable/apps")
set(CPACK_RPM_SPEC_MORE_DEFINE "%define _build_id_links none")

include(CPack)
