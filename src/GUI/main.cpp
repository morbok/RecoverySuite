#include "core/Application.h"
#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QIcon>
#include <QLoggingCategory>
#include "Logging/Logger.h"

int main(int argc, char *argv[])
{
    // Initialize logging
    recoverysuite::logging::Logger::instance().info("Starting RecoverySuite GUI application");

    // Enable high DPI scaling
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);

    // Set application metadata
    QApplication::setApplicationName("RecoverySuite");
    QApplication::setApplicationDisplayName("RecoverySuite Enterprise");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("RecoverySuite Team");
    QApplication::setOrganizationDomain("recoverysuite.example.com");

    // Set application icon (if available)
    // QIcon appIcon(":/icons/recoverysuite.png");
    // app.setWindowIcon(appIcon);

    // Initialize translator
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "RecoverySuite_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            app.installTranslator(&translator);
            break;
        }
    }

    // Create and show the main application window
    recoverysuite::gui::core::Application application;
    int result = application.run();

    recoverysuite::logging::Logger::instance().info("RecoverySuite GUI application exited with code: " + std::to_string(result));
    return result;
}