#include "motor.h"

#include <stdexcept>

Motor::Motor() {
    playbin = gst_element_factory_make("playbin", "reproductor");
    if (!playbin)
        throw std::runtime_error("No se pudo crear 'playbin'. Instala gstreamer1.0-plugins-base.");

    // Opcional: GCMUSIC_AUDIO_SINK="fakesink sync=true" usa otra salida (útil sin tarjeta de sonido)
    if (const char* sink = g_getenv("GCMUSIC_AUDIO_SINK")) {
        if (GstElement* alternativa = gst_parse_bin_from_description(sink, TRUE, nullptr))
            g_object_set(playbin, "audio-sink", alternativa, nullptr);
    }

    GstBus* bus = gst_element_get_bus(playbin);
    gst_bus_add_watch(bus, &Motor::al_mensaje, this);
    gst_object_unref(bus);
}

Motor::~Motor() {
    gst_element_set_state(playbin, GST_STATE_NULL);
    GstBus* bus = gst_element_get_bus(playbin);
    gst_bus_remove_watch(bus);
    gst_object_unref(bus);
    gst_object_unref(playbin);
}

void Motor::reproducir(const std::string& ruta) {
    gst_element_set_state(playbin, GST_STATE_NULL);
    gchar* uri = g_filename_to_uri(ruta.c_str(), nullptr, nullptr);
    if (!uri) {
        al_error.emit("Ruta no válida: " + ruta);
        return;
    }
    g_object_set(playbin, "uri", uri, nullptr);
    g_free(uri);
    pendiente = 0;
    gst_element_set_state(playbin, GST_STATE_PLAYING);
    sonando = true;
}

void Motor::preparar(const std::string& ruta, double desde) {
    gst_element_set_state(playbin, GST_STATE_NULL);
    gchar* uri = g_filename_to_uri(ruta.c_str(), nullptr, nullptr);
    if (!uri) return;
    g_object_set(playbin, "uri", uri, nullptr);
    g_free(uri);
    pendiente = desde;  // el salto se hace al terminar de cargar (ASYNC_DONE)
    gst_element_set_state(playbin, GST_STATE_PAUSED);
    sonando = false;
}

void Motor::pausar() {
    gst_element_set_state(playbin, GST_STATE_PAUSED);
    sonando = false;
}

void Motor::reanudar() {
    gst_element_set_state(playbin, GST_STATE_PLAYING);
    sonando = true;
}

void Motor::detener() {
    gst_element_set_state(playbin, GST_STATE_NULL);
    sonando = false;
}

void Motor::volumen(double v) {
    if (v < 0) v = 0;
    if (v > 1) v = 1;
    g_object_set(playbin, "volume", v * v * v, nullptr);
}

double Motor::posicion() const {
    gint64 ns = 0;
    if (!gst_element_query_position(playbin, GST_FORMAT_TIME, &ns)) return -1;
    return static_cast<double>(ns) / GST_SECOND;
}

double Motor::duracion() const {
    gint64 ns = 0;
    if (!gst_element_query_duration(playbin, GST_FORMAT_TIME, &ns)) return -1;
    return static_cast<double>(ns) / GST_SECOND;
}

void Motor::buscar(double segundos) {
    gst_element_seek_simple(playbin, GST_FORMAT_TIME,
                            static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                            static_cast<gint64>(segundos * GST_SECOND));
}

gboolean Motor::al_mensaje(GstBus*, GstMessage* msg, gpointer datos) {
    auto* self = static_cast<Motor*>(datos);
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ASYNC_DONE:
        if (self->pendiente > 0) {
            const double destino = self->pendiente;
            self->pendiente = 0;
            self->buscar(destino);
        }
        break;
    case GST_MESSAGE_EOS:
        self->sonando = false;
        self->al_terminar.emit();
        break;
    case GST_MESSAGE_ERROR: {
        GError* err = nullptr;
        gchar* depuracion = nullptr;
        gst_message_parse_error(msg, &err, &depuracion);
        self->sonando = false;
        self->al_error.emit(err ? err->message : "Error desconocido");
        if (err) g_error_free(err);
        g_free(depuracion);
        break;
    }
    default:
        break;
    }
    return TRUE;
}
