#pragma once
#include <string>
#include <vector>

struct Cancion {
    std::string ruta;
    std::string titulo;
    std::string artista;
    std::string album;
    std::string genero;  // texto de la etiqueta; puede traer varios ("Rock; Pop")
    long long fecha = 0; // cuándo se agregó el archivo (segundos desde 1970)
};

// Recorre la carpeta (y subcarpetas) y lee las etiquetas de cada archivo de audio.
std::vector<Cancion> escanear_carpeta(const std::string& carpeta);

struct Caratula {
    std::vector<char> datos;  // bytes de la imagen (vacío si no hay)
    std::string ruta;         // ruta del archivo si vino de la carpeta; vacío si estaba incrustada
    std::string extension;    // ".jpg", ".png", ...
};

// Busca la carátula: primero incrustada en el archivo, luego cover.jpg/folder.jpg/... en la carpeta.
Caratula buscar_caratula(const std::string& ruta_cancion);
