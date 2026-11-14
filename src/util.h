#pragma once
#include <glibmm.h>

#include <string>

// Convierte a UTF-8 válido (reemplaza bytes inválidos) para que GTK/D-Bus no se quejen.
inline Glib::ustring utf8_seguro(const std::string& s) {
    gchar* valido = g_utf8_make_valid(s.c_str(), -1);
    Glib::ustring resultado(valido);
    g_free(valido);
    return resultado;
}

// Minúsculas y sin tildes ("Canción" -> "cancion"), para comparar textos al buscar.
inline std::string normalizar(const std::string& texto) {
    gchar* valido = g_utf8_make_valid(texto.c_str(), -1);
    gchar* descompuesto = g_utf8_normalize(valido, -1, G_NORMALIZE_NFD);
    gchar* minusculas = g_utf8_casefold(descompuesto, -1);
    std::string salida;
    for (const gchar* p = minusculas; *p; p = g_utf8_next_char(p)) {
        const gunichar c = g_utf8_get_char(p);
        if (g_unichar_type(c) == G_UNICODE_NON_SPACING_MARK) continue;  // quita los acentos
        char buf[8];
        salida.append(buf, static_cast<std::size_t>(g_unichar_to_utf8(c, buf)));
    }
    g_free(valido);
    g_free(descompuesto);
    g_free(minusculas);
    return salida;
}
