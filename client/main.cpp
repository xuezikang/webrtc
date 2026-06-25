#include <QApplication>
#include <gst/gst.h>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    // Initialize GStreamer
    gst_init(&argc, &argv);

    QApplication app(argc, argv);
    MainWindow window;
    window.show();

    return app.exec();
}
