#pragma once
#include <QWidget>
#include "Application/Service/GUIRecoveryService.h"
class QLabel;
class QLineEdit;
class QCheckBox;
class QComboBox;
namespace recoverysuite::gui::widgets {
/** Configures the supported image-carving operation and validates it through the service. */
class RecoveryConfigurationWidget : public QWidget {
    Q_OBJECT
public:
    explicit RecoveryConfigurationWidget(QWidget* parent = nullptr);
    void setRecoveryService(application::service::GUIRecoveryService* service);
    void setOperation(const application::service::models::RecoveryOperation& operation);
    void setSelectedDevice(const QString& path);
    application::service::models::RecoveryOperation getRecoveryOperation() const;
public slots:
    void validateConfiguration();
signals:
    void configurationReady(const application::service::models::RecoveryOperation& operation);
private:
    application::service::GUIRecoveryService* service_{};
    QString source_;
    QLabel* sourceLabel_;
    QLabel* status_;
    QLineEdit* start_;
    QLineEdit* count_;
    QLineEdit* output_;
    QLineEdit* session_;
    bool resumeSession_{};
    QCheckBox* verify_;
    QComboBox* method_;
};
}
