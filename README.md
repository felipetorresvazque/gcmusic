# GC Music

GC Music es un reproductor de música desarrollado en C++ con GTK y GStreamer. Está pensado para gestionar una biblioteca musical local, reproducir pistas con una interfaz sencilla y ofrecer una experiencia similar a un reproductor moderno de escritorio.

## Descripción

Este proyecto permite:

- Reproducir archivos de audio desde la biblioteca local.
- Explorar música por artistas, álbumes, géneros y canciones.
- Crear y controlar una cola de reproducción.
- Buscar canciones rápidamente.
- Mostrar carátulas y metadatos de las pistas.
- Integrarse con MPRIS para el control del volumen y el estado de reproducción desde el sistema.

## Características principales

- Interfaz gráfica con GTK.
- Reproducción multimedia basada en GStreamer.
- Lectura de etiquetas y metadatos con TagLib.
- Biblioteca organizable por canciones, artistas, álbumes y géneros.
- Búsqueda de contenido musical.
- Soporte para integración con el sistema de escritorio mediante MPRIS.

## Requisitos

Para compilar este proyecto se requieren las siguientes dependencias:

- CMake 3.16 o superior
- Compilador de C++ con soporte de C++17
- GTKMM 4
- GStreamer 1.0
- TagLib
- Paquetes de plugins de audio de GStreamer para formatos como MP3 o M4A

## Compilación

Desde la raíz del proyecto, puedes ejecutar:

```bash
cmake -S . -B build
cmake --build build
```

Luego, para ejecutar la aplicación:

```bash
./build/gcmusic
```

## Instalación

El proyecto incluye soporte para instalación con CPack. También se puede instalar con CMake usando `--install`:

```bash
cmake --install build --prefix ~/.local
```

## Estructura del proyecto

```text
.
├── src/           # Código fuente principal de la aplicación
├── data/          # Archivos de datos y configuración de la aplicación
├── CMakeLists.txt # Configuración de compilación
├── LICENSE        # Licencia del proyecto
├── empaquetar.sh  # Script de empaquetado
└── README.md      # Documentación del proyecto
```

## Licencia

Este proyecto se distribuye bajo la licencia incluida en el archivo `LICENSE`.

## Nota

GC Music está orientado a funcionar como reproductor de música local y escritorio, con un enfoque práctico para la gestión de colecciones de música y el control de reproducción.
