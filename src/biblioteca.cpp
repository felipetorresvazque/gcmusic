#include "biblioteca.h"

#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/mp4coverart.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mpegfile.h>

#include <fcntl.h>
#include <sys/stat.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <tuple>

namespace fs = std::filesystem;

static std::string minusculas(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

static bool es_audio(const fs::path& p) {
    static const char* validas[] = {".mp3", ".flac", ".ogg", ".opus", ".wav", ".m4a", ".aac"};
    const std::string ext = minusculas(p.extension().string());
    return std::any_of(std::begin(validas), std::end(validas),
                       [&](const char* v) { return ext == v; });
}

// Fecha en que el archivo llegó a este equipo: la de creación si el sistema de archivos la
// informa; si no, la de modificación.
static long long fecha_agregado(const std::string& ruta) {
    struct statx sx;
    if (statx(AT_FDCWD, ruta.c_str(), 0, STATX_BTIME | STATX_MTIME, &sx) != 0) return 0;
    if ((sx.stx_mask & STATX_BTIME) && sx.stx_btime.tv_sec > 0) return sx.stx_btime.tv_sec;
    if (sx.stx_mask & STATX_MTIME) return sx.stx_mtime.tv_sec;
    return 0;
}

std::vector<Cancion> escanear_carpeta(const std::string& carpeta) {
    std::vector<Cancion> resultado;
    std::error_code ec;

    fs::recursive_directory_iterator it(carpeta, fs::directory_options::skip_permission_denied, ec);
    const fs::recursive_directory_iterator fin;

    for (; !ec && it != fin; it.increment(ec)) {
        if (!it->is_regular_file(ec) || !es_audio(it->path())) continue;

        Cancion c;
        c.ruta = it->path().string();
        c.fecha = fecha_agregado(c.ruta);

        TagLib::FileRef ref(c.ruta.c_str());
        if (!ref.isNull() && ref.tag()) {
            c.titulo  = ref.tag()->title().to8Bit(true);
            c.artista = ref.tag()->artist().to8Bit(true);
            c.album   = ref.tag()->album().to8Bit(true);
            c.genero  = ref.tag()->genre().to8Bit(true);

            // Con varios géneros, tag()->genre() los pega con un espacio; las propiedades del
            // archivo los entregan por separado (ID3, Vorbis, MP4...).
            TagLib::PropertyMap props = ref.file()->properties();
            const auto g = props.find("GENRE");
            if (g != props.end() && !g->second.isEmpty()) {
                std::string unidos;
                for (const auto& valor : g->second) {
                    if (!unidos.empty()) unidos += "; ";
                    unidos += valor.to8Bit(true);
                }
                c.genero = unidos;
            }
        }
        if (c.titulo.empty())  c.titulo  = it->path().stem().string();
        if (c.artista.empty()) c.artista = "Desconocido";
        if (c.album.empty())   c.album   = "Desconocido";

        resultado.push_back(std::move(c));
    }

    std::sort(resultado.begin(), resultado.end(), [](const Cancion& a, const Cancion& b) {
        return std::make_tuple(minusculas(a.artista), minusculas(a.album), minusculas(a.titulo)) <
               std::make_tuple(minusculas(b.artista), minusculas(b.album), minusculas(b.titulo));
    });
    return resultado;
}

// ---------------------------------------------------------------- Carátulas

static std::string detectar_extension(const std::vector<char>& d) {
    auto u = [&](std::size_t i) { return i < d.size() ? static_cast<unsigned char>(d[i]) : 0; };
    if (u(0) == 0x89 && u(1) == 'P' && u(2) == 'N' && u(3) == 'G') return ".png";
    if (u(0) == 0xFF && u(1) == 0xD8) return ".jpg";
    if (u(0) == 'G' && u(1) == 'I' && u(2) == 'F') return ".gif";
    if (u(0) == 'R' && u(1) == 'I' && u(2) == 'F' && u(3) == 'F') return ".webp";
    return ".img";
}

static bool leer_archivo(const fs::path& p, std::vector<char>& salida) {
    std::error_code ec;
    const auto tam = fs::file_size(p, ec);
    if (ec || tam == 0 || tam > 15u * 1024 * 1024) return false;
    std::ifstream f(p, std::ios::binary);
    salida.resize(static_cast<std::size_t>(tam));
    return static_cast<bool>(f.read(salida.data(), static_cast<std::streamsize>(tam)));
}

Caratula buscar_caratula(const std::string& ruta_cancion) {
    Caratula car;

    // 1) Imagen incrustada en las etiquetas (mp3, flac, m4a)
    TagLib::ByteVector incrustada;
    const std::string ext_cancion = minusculas(fs::path(ruta_cancion).extension().string());

    if (ext_cancion == ".mp3") {
        TagLib::MPEG::File f(ruta_cancion.c_str());
        if (f.isValid() && f.ID3v2Tag()) {
            for (auto* frame : f.ID3v2Tag()->frameList("APIC")) {
                auto* apic = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(frame);
                if (!apic || apic->picture().isEmpty()) continue;
                if (incrustada.isEmpty()) incrustada = apic->picture();
                if (apic->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover) {
                    incrustada = apic->picture();
                    break;
                }
            }
        }
    } else if (ext_cancion == ".flac") {
        TagLib::FLAC::File f(ruta_cancion.c_str());
        if (f.isValid()) {
            for (auto* pic : f.pictureList()) {
                if (pic->data().isEmpty()) continue;
                if (incrustada.isEmpty()) incrustada = pic->data();
                if (pic->type() == TagLib::FLAC::Picture::FrontCover) {
                    incrustada = pic->data();
                    break;
                }
            }
        }
    } else if (ext_cancion == ".m4a" || ext_cancion == ".aac") {
        TagLib::MP4::File f(ruta_cancion.c_str());
        if (f.isValid() && f.tag() && f.tag()->contains("covr")) {
            const auto lista = f.tag()->item("covr").toCoverArtList();
            if (!lista.isEmpty()) incrustada = lista.front().data();
        }
    }

    if (!incrustada.isEmpty()) {
        car.datos.assign(incrustada.data(), incrustada.data() + incrustada.size());
        car.extension = detectar_extension(car.datos);
        return car;
    }

    // 2) Archivo de imagen en la misma carpeta
    static const char* nombres[] = {"cover", "folder", "front", "album", "albumart", "albumartsmall"};
    static const char* extensiones[] = {".jpg", ".jpeg", ".png", ".webp", ".gif"};
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(fs::path(ruta_cancion).parent_path(), ec)) {
        if (!e.is_regular_file(ec)) continue;
        const std::string nombre = minusculas(e.path().stem().string());
        const std::string ext = minusculas(e.path().extension().string());
        const bool nombre_ok = std::any_of(std::begin(nombres), std::end(nombres),
                                           [&](const char* n) { return nombre == n; });
        const bool ext_ok = std::any_of(std::begin(extensiones), std::end(extensiones),
                                        [&](const char* x) { return ext == x; });
        if (nombre_ok && ext_ok && leer_archivo(e.path(), car.datos)) {
            car.ruta = e.path().string();
            car.extension = ext;
            return car;
        }
    }
    return car;
}
