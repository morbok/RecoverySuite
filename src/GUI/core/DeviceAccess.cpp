#include "DeviceAccess.h"
#include <stdexcept>
#ifdef __linux__
#include <QDBusMessage>
#include <QDBusReply>
#include <QFileInfo>
#include <QVariantMap>
#include <fcntl.h>
#include <sys/stat.h>
namespace recoverysuite::gui::core {
QString udisksObjectPath(const QString &device) {
  const QFileInfo file(device);
  const auto canonical = file.canonicalFilePath();
  struct stat info{};
  if (canonical.isEmpty() || !canonical.startsWith("/dev/") ||
      ::lstat(canonical.toUtf8().constData(), &info) || !S_ISBLK(info.st_mode))
    throw std::runtime_error(
        "Choose an existing block device before requesting access");
  const auto name = QFileInfo(canonical).fileName().toUtf8();
  QString encoded;
  for (unsigned char c : name) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9'))
      encoded += QChar(c);
    else
      encoded += "_" + QString::number(c, 16).rightJustified(2, '0');
  }
  return "/org/freedesktop/UDisks2/block_devices/" + encoded;
}
QDBusUnixFileDescriptor
authorizeReadOnlyDevice(const QString &device,
                        const QDBusConnection &connection) {
  auto message = QDBusMessage::createMethodCall(
      "org.freedesktop.UDisks2", udisksObjectPath(device),
      "org.freedesktop.UDisks2.Block", "OpenDevice");
  message.setInteractiveAuthorizationAllowed(true);
  // No writable mode, unmount, format, repair, or permission changes are
  // offered.
  message << QString("r")
          << QVariantMap{{"flags", O_EXCL | O_CLOEXEC | O_NONBLOCK}};
  QDBusReply<QDBusUnixFileDescriptor> reply =
      connection.call(message, QDBus::BlockWithGui, 120000);
  if (!reply.isValid() || !reply.value().isValid())
    throw std::runtime_error(
        ("Read-only access was not authorized. UDisks2 and a desktop "
         "authentication agent are required. " +
         reply.error().message())
            .toStdString());
  return reply.value();
}
} // namespace recoverysuite::gui::core
#endif
#ifdef _WIN32
#include <QCoreApplication>
#include <shellapi.h>
#include <windows.h>
namespace recoverysuite::gui::core {
void requestAdministratorWindow() {
  const auto executable =
      QCoreApplication::applicationFilePath().toStdWString();
  const auto result = ShellExecuteW(nullptr, L"runas", executable.c_str(),
                                    nullptr, nullptr, SW_SHOWNORMAL);
  if (reinterpret_cast<INT_PTR>(result) <= 32)
    throw std::runtime_error(
        "Administrator access was cancelled or the new window could not start");
}
} // namespace recoverysuite::gui::core
#endif
