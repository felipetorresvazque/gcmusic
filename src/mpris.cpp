#include "mpris.h"

#include <typeinfo>
#include <vector>

#include "util.h"

namespace {

const char* RUTA = "/org/mpris/MediaPlayer2";
const char* IF_RAIZ = "org.mpris.MediaPlayer2";
const char* IF_PLAYER = "org.mpris.MediaPlayer2.Player";

const char* XML = R"(
<node>
  <interface name="org.mpris.MediaPlayer2">
    <method name="Raise"/>
    <method name="Quit"/>
    <property name="CanQuit" type="b" access="read"/>
    <property name="CanRaise" type="b" access="read"/>
    <property name="HasTrackList" type="b" access="read"/>
    <property name="Identity" type="s" access="read"/>
    <property name="DesktopEntry" type="s" access="read"/>
    <property name="SupportedUriSchemes" type="as" access="read"/>
    <property name="SupportedMimeTypes" type="as" access="read"/>
  </interface>
  <interface name="org.mpris.MediaPlayer2.Player">
    <method name="Next"/>
    <method name="Previous"/>
    <method name="Pause"/>
    <method name="PlayPause"/>
    <method name="Stop"/>
    <method name="Play"/>
    <method name="Seek"><arg direction="in" name="Offset" type="x"/></method>
    <method name="SetPosition">
      <arg direction="in" name="TrackId" type="o"/>
      <arg direction="in" name="Position" type="x"/>
    </method>
    <method name="OpenUri"><arg direction="in" name="Uri" type="s"/></method>
    <property name="PlaybackStatus" type="s" access="read"/>
    <property name="LoopStatus" type="s" access="readwrite"/>
    <property name="Rate" type="d" access="read"/>
    <property name="Shuffle" type="b" access="read"/>
    <property name="Metadata" type="a{sv}" access="read"/>
    <property name="Volume" type="d" access="readwrite"/>
    <property name="Position" type="x" access="read"/>
    <property name="MinimumRate" type="d" access="read"/>
    <property name="MaximumRate" type="d" access="read"/>
    <property name="CanGoNext" type="b" access="read"/>
    <property name="CanGoPrevious" type="b" access="read"/>
    <property name="CanPlay" type="b" access="read"/>
    <property name="CanPause" type="b" access="read"/>
    <property name="CanSeek" type="b" access="read"/>
    <property name="CanControl" type="b" access="read"/>
  </interface>
</node>
)";

using Texto = Glib::ustring;
}  // namespace

Mpris::Mpris() {
    info = Gio::DBus::NodeInfo::create_for_xml(XML);
    dueno = Gio::DBus::own_name(
        Gio::DBus::BusType::SESSION, "org.mpris.MediaPlayer2.gcmusic",
        sigc::mem_fun(*this, &Mpris::al_obtener_bus));
}

Mpris::~Mpris() {
    if (dueno) Gio::DBus::unown_name(dueno);
}

void Mpris::al_obtener_bus(const Glib::RefPtr<Gio::DBus::Connection>& con, const Glib::ustring&) {
    conexion = con;
    vtable = std::make_unique<Gio::DBus::InterfaceVTable>(
        sigc::mem_fun(*this, &Mpris::al_metodo), sigc::mem_fun(*this, &Mpris::al_propiedad),
        sigc::mem_fun(*this, &Mpris::al_fijar_propiedad));
    try {
        con->register_object(RUTA, info->lookup_interface(IF_RAIZ), *vtable);
        con->register_object(RUTA, info->lookup_interface(IF_PLAYER), *vtable);
    } catch (const Glib::Error& e) {
        g_warning("MPRIS: no se pudo registrar el objeto: %s", e.what());
    }
}

void Mpris::al_metodo(const Glib::RefPtr<Gio::DBus::Connection>&, const Glib::ustring&,
                      const Glib::ustring&, const Glib::ustring& interfaz,
                      const Glib::ustring& metodo, const Glib::VariantContainerBase&,
                      const Glib::RefPtr<Gio::DBus::MethodInvocation>& llamada) {
    if (interfaz == IF_PLAYER) {
        if (metodo == "Play") al_play.emit();
        else if (metodo == "Pause") al_pausa.emit();
        else if (metodo == "PlayPause") al_alternar.emit();
        else if (metodo == "Next") al_siguiente.emit();
        else if (metodo == "Previous") al_anterior.emit();
        else if (metodo == "Stop") al_detener.emit();
        // Seek, SetPosition y OpenUri: se aceptan pero no hacen nada (CanSeek = false)
    }
    // Raise y Quit: no hacen nada (CanRaise/CanQuit = false)
    llamada->return_value(Glib::VariantContainerBase());
}

Glib::Variant<Mpris::Mapa> Mpris::metadatos() const {
    Mapa m;
    if (id_pista >= 0) {
        m["mpris:trackid"] = Glib::Variant<Glib::DBusObjectPathString>::create(
            "/org/gcmusic/pista/" + std::to_string(id_pista));
        m["xesam:title"] = Glib::Variant<Texto>::create(utf8_seguro(titulo));
        m["xesam:artist"] =
            Glib::Variant<std::vector<Texto>>::create(std::vector<Texto>{utf8_seguro(artista)});
        m["xesam:album"] = Glib::Variant<Texto>::create(utf8_seguro(album));
        if (!url_caratula.empty())
            m["mpris:artUrl"] = Glib::Variant<Texto>::create(utf8_seguro(url_caratula));
        if (duracion > 0)
            m["mpris:length"] = Glib::Variant<gint64>::create(static_cast<gint64>(duracion * 1e6));
    } else {
        m["mpris:trackid"] = Glib::Variant<Glib::DBusObjectPathString>::create(
            "/org/mpris/MediaPlayer2/TrackList/NoTrack");
    }
    return Glib::Variant<Mapa>::create(m);
}

void Mpris::al_propiedad(Glib::VariantBase& valor, const Glib::RefPtr<Gio::DBus::Connection>&,
                         const Glib::ustring&, const Glib::ustring&, const Glib::ustring& interfaz,
                         const Glib::ustring& n) {
    if (interfaz == IF_RAIZ) {
        if (n == "CanQuit" || n == "CanRaise" || n == "HasTrackList")
            valor = Glib::Variant<bool>::create(false);
        else if (n == "Identity")
            valor = Glib::Variant<Texto>::create("GC Music");
        else if (n == "DesktopEntry")
            valor = Glib::Variant<Texto>::create("cl.pipe.gcmusic");
        else if (n == "SupportedUriSchemes")
            valor = Glib::Variant<std::vector<Texto>>::create(std::vector<Texto>{"file"});
        else if (n == "SupportedMimeTypes")
            valor = Glib::Variant<std::vector<Texto>>::create(std::vector<Texto>{
                "audio/mpeg", "audio/flac", "audio/ogg", "audio/x-wav", "audio/mp4"});
        return;
    }

    if (n == "PlaybackStatus") valor = Glib::Variant<Texto>::create(estado);
    else if (n == "LoopStatus") valor = Glib::Variant<Texto>::create(repeticion);
    else if (n == "Rate" || n == "MinimumRate" || n == "MaximumRate")
        valor = Glib::Variant<double>::create(1.0);
    else if (n == "Volume") valor = Glib::Variant<double>::create(volumen);
    else if (n == "Shuffle") valor = Glib::Variant<bool>::create(false);
    else if (n == "Metadata") valor = metadatos();
    else if (n == "Position") {
        const double s = posicion ? posicion() : 0;
        valor = Glib::Variant<gint64>::create(s > 0 ? static_cast<gint64>(s * 1e6) : 0);
    } else if (n == "CanGoNext" || n == "CanGoPrevious" || n == "CanPlay" || n == "CanPause")
        valor = Glib::Variant<bool>::create(hay_cola);
    else if (n == "CanSeek") valor = Glib::Variant<bool>::create(false);
    else if (n == "CanControl") valor = Glib::Variant<bool>::create(true);
}

bool Mpris::al_fijar_propiedad(const Glib::RefPtr<Gio::DBus::Connection>&, const Glib::ustring&,
                               const Glib::ustring&, const Glib::ustring& interfaz,
                               const Glib::ustring& nombre, const Glib::VariantBase& valor) {
    if (interfaz == IF_PLAYER && nombre == "Volume") {
        try {
            al_volumen.emit(Glib::VariantBase::cast_dynamic<Glib::Variant<double>>(valor).get());
            return true;
        } catch (const std::bad_cast&) {
            return false;
        }
    }
    if (interfaz == IF_PLAYER && nombre == "LoopStatus") {
        try {
            const Glib::ustring modo =
                Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(valor).get();
            if (modo != "None" && modo != "Playlist" && modo != "Track") return false;
            al_repetir.emit(modo);
            return true;
        } catch (const std::bad_cast&) {
            return false;
        }
    }
    return false;
}

void Mpris::emitir(const Mapa& cambios) {
    if (!conexion) return;
    try {
        const auto params = Glib::VariantContainerBase::create_tuple(std::vector<Glib::VariantBase>{
            Glib::Variant<Texto>::create(IF_PLAYER), Glib::Variant<Mapa>::create(cambios),
            Glib::Variant<std::vector<Texto>>::create(std::vector<Texto>{})});
        conexion->emit_signal(RUTA, "org.freedesktop.DBus.Properties", "PropertiesChanged", {},
                              params);
    } catch (const Glib::Error& e) {
        g_warning("MPRIS: no se pudo notificar el cambio: %s", e.what());
    }
}

void Mpris::actualizar_volumen(double v) {
    if (v == volumen) return;
    volumen = v;
    Mapa cambios;
    cambios["Volume"] = Glib::Variant<double>::create(volumen);
    emitir(cambios);
}

void Mpris::actualizar_repetir(const std::string& modo) {
    if (modo == repeticion) return;
    repeticion = modo;
    Mapa cambios;
    cambios["LoopStatus"] = Glib::Variant<Texto>::create(repeticion);
    emitir(cambios);
}

void Mpris::actualizar(const std::string& est, int id, const std::string& tit,
                       const std::string& art, const std::string& alb, double dur,
                       const std::string& url, bool cola) {
    estado = est;
    id_pista = id;
    titulo = tit;
    artista = art;
    album = alb;
    duracion = dur;
    url_caratula = url;
    hay_cola = cola;

    Mapa cambios;
    cambios["PlaybackStatus"] = Glib::Variant<Texto>::create(estado);
    cambios["Metadata"] = metadatos();
    cambios["CanGoNext"] = Glib::Variant<bool>::create(hay_cola);
    cambios["CanGoPrevious"] = Glib::Variant<bool>::create(hay_cola);
    cambios["CanPlay"] = Glib::Variant<bool>::create(hay_cola);
    cambios["CanPause"] = Glib::Variant<bool>::create(hay_cola);
    emitir(cambios);
}
