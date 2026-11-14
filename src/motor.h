#pragma once
#include <gst/gst.h>
#include <sigc++/sigc++.h>

#include <string>

// Envoltorio simple de GStreamer (playbin) para reproducir archivos.
class Motor {
public:
    Motor();
    ~Motor();
    Motor(const Motor&) = delete;
    Motor& operator=(const Motor&) = delete;

    void reproducir(const std::string& ruta);
    void preparar(const std::string& ruta, double desde);  // carga en pausa y salta a "desde" (s)
    void pausar();
    void reanudar();
    void detener();
    bool reproduciendo() const { return sonando; }

    double posicion() const;   // segundos, -1 si no se sabe
    double duracion() const;   // segundos, -1 si no se sabe
    void buscar(double segundos);
    void volumen(double v);  // 0.0 - 1.0 (curva cúbica, como PulseAudio)

    sigc::signal<void()> al_terminar;
    sigc::signal<void(const std::string&)> al_error;

private:
    static gboolean al_mensaje(GstBus* bus, GstMessage* msg, gpointer datos);

    GstElement* playbin = nullptr;
    bool sonando = false;
    double pendiente = 0;  // posición a la que saltar cuando termine de cargar
};
