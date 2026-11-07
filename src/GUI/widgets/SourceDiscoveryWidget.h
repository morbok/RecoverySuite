#pragma once
#include <QWidget>
#include "Application/Service/GUIRecoveryService.h"
class QLineEdit;
class QComboBox;
class QLabel;
class QTreeWidget;
namespace recoverysuite::gui::widgets {
/** Selects connected drives; disk images are an optional secondary source. */
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
    QTreeWidget* drives_;
    QLineEdit* path_;
    QComboBox* sectorSize_;
    QLabel* status_;
};
}
