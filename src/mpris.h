#pragma once
#include <giomm.h>
#include <sigc++/sigc++.h>

#include <functional>
#include <map>
#include <memory>
#include <string>

// Servidor MPRIS2 (D-Bus): permite controlar GC Music desde el applet de volumen,
// las teclas multimedia, playerctl, KDE Connect, etc.
class Mpris {
public:
    Mpris();
    ~Mpris();
    Mpris(const Mpris&) = delete;
    Mpris& operator=(const Mpris&) = delete;

    // Órdenes que llegan desde fuera
    sigc::signal<void()> al_play, al_pausa, al_alternar, al_siguiente, al_anterior, al_detener;
    sigc::signal<void(double)> al_volumen;  // 0.0 - 1.0
    sigc::signal<void(const std::string&)> al_repetir;  // "None", "Playlist" o "Track"

    // Posición actual en segundos (la rellena la ventana)
    std::function<double()> posicion;

    // Informar a los clientes del estado actual. estado: "Playing", "Paused" o "Stopped".
    void actualizar(const std::string& estado, int id_pista, const std::string& titulo,
                    const std::string& artista, const std::string& album, double duracion,
                    const std::string& url_caratula, bool hay_cola);
    void actualizar_volumen(double v);
    void actualizar_repetir(const std::string& modo);

private:
    using Mapa = std::map<Glib::ustring, Glib::VariantBase>;

    void al_obtener_bus(const Glib::RefPtr<Gio::DBus::Connection>& con, const Glib::ustring& nombre);
    void al_metodo(const Glib::RefPtr<Gio::DBus::Connection>& con, const Glib::ustring& emisor,
                   const Glib::ustring& ruta, const Glib::ustring& interfaz,
                   const Glib::ustring& metodo, const Glib::VariantContainerBase& params,
                   const Glib::RefPtr<Gio::DBus::MethodInvocation>& llamada);
    void al_propiedad(Glib::VariantBase& valor, const Glib::RefPtr<Gio::DBus::Connection>& con,
                      const Glib::ustring& emisor, const Glib::ustring& ruta,
                      const Glib::ustring& interfaz, const Glib::ustring& nombre);
    bool al_fijar_propiedad(const Glib::RefPtr<Gio::DBus::Connection>& con,
                            const Glib::ustring& emisor, const Glib::ustring& ruta,
                            const Glib::ustring& interfaz, const Glib::ustring& nombre,
                            const Glib::VariantBase& valor);
    Glib::Variant<Mapa> metadatos() const;
    void emitir(const Mapa& cambios);

    guint dueno = 0;
    Glib::RefPtr<Gio::DBus::Connection> conexion;
    Glib::RefPtr<Gio::DBus::NodeInfo> info;
    std::unique_ptr<Gio::DBus::InterfaceVTable> vtable;  // debe vivir tanto como el objeto

    std::string estado = "Stopped";
    int id_pista = -1;
    std::string titulo, artista, album, url_caratula;
    double duracion = -1;
    double volumen = 1.0;
    std::string repeticion = "Playlist";
    bool hay_cola = false;
};
