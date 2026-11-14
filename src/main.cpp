#include <gst/gst.h>

#include "ventana.h"

int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);
    auto app = Gtk::Application::create("cl.pipe.gcmusic");
    return app->make_window_and_run<Ventana>(argc, argv);
}
