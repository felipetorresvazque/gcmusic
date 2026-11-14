#pragma once
#include <glibmm/dispatcher.h>
#include <gtkmm.h>

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "biblioteca.h"
#include "motor.h"
#include "mpris.h"

class Ventana : public Gtk::ApplicationWindow {
public:
    Ventana();
    ~Ventana() override;

private:
    enum class Modo { Canciones, Artistas, Albumes, Generos, ParaTi };
    enum class Repetir { No, Lista, Una };
    struct Grupo {
        std::string nombre;
        std::vector<int> indices;
    };

    // Configuración
    std::string archivo_config() const;
    void cargar_config();
    void guardar_config() const;
    void programar_guardado();
    std::vector<std::string> rutas_cola() const;

    // Carpeta y biblioteca
    void elegir_carpeta();
    void cargar_carpeta(const std::string& ruta);
    void al_escaneo_listo();

    // Vistas
    void cambiar_modo(Modo m);
    void reconstruir_grupos();
    std::vector<Grupo> grupos_para_ti();
    std::vector<Grupo> grupos_por_genero() const;
    void llenar_canciones();
    void aplicar_filtro();
    void actualizar_estado();
    void al_elegir_grupo(Gtk::ListBoxRow* fila);
    void al_activar_cancion(Gtk::ListBoxRow* fila);
    Gtk::Box* crear_fila(const Cancion& c, const char* icono, const char* ayuda,
                         std::function<void()> accion);

    // Reproducción
    void aleatorio();
    void reproducir_en(std::size_t pos);
    void reproducir_cancion(int idx, bool iniciar = true, double desde = 0);
    void siguiente();
    void anterior();
    void alternar_pausa();
    void detener();
    void al_error_motor(const std::string& mensaje);
    void sincronizar_mpris();
    bool actualizar_posicion();
    bool al_mover_escala(Gtk::ScrollType tipo, double valor);

    // Repetir
    void ciclar_repetir();
    void fijar_repetir(Repetir r);
    void actualizar_boton_repetir();

    // Teclado
    bool al_tecla(guint tecla, guint codigo, Gdk::ModifierType estado);

    // Volumen
    void al_cambiar_volumen();

    // Sesión: retomar la canción donde quedó
    struct Sesion {
        bool valida = false;
        std::string ruta_actual;
        double posicion = 0;
        bool de_cola = false;
        int pos_contexto = 0;
        std::vector<std::string> contexto;
    };
    std::string archivo_sesion() const;
    void cargar_sesion();
    void guardar_sesion();
    void programar_sesion();
    void restaurar_sesion(const std::map<std::string, int>& por_ruta);

    // Tema claro/oscuro del sistema
    void seguir_tema_del_sistema();
    void aplicar_esquema(guint32 esquema);

    // Acerca de
    void mostrar_acerca();

    // Cola "a continuación"
    void agregar_a_cola(int idx);
    void refrescar_cola();
    void quitar_de_cola(std::size_t k);
    void tocar_de_cola(std::size_t k);

    // Carátula
    void mostrar_caratula(const Cancion& c);
    void mostrar_icono_generico();

    // Datos
    Motor motor;
    Mpris mpris;
    std::vector<Cancion> canciones;
    std::vector<Grupo> grupos;
    std::vector<int> vista;               // canciones mostradas ahora
    std::vector<int> cola;                // lista de contexto (la vista donde se empezó)
    std::deque<int> a_continuacion;       // cola del usuario: suena antes de seguir con 'cola'
    std::size_t pos_cola = 0;
    int actual = -1;                      // canción sonando (índice en 'canciones'), -1 = ninguna
    bool tocando_de_cola = false;
    Modo modo = Modo::Canciones;
    Repetir repetir = Repetir::Lista;
    int grupo_sel = -1;                   // grupo elegido en Artistas/Álbumes
    std::string filtro;                   // texto del buscador, normalizado
    std::vector<int> seleccion_aleatoria;  // lista aleatoria de "Para ti" (se mantiene hasta renovarla)
    std::vector<std::string> indice_busqueda;  // texto normalizado de cada canción
    std::vector<std::string> cola_pendiente;   // cola guardada, a restaurar al terminar el escaneo
    bool reconstruyendo = false;
    int errores_seguidos = 0;
    double ultima_duracion = -1;
    double ultima_posicion = 0;           // última posición conocida (s), para guardarla
    Sesion sesion_pendiente;              // sesión anterior, a restaurar al terminar el primer escaneo
    bool sesion_programada = false;
    std::unique_ptr<Gtk::AboutDialog> acerca;
    Glib::RefPtr<Gio::DBus::Proxy> proxy_tema;  // portal de escritorio (preferencia claro/oscuro)
    bool tema_oscuro_inicial = false;
    gint64 ultimo_arrastre = 0;
    double volumen = 0.8;
    bool guardado_pendiente = false;
    std::string carpeta_actual;
    std::string url_caratula;
    std::string archivo_caratula_cache;
    int contador_caratula = 0;
    Glib::RefPtr<Gdk::Paintable> icono_generico;

    // Escaneo en segundo plano
    std::thread hilo;
    std::mutex mtx;
    std::vector<Cancion> resultado;
    Glib::Dispatcher dispatcher;
    bool escaneando = false;

    // Cabecera
    Gtk::HeaderBar cabecera;
    Gtk::Button b_carpeta, b_aleatorio;
    Gtk::ToggleButton b_buscar;
    Gtk::MenuButton b_menu;
    Gtk::ToggleButton b_cola{"Cola"};
    Gtk::Box caja_modos{Gtk::Orientation::HORIZONTAL};
    Gtk::ToggleButton t_canciones{"Canciones"}, t_artistas{"Artistas"}, t_albumes{"Álbumes"},
        t_generos{"Géneros"}, t_parati{"Para ti"};

    // Contenido
    Gtk::Box raiz{Gtk::Orientation::VERTICAL};
    Gtk::SearchBar barra_busqueda;
    Gtk::SearchEntry entrada_busqueda;
    Gtk::Box contenido{Gtk::Orientation::HORIZONTAL};
    Gtk::Paned panel{Gtk::Orientation::HORIZONTAL};
    Gtk::ScrolledWindow sc_grupos, sc_canciones;
    Gtk::Box caja_grupos{Gtk::Orientation::VERTICAL};
    Gtk::Button b_renovar;
    Gtk::ListBox lista_grupos, lista_canciones;

    // Panel de cola
    Gtk::Revealer rev_cola;
    Gtk::Box cola_externa{Gtk::Orientation::HORIZONTAL};
    Gtk::Separator sep_cola{Gtk::Orientation::VERTICAL};
    Gtk::Box caja_cola{Gtk::Orientation::VERTICAL};
    Gtk::Box cab_cola{Gtk::Orientation::HORIZONTAL};
    Gtk::Label l_cola_titulo, l_cola_vacia;
    Gtk::Button b_vaciar_cola;
    Gtk::ScrolledWindow sc_cola;
    Gtk::ListBox lista_cola;

    // Barra inferior
    Gtk::Box pie{Gtk::Orientation::HORIZONTAL};
    Gtk::Picture caratula;
    Gtk::Box barra{Gtk::Orientation::VERTICAL};
    Gtk::Box fila_botones{Gtk::Orientation::HORIZONTAL};
    Gtk::Button b_ant, b_play, b_sig, b_repetir;
    Gtk::Label l_titulo, l_pos, l_dur, l_estado;
    Gtk::Scale escala;
    Gtk::Image i_vol;
    Gtk::Scale esc_vol;
};
