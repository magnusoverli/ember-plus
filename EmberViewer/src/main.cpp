/*
 * EmberViewer - Cross-platform Ember+ Protocol Viewer
 * Copyright (c) 2025 Magnus Overli
 * 
 * This application provides a modern, cross-platform GUI for browsing
 * and controlling Ember+ providers.
 */

#include <QApplication>
#include "MainWindow.h"
#include "Logger.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Set application metadata
    QApplication::setApplicationName("EmberViewer");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("Magnus Overli");
    QApplication::setOrganizationDomain("github.com/magnusoverli");
    
    // Initialize structured logging
    Logger::instance()->initialize();
    Logger::instance()->info("application", "EmberViewer starting");
    
    // Create and show main window
    MainWindow window;
    window.show();
    
    Logger::instance()->info("application", "Main window shown");
    
    // Run application
    int result = app.exec();
    
    // Shutdown logging
    Logger::instance()->info("application", "EmberViewer shutting down");
    Logger::instance()->shutdown();
    
    return result;
}

