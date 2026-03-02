#include "mainwindow.h"
#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Set application metadata
    app.setApplicationName("Rebear GUI");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Rebear Project");
    
    // Use Fusion style for consistent cross-platform appearance
    app.setStyle(QStyleFactory::create("Fusion"));
    
    // Create and show main window
    MainWindow window;
    window.show();
    
    return app.exec();
}
