#pragma once
#include <QWidget>
#include "Application/Service/GUIRecoveryService.h"
class QLineEdit;
class QComboBox;
class QLabel;
namespace recoverysuite::gui::widgets {
/** Opens real read-only disk images; no simulated hardware inventory is shown. */
class SourceDiscoveryWidget : public QWidget {
    Q_OBJECT
public:
    explicit SourceDiscoveryWidget(QWidget* parent = nullptr);
    void setRecoveryService(application::service::GUIRecoveryService* service);
public slots:
    void onRefreshClicked();
    void openImagePath(const QString& path, uint32_t sectorSize = 512);
signals:
    void sourceSelected(const QString& path);
private:
    application::service::GUIRecoveryService* service_{};
    QLineEdit* path_;
    QComboBox* sectorSize_;
    QLabel* status_;
};
}
