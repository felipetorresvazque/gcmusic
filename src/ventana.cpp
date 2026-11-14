#include "ventana.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>

#include <gst/gst.h>
#include <taglib/taglib.h>

#include "util.h"

namespace fs = std::filesystem;

namespace {

// --- Datos del "Acerca de" (edítalos aquí) ---
constexpr const char* MODELOS_IA = "Claude Sonnet 5.5 (Anthropic)";
constexpr const char* TEXTO_LICENCIA = R"(MIT License

Copyright (c) 2026 felipesonic

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.)";

constexpr int TAM_CARATULA = 80;  // lado en píxeles de la carátula en la barra inferior

std::string formatear_tiempo(double s) {
    if (s < 0) s = 0;
    const int total = static_cast<int>(s);
    char buf[16];
    std::snprintf(buf, sizeof buf, "%d:%02d", total / 60, total % 60);
    return buf;
}

void vaciar(Gtk::ListBox& lista) {
    while (auto* fila = lista.get_row_at_index(0)) lista.remove(*fila);
}

bool con_mayuscula(const std::string& t) {
    return std::any_of(t.begin(), t.end(), [](unsigned char c) { return std::isupper(c); });
}

Glib::ustring escapar(const std::string& s) {
    return Glib::Markup::escape_text(utf8_seguro(s));
}

}  // namespace

Ventana::Ventana() {
    set_title("GC Music");
    seguir_tema_del_sistema();
    set_icon_name("cl.pipe.gcmusic");
    set_default_size(1000, 640);
    cargar_config();
    cargar_sesion();

    // --- Cabecera ---
    b_carpeta.set_icon_name("folder-open-symbolic");
    b_carpeta.set_label("Carpeta…");
    b_carpeta.set_tooltip_text("Elegir la carpeta de música");
    b_aleatorio.set_icon_name("media-playlist-shuffle-symbolic");
    b_aleatorio.set_label("Aleatorio");
    b_aleatorio.set_tooltip_text("Reproducir en orden aleatorio lo que se está viendo");
    b_cola.set_tooltip_text("Mostrar u ocultar la cola de reproducción");
    b_buscar.set_icon_name("system-search-symbolic");
    b_buscar.set_tooltip_text("Buscar canciones (Ctrl+F)");

    entrada_busqueda.set_placeholder_text("Buscar por canción, artista o álbum…");
    entrada_busqueda.set_hexpand(true);
    entrada_busqueda.set_size_request(420, -1);
    barra_busqueda.set_child(entrada_busqueda);
    barra_busqueda.connect_entry(entrada_busqueda);

    t_artistas.set_group(t_canciones);
    t_albumes.set_group(t_canciones);
    t_generos.set_group(t_canciones);
    t_parati.set_group(t_canciones);
    t_canciones.set_active(true);
    caja_modos.add_css_class("linked");
    caja_modos.append(t_parati);
    caja_modos.append(t_canciones);
    caja_modos.append(t_artistas);
    caja_modos.append(t_albumes);
    caja_modos.append(t_generos);

    cabecera.pack_start(b_carpeta);
    cabecera.pack_start(b_buscar);
    auto menu = Gio::Menu::create();
    menu->append("Acerca de GC Music", "win.acerca");
    b_menu.set_icon_name("open-menu-symbolic");
    b_menu.set_menu_model(menu);
    b_menu.set_tooltip_text("Menú");
    add_action("acerca", sigc::mem_fun(*this, &Ventana::mostrar_acerca));

    cabecera.pack_end(b_menu);
    cabecera.pack_end(b_aleatorio);
    cabecera.pack_end(b_cola);
    cabecera.set_title_widget(caja_modos);
    set_titlebar(cabecera);

    // --- Listas ---
    sc_grupos.set_child(lista_grupos);
    sc_grupos.set_min_content_width(240);
    sc_canciones.set_child(lista_canciones);
    lista_grupos.set_selection_mode(Gtk::SelectionMode::SINGLE);
    lista_canciones.set_selection_mode(Gtk::SelectionMode::SINGLE);
    lista_canciones.set_activate_on_single_click(false);  // doble clic / Enter para reproducir

    sc_grupos.set_vexpand(true);
    b_renovar.set_icon_name("view-refresh-symbolic");
    b_renovar.set_label("Renovar aleatorias");
    b_renovar.set_tooltip_text("Elegir otra selección aleatoria");
    b_renovar.set_margin(8);
    b_renovar.set_visible(false);
    caja_grupos.append(sc_grupos);
    caja_grupos.append(b_renovar);
    panel.set_start_child(caja_grupos);
    panel.set_end_child(sc_canciones);
    panel.set_resize_start_child(false);
    panel.set_shrink_start_child(false);
    panel.set_hexpand(true);
    panel.set_vexpand(true);
    caja_grupos.set_visible(false);

    // --- Panel de cola (se desliza desde la derecha) ---
    l_cola_titulo.set_markup("<b>A continuación</b>");
    l_cola_titulo.set_xalign(0);
    l_cola_titulo.set_hexpand(true);
    b_vaciar_cola.set_label("Vaciar");
    b_vaciar_cola.add_css_class("flat");
    cab_cola.set_margin(8);
    cab_cola.append(l_cola_titulo);
    cab_cola.append(b_vaciar_cola);

    l_cola_vacia.set_text("La cola está vacía.\nUsa «+» en una canción para añadirla.");
    l_cola_vacia.set_justify(Gtk::Justification::CENTER);
    l_cola_vacia.set_margin(16);
    l_cola_vacia.add_css_class("dim-label");
    lista_cola.set_placeholder(l_cola_vacia);
    lista_cola.set_selection_mode(Gtk::SelectionMode::SINGLE);
    lista_cola.set_activate_on_single_click(false);
    sc_cola.set_child(lista_cola);
    sc_cola.set_vexpand(true);

    caja_cola.set_size_request(300, -1);
    caja_cola.append(cab_cola);
    caja_cola.append(sc_cola);
    cola_externa.append(sep_cola);
    cola_externa.append(caja_cola);
    rev_cola.set_transition_type(Gtk::RevealerTransitionType::SLIDE_LEFT);
    rev_cola.set_child(cola_externa);
    rev_cola.set_reveal_child(false);
    rev_cola.set_hexpand(false);  // si no, el panel oculto se queda con la mitad del ancho
    cola_externa.set_hexpand(false);
    caja_cola.set_hexpand(false);

    contenido.set_vexpand(true);
    contenido.append(panel);
    contenido.append(rev_cola);

    // --- Barra inferior ---
    caratula.set_size_request(TAM_CARATULA, TAM_CARATULA);
    caratula.set_can_shrink(true);
    caratula.set_content_fit(Gtk::ContentFit::COVER);
    caratula.set_margin(10);
    caratula.set_halign(Gtk::Align::START);
    caratula.set_valign(Gtk::Align::CENTER);
    caratula.set_hexpand(false);
    caratula.set_vexpand(false);
    icono_generico = Gtk::IconTheme::get_for_display(get_display())->lookup_icon("audio-x-generic", TAM_CARATULA);
    mostrar_icono_generico();

    b_ant.set_icon_name("media-skip-backward-symbolic");
    b_play.set_icon_name("media-playback-start-symbolic");
    b_sig.set_icon_name("media-skip-forward-symbolic");
    actualizar_boton_repetir();

    l_titulo.set_xalign(0);
    l_titulo.set_ellipsize(Pango::EllipsizeMode::END);
    l_titulo.set_markup("<b>Elige una carpeta de música</b>");
    l_estado.set_xalign(0);
    l_estado.add_css_class("dim-label");
    l_pos.set_text("0:00");
    l_dur.set_text("0:00");
    escala.set_range(0, 1);
    escala.set_draw_value(false);
    escala.set_hexpand(true);

    i_vol.set_from_icon_name("audio-volume-high-symbolic");
    esc_vol.set_range(0, 1);
    esc_vol.set_draw_value(false);
    esc_vol.set_size_request(110, -1);
    esc_vol.set_value(volumen);
    esc_vol.set_tooltip_text("Volumen");
    motor.volumen(volumen);

    fila_botones.set_spacing(8);
    fila_botones.append(b_ant);
    fila_botones.append(b_play);
    fila_botones.append(b_sig);
    fila_botones.append(b_repetir);
    fila_botones.append(l_pos);
    fila_botones.append(escala);
    fila_botones.append(l_dur);
    fila_botones.append(i_vol);
    fila_botones.append(esc_vol);

    barra.set_spacing(6);
    barra.set_margin(10);
    barra.set_hexpand(true);
    barra.append(l_titulo);
    barra.append(fila_botones);
    barra.append(l_estado);

    pie.append(caratula);
    pie.append(barra);

    raiz.append(barra_busqueda);
    raiz.append(contenido);
    raiz.append(pie);
    set_child(raiz);

    // --- Señales ---
    b_carpeta.signal_clicked().connect(sigc::mem_fun(*this, &Ventana::elegir_carpeta));
    b_aleatorio.signal_clicked().connect(sigc::mem_fun(*this, &Ventana::aleatorio));
    b_cola.signal_toggled().connect([this] { rev_cola.set_reveal_child(b_cola.get_active()); });
    b_vaciar_cola.signal_clicked().connect([this] {
        a_continuacion.clear();
        refrescar_cola();
    });
    b_ant.signal_clicked().connect(sigc::mem_fun(*this, &Ventana::anterior));
    b_sig.signal_clicked().connect(sigc::mem_fun(*this, &Ventana::siguiente));
    b_repetir.signal_clicked().connect(sigc::mem_fun(*this, &Ventana::ciclar_repetir));

    // Buscador: botón de la cabecera <-> barra de búsqueda
    b_buscar.signal_toggled().connect([this] { barra_busqueda.set_search_mode(b_buscar.get_active()); });
    barra_busqueda.property_search_mode_enabled().signal_changed().connect([this] {
        const bool abierta = barra_busqueda.get_search_mode();
        if (b_buscar.get_active() != abierta) b_buscar.set_active(abierta);
        if (!abierta) entrada_busqueda.set_text("");  // al cerrar (Esc) se quita el filtro
    });
    entrada_busqueda.signal_search_changed().connect([this] {
        filtro = normalizar(entrada_busqueda.get_text());
        aplicar_filtro();
    });
    entrada_busqueda.signal_activate().connect([this] {  // Enter: reproducir el primer resultado
        if (vista.empty()) return;
        cola = vista;
        errores_seguidos = 0;
        reproducir_en(0);
    });
    auto teclas = Gtk::EventControllerKey::create();
    teclas->signal_key_pressed().connect(sigc::mem_fun(*this, &Ventana::al_tecla), false);
    add_controller(teclas);
    b_play.signal_clicked().connect(sigc::mem_fun(*this, &Ventana::alternar_pausa));
    esc_vol.signal_value_changed().connect(sigc::mem_fun(*this, &Ventana::al_cambiar_volumen));

    t_canciones.signal_toggled().connect([this] { if (t_canciones.get_active()) cambiar_modo(Modo::Canciones); });
    t_artistas.signal_toggled().connect([this] { if (t_artistas.get_active()) cambiar_modo(Modo::Artistas); });
    t_albumes.signal_toggled().connect([this] { if (t_albumes.get_active()) cambiar_modo(Modo::Albumes); });
    t_generos.signal_toggled().connect([this] { if (t_generos.get_active()) cambiar_modo(Modo::Generos); });
    t_parati.signal_toggled().connect([this] { if (t_parati.get_active()) cambiar_modo(Modo::ParaTi); });
    b_renovar.signal_clicked().connect([this] {
        seleccion_aleatoria.clear();
        grupo_sel = 1;  // mostrar la lista aleatoria nueva
        reconstruir_grupos();
    });

    lista_grupos.signal_row_selected().connect(sigc::mem_fun(*this, &Ventana::al_elegir_grupo));
    lista_canciones.signal_row_activated().connect(sigc::mem_fun(*this, &Ventana::al_activar_cancion));
    lista_cola.signal_row_activated().connect([this](Gtk::ListBoxRow* fila) {
        if (!fila) return;
        const auto k = static_cast<std::size_t>(fila->get_index());
        Glib::signal_idle().connect_once([this, k] { tocar_de_cola(k); });
    });
    escala.signal_change_value().connect(sigc::mem_fun(*this, &Ventana::al_mover_escala), false);

    motor.al_terminar.connect([this] {
        errores_seguidos = 0;
        if (repetir == Repetir::Una && actual >= 0) reproducir_cancion(actual);
        else siguiente();
    });
    motor.al_error.connect(sigc::mem_fun(*this, &Ventana::al_error_motor));
    dispatcher.connect(sigc::mem_fun(*this, &Ventana::al_escaneo_listo));
    Glib::signal_timeout().connect(sigc::mem_fun(*this, &Ventana::actualizar_posicion), 500);
    Glib::signal_timeout().connect_seconds([this] {  // mientras suena, se guarda la posición
        if (actual >= 0 && motor.reproduciendo()) guardar_sesion();
        return true;
    }, 10);

    // Control externo (applet de volumen, teclas multimedia, playerctl)
    mpris.posicion = [this] { return motor.posicion(); };
    mpris.al_alternar.connect(sigc::mem_fun(*this, &Ventana::alternar_pausa));
    mpris.al_siguiente.connect(sigc::mem_fun(*this, &Ventana::siguiente));
    mpris.al_anterior.connect(sigc::mem_fun(*this, &Ventana::anterior));
    mpris.al_play.connect([this] { if (actual < 0 || !motor.reproduciendo()) alternar_pausa(); });
    mpris.al_pausa.connect([this] { if (actual >= 0 && motor.reproduciendo()) alternar_pausa(); });
    mpris.al_detener.connect(sigc::mem_fun(*this, &Ventana::detener));
    mpris.al_volumen.connect([this](double v) { esc_vol.set_value(std::clamp(v, 0.0, 1.0)); });
    mpris.al_repetir.connect([this](const std::string& m) {
        fijar_repetir(m == "Track" ? Repetir::Una : m == "Playlist" ? Repetir::Lista : Repetir::No);
    });
    al_cambiar_volumen();

    // Recuperar la última carpeta usada
    if (!carpeta_actual.empty() && fs::is_directory(carpeta_actual)) cargar_carpeta(carpeta_actual);
}

Ventana::~Ventana() {
    if (hilo.joinable()) hilo.join();
    guardar_config();
    guardar_sesion();
    if (!archivo_caratula_cache.empty()) {
        std::error_code ec;
        fs::remove(archivo_caratula_cache, ec);
    }
}

// ----------------------------------------------------------- Configuración

std::string Ventana::archivo_config() const {
    return Glib::build_filename(Glib::get_user_config_dir(), "gcmusic", "config.ini");
}

void Ventana::cargar_config() {
    auto kf = Glib::KeyFile::create();
    try {
        kf->load_from_file(archivo_config());
        try { carpeta_actual = kf->get_string("gcmusic", "carpeta"); } catch (const Glib::Error&) {}
        try { volumen = std::clamp(kf->get_double("gcmusic", "volumen"), 0.0, 1.0); } catch (const Glib::Error&) {}
        try {
            const int r = std::clamp(kf->get_integer("gcmusic", "repetir"), 0, 2);
            repetir = r == 0 ? Repetir::No : r == 1 ? Repetir::Lista : Repetir::Una;
        } catch (const Glib::Error&) {}
        try {
            for (const auto& ruta : kf->get_string_list("gcmusic", "cola")) cola_pendiente.push_back(ruta);
        } catch (const Glib::Error&) {}
    } catch (const Glib::Error&) {
        // Primera vez: intentar leer la carpeta que guardaba la versión anterior ("reproductor-gtk")
        std::ifstream f(Glib::build_filename(Glib::get_user_config_dir(), "reproductor-gtk", "carpeta.txt"));
        std::getline(f, carpeta_actual);
    }
}

void Ventana::guardar_config() const {
    std::error_code ec;
    fs::create_directories(fs::path(archivo_config()).parent_path(), ec);
    auto kf = Glib::KeyFile::create();
    kf->set_string("gcmusic", "carpeta", carpeta_actual);
    kf->set_double("gcmusic", "volumen", volumen);
    kf->set_integer("gcmusic", "repetir", repetir == Repetir::No ? 0 : repetir == Repetir::Lista ? 1 : 2);
    std::vector<Glib::ustring> cola_texto;
    for (const auto& ruta : rutas_cola()) cola_texto.push_back(utf8_seguro(ruta));
    kf->set_string_list("gcmusic", "cola", cola_texto);
    try { kf->save_to_file(archivo_config()); } catch (const Glib::Error&) {}
}

std::vector<std::string> Ventana::rutas_cola() const {
    // Mientras se escanea, los índices no valen: se conserva la cola tal como estaba guardada
    if (escaneando) return cola_pendiente;
    std::vector<std::string> rutas;
    for (int i : a_continuacion) rutas.push_back(canciones[i].ruta);
    return rutas;
}

void Ventana::programar_guardado() {
    if (guardado_pendiente) return;  // guardar sin esperar a cerrar, pero sin escribir en cada cambio
    guardado_pendiente = true;
    Glib::signal_timeout().connect_once([this] {
        guardado_pendiente = false;
        guardar_config();
    }, 1500);
}

// ---------------------------------------------------------------- Carpeta

void Ventana::elegir_carpeta() {
    auto dialogo = Gtk::FileDialog::create();
    dialogo->set_title("Elegir carpeta de música");
    dialogo->select_folder(*this, [this, dialogo](const Glib::RefPtr<Gio::AsyncResult>& res) {
        try {
            auto carpeta = dialogo->select_folder_finish(res);
            if (carpeta) cargar_carpeta(carpeta->get_path());
        } catch (const Glib::Error&) {
            // el usuario canceló
        }
    });
}

void Ventana::cargar_carpeta(const std::string& ruta) {
    if (escaneando) return;
    if (!a_continuacion.empty()) cola_pendiente = rutas_cola();  // se restaura lo que exista en la nueva carpeta
    escaneando = true;
    carpeta_actual = ruta;
    b_carpeta.set_sensitive(false);
    l_estado.set_text("Escaneando " + ruta + " …");

    hilo = std::thread([this, ruta] {
        auto res = escanear_carpeta(ruta);
        {
            std::lock_guard<std::mutex> lk(mtx);
            resultado = std::move(res);
        }
        dispatcher.emit();
    });
}

void Ventana::al_escaneo_listo() {
    if (hilo.joinable()) hilo.join();
    {
        std::lock_guard<std::mutex> lk(mtx);
        canciones = std::move(resultado);
        resultado.clear();
    }
    escaneando = false;
    b_carpeta.set_sensitive(true);

    // Los índices anteriores ya no sirven
    detener();
    a_continuacion.clear();

    indice_busqueda.clear();
    std::map<std::string, int> por_ruta;
    for (std::size_t i = 0; i < canciones.size(); ++i) {
        const Cancion& c = canciones[i];
        indice_busqueda.push_back(normalizar(c.titulo + " " + c.artista + " " + c.album + " " + c.genero));
        por_ruta[c.ruta] = static_cast<int>(i);
    }

    // Restaurar la cola guardada (las canciones que ya no existan se omiten)
    for (const auto& ruta : cola_pendiente) {
        const auto it = por_ruta.find(ruta);
        if (it != por_ruta.end()) a_continuacion.push_back(it->second);
    }
    cola_pendiente.clear();
    seleccion_aleatoria.clear();
    refrescar_cola();

    guardar_config();
    cambiar_modo(modo);  // vuelve a aplicar el filtro si hay uno escrito
    restaurar_sesion(por_ruta);  // retomar la canción donde quedó (solo en el primer escaneo)
}

// ----------------------------------------------------------------- Vistas

void Ventana::actualizar_estado() {
    if (filtro.empty())
        l_estado.set_text(std::to_string(canciones.size()) + " canciones — " + carpeta_actual);
    else
        l_estado.set_text(std::to_string(vista.size()) + " resultados de " + std::to_string(canciones.size()) +
                          " canciones");
}

void Ventana::aplicar_filtro() {
    if (filtro.empty()) {
        cambiar_modo(modo);  // volver a la vista normal
        return;
    }
    // Todas las palabras escritas deben aparecer (en título, artista o álbum)
    std::vector<std::string> palabras;
    std::size_t ini = 0;
    while (ini < filtro.size()) {
        const auto fin = filtro.find(' ', ini);
        if (fin != ini) palabras.push_back(filtro.substr(ini, fin == std::string::npos ? fin : fin - ini));
        if (fin == std::string::npos) break;
        ini = fin + 1;
    }
    vista.clear();
    for (std::size_t i = 0; i < indice_busqueda.size(); ++i) {
        const bool todas = std::all_of(palabras.begin(), palabras.end(), [&](const std::string& w) {
            return indice_busqueda[i].find(w) != std::string::npos;
        });
        if (todas) vista.push_back(static_cast<int>(i));
    }
    caja_grupos.set_visible(false);  // al buscar se ve toda la biblioteca
    llenar_canciones();
    actualizar_estado();
}

void Ventana::cambiar_modo(Modo m) {
    if (m != modo) grupo_sel = -1;
    modo = m;
    if (!filtro.empty()) {
        aplicar_filtro();
        return;
    }
    caja_grupos.set_visible(m != Modo::Canciones);
    b_renovar.set_visible(m == Modo::ParaTi);

    if (m == Modo::Canciones) {
        vista.clear();
        for (std::size_t i = 0; i < canciones.size(); ++i) vista.push_back(static_cast<int>(i));
        llenar_canciones();
    } else {
        reconstruir_grupos();
    }
    actualizar_estado();
}

std::vector<Ventana::Grupo> Ventana::grupos_por_genero() const {
    // Una canción puede tener varios géneros ("Rock; Pop"): aparece en cada uno.
    // Se agrupa sin distinguir mayúsculas ni tildes ("rock" y "Rock" son el mismo).
    struct Entrada { std::string nombre; std::vector<int> indices; };
    std::map<std::string, Entrada> mapa;

    for (std::size_t i = 0; i < canciones.size(); ++i) {
        std::vector<std::string> generos;
        const std::string& crudo = canciones[i].genero;
        std::size_t ini = 0;
        while (ini <= crudo.size()) {
            std::size_t fin = crudo.size();
            std::size_t salto = 1;
            const auto pc = crudo.find(';', ini);
            const auto sl = crudo.find(" / ", ini);
            if (pc != std::string::npos && pc < fin) fin = pc;
            if (sl != std::string::npos && sl < fin) { fin = sl; salto = 3; }
            std::string g = crudo.substr(ini, fin - ini);
            g.erase(0, g.find_first_not_of(" \t"));
            if (const auto ultimo = g.find_last_not_of(" \t"); ultimo != std::string::npos) g.erase(ultimo + 1);
            else g.clear();
            if (!g.empty()) generos.push_back(g);
            if (fin >= crudo.size()) break;
            ini = fin + salto;
        }
        if (generos.empty()) generos.push_back("Sin género");

        for (const auto& g : generos) {
            auto& e = mapa[normalizar(g)];
            // para mostrar, se prefiere la escritura con mayúscula ("Rock" antes que "rock")
            if (e.nombre.empty() || (!con_mayuscula(e.nombre) && con_mayuscula(g))) e.nombre = g;
            if (e.indices.empty() || e.indices.back() != static_cast<int>(i)) e.indices.push_back(static_cast<int>(i));
        }
    }
    std::vector<Grupo> resultado;
    for (auto& [clave, e] : mapa) resultado.push_back({e.nombre, std::move(e.indices)});
    return resultado;
}

std::vector<Ventana::Grupo> Ventana::grupos_para_ti() {
    std::vector<int> todas(canciones.size());
    for (std::size_t i = 0; i < todas.size(); ++i) todas[i] = static_cast<int>(i);

    // Agregadas recientemente: las 50 con la fecha más nueva
    std::vector<int> recientes = todas;
    std::stable_sort(recientes.begin(), recientes.end(),
                     [&](int a, int b) { return canciones[a].fecha > canciones[b].fecha; });
    if (recientes.size() > 50) recientes.resize(50);

    // Selección aleatoria: 30 canciones; se conservan hasta pulsar "Renovar"
    if (seleccion_aleatoria.empty()) {
        std::mt19937 gen{std::random_device{}()};
        std::shuffle(todas.begin(), todas.end(), gen);
        if (todas.size() > 30) todas.resize(30);
        seleccion_aleatoria = std::move(todas);
    }
    std::vector<Grupo> resultado;
    if (!canciones.empty()) {
        resultado.push_back({"Agregadas recientemente", std::move(recientes)});
        resultado.push_back({"Selección aleatoria", seleccion_aleatoria});
    }
    return resultado;
}

void Ventana::reconstruir_grupos() {
    reconstruyendo = true;
    vaciar(lista_grupos);
    grupos.clear();

    if (modo == Modo::ParaTi) {
        grupos = grupos_para_ti();
    } else if (modo == Modo::Generos) {
        grupos = grupos_por_genero();
    } else {
        std::map<std::string, std::vector<int>> mapa;
        for (std::size_t i = 0; i < canciones.size(); ++i)
            mapa[modo == Modo::Artistas ? canciones[i].artista : canciones[i].album].push_back(static_cast<int>(i));
        for (auto& [nombre, indices] : mapa) grupos.push_back({nombre, std::move(indices)});
    }

    for (const auto& g : grupos) {
        auto* etiqueta = Gtk::make_managed<Gtk::Label>();
        etiqueta->set_markup(escapar(g.nombre) + "  <small>(" + std::to_string(g.indices.size()) + ")</small>");
        etiqueta->set_xalign(0);
        etiqueta->set_margin(8);
        lista_grupos.append(*etiqueta);
    }
    reconstruyendo = false;

    vista.clear();
    const int sel = (grupo_sel >= 0 && grupo_sel < static_cast<int>(grupos.size())) ? grupo_sel : 0;
    if (auto* fila = lista_grupos.get_row_at_index(sel))
        lista_grupos.select_row(*fila);  // dispara al_elegir_grupo
    else
        llenar_canciones();
}

void Ventana::al_elegir_grupo(Gtk::ListBoxRow* fila) {
    if (reconstruyendo || !fila) return;
    const auto idx = static_cast<std::size_t>(fila->get_index());
    if (idx >= grupos.size()) return;
    grupo_sel = static_cast<int>(idx);
    vista = grupos[idx].indices;
    llenar_canciones();
}

Gtk::Box* Ventana::crear_fila(const Cancion& c, const char* icono, const char* ayuda,
                              std::function<void()> accion) {
    auto* caja = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    caja->set_spacing(6);

    auto* etiqueta = Gtk::make_managed<Gtk::Label>();
    etiqueta->set_markup("<b>" + escapar(c.titulo) + "</b>\n<small>" + escapar(c.artista) + " — " +
                         escapar(c.album) + "</small>");
    etiqueta->set_xalign(0);
    etiqueta->set_hexpand(true);
    etiqueta->set_ellipsize(Pango::EllipsizeMode::END);
    etiqueta->set_margin(6);

    auto* boton = Gtk::make_managed<Gtk::Button>();
    boton->set_icon_name(icono);
    boton->set_tooltip_text(ayuda);
    boton->add_css_class("flat");
    boton->set_valign(Gtk::Align::CENTER);
    boton->signal_clicked().connect(std::move(accion));

    caja->append(*etiqueta);
    caja->append(*boton);
    return caja;
}

void Ventana::llenar_canciones() {
    vaciar(lista_canciones);
    for (int i : vista)
        lista_canciones.append(*crear_fila(canciones[i], "list-add-symbolic", "Añadir a la cola",
                                           [this, i] { agregar_a_cola(i); }));
}

void Ventana::al_activar_cancion(Gtk::ListBoxRow* fila) {
    if (!fila) return;
    errores_seguidos = 0;
    cola = vista;
    reproducir_en(static_cast<std::size_t>(fila->get_index()));
}

// ------------------------------------------------------------ Reproducción

void Ventana::aleatorio() {
    // Mezcla lo que se ve (un artista, un álbum o todo); si no hay nada, toda la biblioteca.
    std::vector<int> fuente = vista;
    if (fuente.empty())
        for (std::size_t i = 0; i < canciones.size(); ++i) fuente.push_back(static_cast<int>(i));
    if (fuente.empty()) return;

    std::mt19937 gen{std::random_device{}()};
    std::shuffle(fuente.begin(), fuente.end(), gen);
    cola = std::move(fuente);
    errores_seguidos = 0;
    reproducir_en(0);
}

void Ventana::reproducir_en(std::size_t pos) {
    if (cola.empty()) return;
    pos_cola = pos % cola.size();
    tocando_de_cola = false;
    reproducir_cancion(cola[pos_cola]);
}

void Ventana::reproducir_cancion(int idx, bool iniciar, double desde) {
    if (idx < 0 || idx >= static_cast<int>(canciones.size())) return;
    actual = idx;
    const Cancion& c = canciones[idx];

    if (iniciar) {
        motor.reproducir(c.ruta);
        b_play.set_icon_name("media-playback-pause-symbolic");
    } else {  // cargar en pausa, lista para seguir desde "desde"
        motor.preparar(c.ruta, desde);
        b_play.set_icon_name("media-playback-start-symbolic");
    }
    ultima_posicion = iniciar ? 0 : desde;
    l_titulo.set_markup("<b>" + escapar(c.titulo) + "</b>  <small>" + escapar(c.artista) + " — " +
                        escapar(c.album) + "</small>");
    ultima_duracion = -1;
    escala.set_value(iniciar ? 0 : desde);
    if (!iniciar) l_pos.set_text(formatear_tiempo(desde));
    mostrar_caratula(c);

    // Marcar la canción en la lista si está visible
    const auto it = std::find(vista.begin(), vista.end(), idx);
    if (it != vista.end())
        if (auto* fila = lista_canciones.get_row_at_index(static_cast<int>(it - vista.begin())))
            lista_canciones.select_row(*fila);

    sincronizar_mpris();
    programar_sesion();
}

void Ventana::siguiente() {
    // Primero lo que el usuario puso en la cola; después se sigue con la lista de contexto
    if (!a_continuacion.empty()) {
        const int idx = a_continuacion.front();
        a_continuacion.pop_front();
        tocando_de_cola = true;
        reproducir_cancion(idx);
        refrescar_cola();
        return;
    }
    if (cola.empty()) {
        if (actual >= 0) detener();
        return;
    }
    if (pos_cola + 1 >= cola.size() && repetir == Repetir::No) {  // fin de la lista sin repetir
        detener();
        return;
    }
    reproducir_en(pos_cola + 1);
}

void Ventana::anterior() {
    if (actual < 0) return;
    if (motor.posicion() > 3.0) {
        motor.buscar(0);
        return;
    }
    if (tocando_de_cola) {  // volver a la canción de la lista que sonó antes de la cola
        if (cola.empty()) motor.buscar(0);
        else reproducir_en(pos_cola);
        return;
    }
    if (cola.empty()) return;
    if (pos_cola == 0) {
        if (repetir == Repetir::No) motor.buscar(0);  // primera canción y sin repetir: reinicia
        else reproducir_en(cola.size() - 1);
        return;
    }
    reproducir_en(pos_cola - 1);
}

void Ventana::alternar_pausa() {
    if (actual < 0) {
        if (!a_continuacion.empty()) siguiente();
        else aleatorio();
        return;
    }
    if (motor.reproduciendo()) {
        motor.pausar();
        b_play.set_icon_name("media-playback-start-symbolic");
    } else {
        motor.reanudar();
        b_play.set_icon_name("media-playback-pause-symbolic");
    }
    sincronizar_mpris();
    guardar_sesion();
}

void Ventana::detener() {
    motor.detener();
    actual = -1;
    tocando_de_cola = false;
    cola.clear();
    b_play.set_icon_name("media-playback-start-symbolic");
    l_titulo.set_markup("<b>Nada en reproducción</b>");
    escala.set_value(0);
    l_pos.set_text("0:00");
    l_dur.set_text("0:00");
    ultima_duracion = -1;
    url_caratula.clear();
    mostrar_icono_generico();
    sincronizar_mpris();
    programar_sesion();
}

void Ventana::al_error_motor(const std::string& mensaje) {
    l_estado.set_text("Error: " + mensaje);
    const std::size_t total = std::max<std::size_t>(cola.size() + a_continuacion.size(), 1);
    if (actual < 0 || ++errores_seguidos >= static_cast<int>(total)) {
        b_play.set_icon_name("media-playback-start-symbolic");
        sincronizar_mpris();
        return;
    }
    siguiente();
}

void Ventana::sincronizar_mpris() {
    if (actual < 0) {
        mpris.actualizar("Stopped", -1, "", "", "", -1, "", !canciones.empty());
        return;
    }
    const Cancion& c = canciones[actual];
    mpris.actualizar(motor.reproduciendo() ? "Playing" : "Paused", actual, c.titulo, c.artista,
                     c.album, motor.duracion(), url_caratula, true);
}

bool Ventana::actualizar_posicion() {
    const double dur = motor.duracion();
    const double pos = motor.posicion();

    if (dur > 0 && dur != ultima_duracion) {
        ultima_duracion = dur;
        escala.set_range(0, dur);
        l_dur.set_text(formatear_tiempo(dur));
        sincronizar_mpris();
    }
    // No pelear con el usuario mientras arrastra la barra
    const bool arrastrando = (g_get_monotonic_time() - ultimo_arrastre) < 1000000;
    if (pos >= 0) ultima_posicion = pos;
    if (pos >= 0 && !arrastrando) {
        escala.set_value(pos);
        l_pos.set_text(formatear_tiempo(pos));
    }
    return true;
}

bool Ventana::al_mover_escala(Gtk::ScrollType, double valor) {
    ultimo_arrastre = g_get_monotonic_time();
    ultima_posicion = valor;
    motor.buscar(valor);
    l_pos.set_text(formatear_tiempo(valor));
    return false;
}

// ------------------------------------------------------------------ Volumen

void Ventana::al_cambiar_volumen() {
    volumen = esc_vol.get_value();
    motor.volumen(volumen);
    const char* icono = volumen <= 0.0   ? "audio-volume-muted-symbolic"
                        : volumen < 0.34 ? "audio-volume-low-symbolic"
                        : volumen < 0.67 ? "audio-volume-medium-symbolic"
                                         : "audio-volume-high-symbolic";
    i_vol.set_from_icon_name(icono);
    mpris.actualizar_volumen(volumen);

    programar_guardado();
}

// --------------------------------------------------------------------- Cola

void Ventana::agregar_a_cola(int idx) {
    if (actual < 0) {
        // No suena nada: es como darle doble clic a la canción
        cola = vista;
        const auto it = std::find(vista.begin(), vista.end(), idx);
        errores_seguidos = 0;
        reproducir_en(it == vista.end() ? 0 : static_cast<std::size_t>(it - vista.begin()));
        return;
    }
    a_continuacion.push_back(idx);
    refrescar_cola();
}

void Ventana::refrescar_cola() {
    vaciar(lista_cola);
    for (std::size_t k = 0; k < a_continuacion.size(); ++k) {
        lista_cola.append(*crear_fila(canciones[a_continuacion[k]], "window-close-symbolic", "Quitar de la cola",
                                      [this, k] {
                                          // se difiere: quitar la fila destruye el botón que emitió la señal
                                          Glib::signal_idle().connect_once([this, k] { quitar_de_cola(k); });
                                      }));
    }
    b_cola.set_label(a_continuacion.empty() ? "Cola" : "Cola (" + std::to_string(a_continuacion.size()) + ")");
    programar_guardado();
}

void Ventana::quitar_de_cola(std::size_t k) {
    if (k >= a_continuacion.size()) return;
    a_continuacion.erase(a_continuacion.begin() + static_cast<std::ptrdiff_t>(k));
    refrescar_cola();
}

void Ventana::tocar_de_cola(std::size_t k) {
    if (k >= a_continuacion.size()) return;
    const int idx = a_continuacion[k];
    a_continuacion.erase(a_continuacion.begin() + static_cast<std::ptrdiff_t>(k));
    tocando_de_cola = true;
    errores_seguidos = 0;
    reproducir_cancion(idx);
    refrescar_cola();
}

// ----------------------------------------------------------------- Carátula

void Ventana::mostrar_icono_generico() {
    if (icono_generico) caratula.set_paintable(icono_generico);
}

void Ventana::mostrar_caratula(const Cancion& c) {
    url_caratula.clear();
    if (!archivo_caratula_cache.empty()) {
        std::error_code ec;
        fs::remove(archivo_caratula_cache, ec);
        archivo_caratula_cache.clear();
    }

    const Caratula car = buscar_caratula(c.ruta);
    if (!car.datos.empty()) {
        try {
            // Un Gtk::Picture pide el tamaño original de la imagen, y una portada grande agrandaría
            // toda la barra: se recorta a cuadrado y se reduce a un tamaño fijo antes de mostrarla.
            auto cargador = Gdk::PixbufLoader::create();
            cargador->write(reinterpret_cast<const guint8*>(car.datos.data()), car.datos.size());
            cargador->close();
            auto original = cargador->get_pixbuf();
            if (!original) throw Glib::Error(G_FILE_ERROR, 0, "imagen no soportada");
            const int lado = std::min(original->get_width(), original->get_height());
            auto cuadrada = Gdk::Pixbuf::create_subpixbuf(original, (original->get_width() - lado) / 2,
                                                          (original->get_height() - lado) / 2, lado, lado);
            auto reducida = cuadrada->scale_simple(TAM_CARATULA, TAM_CARATULA, Gdk::InterpType::HYPER);
            caratula.set_paintable(Gdk::Texture::create_for_pixbuf(reducida));

            // Para MPRIS: los clientes piden la imagen por URL de archivo
            if (!car.ruta.empty()) {
                url_caratula = Glib::filename_to_uri(car.ruta);
            } else {
                const std::string dir = Glib::build_filename(Glib::get_user_cache_dir(), "gcmusic");
                std::error_code ec;
                fs::create_directories(dir, ec);
                const std::string ruta = Glib::build_filename(
                    dir, "caratula-" + std::to_string(++contador_caratula) + car.extension);
                std::ofstream f(ruta, std::ios::binary);
                if (f.write(car.datos.data(), static_cast<std::streamsize>(car.datos.size()))) {
                    archivo_caratula_cache = ruta;
                    url_caratula = Glib::filename_to_uri(ruta);
                }
            }
            return;
        } catch (const Glib::Error&) {
            // formato de imagen no soportado: se usa el ícono genérico
        }
    }
    mostrar_icono_generico();
}

// ------------------------------------------------------------------ Repetir

void Ventana::ciclar_repetir() {
    fijar_repetir(repetir == Repetir::No ? Repetir::Lista : repetir == Repetir::Lista ? Repetir::Una : Repetir::No);
}

void Ventana::fijar_repetir(Repetir r) {
    repetir = r;
    actualizar_boton_repetir();
    programar_guardado();
}

void Ventana::actualizar_boton_repetir() {
    b_repetir.set_icon_name(repetir == Repetir::Una ? "media-playlist-repeat-song-symbolic"
                                                    : "media-playlist-repeat-symbolic");
    if (repetir == Repetir::No) b_repetir.remove_css_class("suggested-action");
    else b_repetir.add_css_class("suggested-action");
    b_repetir.set_tooltip_text(repetir == Repetir::No      ? "Repetir: desactivado"
                               : repetir == Repetir::Lista ? "Repetir: toda la lista"
                                                           : "Repetir: esta canción");
    mpris.actualizar_repetir(repetir == Repetir::No ? "None" : repetir == Repetir::Lista ? "Playlist" : "Track");
}

// ------------------------------------------------------------------ Teclado

bool Ventana::al_tecla(guint tecla, guint, Gdk::ModifierType estado) {
    const bool ctrl = (estado & Gdk::ModifierType::CONTROL_MASK) == Gdk::ModifierType::CONTROL_MASK;
    if (ctrl && (tecla == GDK_KEY_f || tecla == GDK_KEY_F)) {
        b_buscar.set_active(true);
        entrada_busqueda.grab_focus();
        return true;
    }
    return false;
}

// ------------------------------------------------------------------- Sesión

std::string Ventana::archivo_sesion() const {
    return Glib::build_filename(Glib::get_user_config_dir(), "gcmusic", "sesion.ini");
}

void Ventana::cargar_sesion() {
    auto kf = Glib::KeyFile::create();
    try {
        kf->load_from_file(archivo_sesion());
        Sesion s;
        s.ruta_actual = kf->get_string("sesion", "ruta");
        if (s.ruta_actual.empty()) return;
        try { s.posicion = std::max(0.0, kf->get_double("sesion", "posicion")); } catch (const Glib::Error&) {}
        try { s.de_cola = kf->get_boolean("sesion", "de_cola"); } catch (const Glib::Error&) {}
        try { s.pos_contexto = kf->get_integer("sesion", "pos_contexto"); } catch (const Glib::Error&) {}
        try {
            for (const auto& ruta : kf->get_string_list("sesion", "contexto")) s.contexto.push_back(ruta);
        } catch (const Glib::Error&) {}
        s.valida = true;
        sesion_pendiente = std::move(s);
    } catch (const Glib::Error&) {
        // no hay sesión guardada
    }
}

void Ventana::guardar_sesion() {
    // Mientras se escanea (o la sesión anterior no se ha restaurado) no se toca el archivo
    if (escaneando || sesion_pendiente.valida) return;

    std::error_code ec;
    fs::create_directories(fs::path(archivo_sesion()).parent_path(), ec);

    auto kf = Glib::KeyFile::create();
    if (actual >= 0 && actual < static_cast<int>(canciones.size())) {
        kf->set_string("sesion", "ruta", utf8_seguro(canciones[actual].ruta));
        kf->set_double("sesion", "posicion", ultima_posicion);
        kf->set_boolean("sesion", "de_cola", tocando_de_cola);

        // Lista de contexto (de dónde viene la canción): se guardan como mucho 3000 entradas
        const std::size_t inicio = cola.size() > 3000 ? pos_cola : 0;
        const std::size_t fin = std::min(cola.size(), inicio + 3000);
        std::vector<Glib::ustring> contexto;
        for (std::size_t i = inicio; i < fin; ++i) contexto.push_back(utf8_seguro(canciones[cola[i]].ruta));
        kf->set_integer("sesion", "pos_contexto", cola.empty() ? 0 : static_cast<int>(pos_cola - inicio));
        kf->set_string_list("sesion", "contexto", contexto);
    }
    try { kf->save_to_file(archivo_sesion()); } catch (const Glib::Error&) {}
}

void Ventana::programar_sesion() {
    if (sesion_programada) return;  // varios cambios seguidos (saltar canciones) = una sola escritura
    sesion_programada = true;
    Glib::signal_timeout().connect_once([this] {
        sesion_programada = false;
        guardar_sesion();
    }, 1500);
}

void Ventana::restaurar_sesion(const std::map<std::string, int>& por_ruta) {
    const Sesion s = std::move(sesion_pendiente);
    sesion_pendiente = Sesion{};  // se intenta una sola vez, en el primer escaneo
    if (!s.valida) return;

    const auto actual_it = por_ruta.find(s.ruta_actual);
    if (actual_it == por_ruta.end()) return;  // la canción ya no está en la carpeta

    // Reconstruir la lista de contexto con las canciones que sigan existiendo
    cola.clear();
    std::size_t pos_nueva = 0;
    for (std::size_t i = 0; i < s.contexto.size(); ++i) {
        if (static_cast<int>(i) == s.pos_contexto) pos_nueva = cola.size();
        const auto c = por_ruta.find(s.contexto[i]);
        if (c != por_ruta.end()) cola.push_back(c->second);
    }
    if (!s.de_cola) {
        const auto en = std::find(cola.begin(), cola.end(), actual_it->second);
        if (en != cola.end()) {
            pos_nueva = static_cast<std::size_t>(en - cola.begin());
        } else {
            cola = {actual_it->second};
            pos_nueva = 0;
        }
    }
    pos_cola = cola.empty() ? 0 : std::min(pos_nueva, cola.size() - 1);
    tocando_de_cola = s.de_cola;

    reproducir_cancion(actual_it->second, /*iniciar=*/false, s.posicion);  // queda en pausa
}

// --------------------------------------------------------------- Acerca de

void Ventana::mostrar_acerca() {
    if (!acerca) {
        // Versiones de las bibliotecas: las de GTK y GStreamer son las que están en uso ahora
        gchar* gst = gst_version_string();
        char info[512];
        std::snprintf(info, sizeof info,
                      "GC Music %s\nLenguaje: C++17\nGTK %u.%u.%u (en uso)\ngtkmm %d.%d.%d (compilado con)\n%s\n"
                      "TagLib %d.%d (compilado con)",
                      GCMUSIC_VERSION, gtk_get_major_version(), gtk_get_minor_version(), gtk_get_micro_version(),
                      GTKMM_MAJOR_VERSION, GTKMM_MINOR_VERSION, GTKMM_MICRO_VERSION, gst, TAGLIB_MAJOR_VERSION,
                      TAGLIB_MINOR_VERSION);
        g_free(gst);

        acerca = std::make_unique<Gtk::AboutDialog>();
        acerca->set_transient_for(*this);
        acerca->set_modal(true);
        acerca->set_hide_on_close(true);  // se vuelve a usar la próxima vez
        acerca->set_program_name("GC Music");
        acerca->set_logo_icon_name("cl.pipe.gcmusic");
        acerca->set_version(GCMUSIC_VERSION);
        acerca->set_comments(
            "GTK C++ Music (gcmusic): reproductor de música hecho con GTK y C++.\n\n"
            "Software libre, distribuido «tal cual» y sin garantía de ningún tipo. Los autores "
            "no se hacen responsables de daños, pérdidas de datos u otros perjuicios derivados "
            "de su uso.");
        acerca->set_copyright("Copyright © 2026 felipesonic");
        acerca->set_license_type(Gtk::License::CUSTOM);
        acerca->set_license(TEXTO_LICENCIA);
        acerca->set_wrap_license(true);
        acerca->set_authors({"felipesonic"});
        acerca->add_credit_section("Programado con ayuda de IA", {MODELOS_IA});
        acerca->set_system_information(info);
    }
    acerca->present();
}

// ------------------------------------------------------- Tema del sistema

namespace {
// El portal entrega el valor envuelto en uno o dos "variant": se desenvuelve hasta el número.
bool leer_esquema(Glib::VariantBase v, guint32& esquema) {
    try {
        while (v.get_type_string() == "v")
            v = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::VariantBase>>(v).get();
        esquema = Glib::VariantBase::cast_dynamic<Glib::Variant<guint32>>(v).get();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
}  // namespace

// Las apps GTK4 "puras" no siguen solas el modo oscuro de GNOME (eso lo hace libadwaita), así que se
// lee la preferencia del sistema por el portal de escritorio (org.freedesktop.appearance) y se aplica.
void Ventana::seguir_tema_del_sistema() {
    auto ajustes = Gtk::Settings::get_default();
    if (!ajustes) return;
    // GTK 4.20 o más nuevo ya sigue la preferencia del sistema por sí solo
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(ajustes->gobj()), "gtk-interface-color-scheme")) return;
    tema_oscuro_inicial = ajustes->property_gtk_application_prefer_dark_theme().get_value();

    try {
        proxy_tema = Gio::DBus::Proxy::create_for_bus_sync(
            Gio::DBus::BusType::SESSION, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.Settings");

        const auto respuesta = proxy_tema->call_sync(
            "Read",
            Glib::VariantContainerBase::create_tuple(std::vector<Glib::VariantBase>{
                Glib::Variant<Glib::ustring>::create("org.freedesktop.appearance"),
                Glib::Variant<Glib::ustring>::create("color-scheme")}),
            1500);
        guint32 esquema = 0;
        if (leer_esquema(respuesta.get_child(0), esquema)) aplicar_esquema(esquema);

        // Si el usuario cambia entre claro y oscuro con la app abierta, se actualiza al instante
        proxy_tema->signal_signal().connect([this](const Glib::ustring&, const Glib::ustring& senal,
                                                   const Glib::VariantContainerBase& params) {
            if (senal != "SettingChanged" || params.get_n_children() < 3) return;
            Glib::Variant<Glib::ustring> ns, clave;
            params.get_child(ns, 0);
            params.get_child(clave, 1);
            guint32 esquema = 0;
            if (ns.get() == "org.freedesktop.appearance" && clave.get() == "color-scheme" &&
                leer_esquema(params.get_child(2), esquema))
                aplicar_esquema(esquema);
        });
    } catch (const Glib::Error&) {
        // sin portal (otros escritorios): se deja el tema que ya use GTK
        proxy_tema.reset();
    }
}

void Ventana::aplicar_esquema(guint32 esquema) {  // 0 = sin preferencia, 1 = oscuro, 2 = claro
    auto ajustes = Gtk::Settings::get_default();
    if (!ajustes) return;
    ajustes->property_gtk_application_prefer_dark_theme() =
        esquema == 1 ? true : esquema == 2 ? false : tema_oscuro_inicial;
}
