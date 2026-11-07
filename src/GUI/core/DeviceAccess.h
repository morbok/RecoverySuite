#pragma once
#include <QString>
#ifdef __linux__
#include <QDBusConnection>
#include <QDBusUnixFileDescriptor>
namespace recoverysuite::gui::core {
QString udisksObjectPath(const QString &device);
QDBusUnixFileDescriptor authorizeReadOnlyDevice(
    const QString &device,
    const QDBusConnection &connection = QDBusConnection::systemBus());
} // namespace recoverysuite::gui::core
#endif
#ifdef _WIN32
namespace recoverysuite::gui::core {
void requestAdministratorWindow();
}
#endif
