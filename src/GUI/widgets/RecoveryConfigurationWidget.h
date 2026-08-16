#ifndef RECOVERYSUITE_GUI_WIDGETS_RECOVERYCONFIGURATIONWIDGET_H
#define RECOVERYSUITE_GUI_WIDGETS_RECOVERYCONFIGURATIONWIDGET_H

#include <QWidget>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTextEdit>
#include <QMessageBox>

#include "../../Application/Service/GUIRecoveryService.h"
#include "../../Application/Service/Models/GUIModels.h"

namespace recoverysuite {
namespace gui {
namespace widgets {

/**
 * RecoveryConfigurationWidget
 *
 * Widget for configuring recovery operations based on backend capabilities.
 * Exposes only capabilities actually registered and supported by the backend.
 */
class RecoveryConfigurationWidget : public QWidget {
    Q_OBJECT

public:
    explicit RecoveryConfigurationWidget(QWidget *parent = nullptr);
    ~RecoveryConfigurationWidget() override;

    /**
     * @brief Set the GUI recovery service
     * @param service Pointer to the GUI recovery service
     */
    void setRecoveryService(recoverysuite::application::service::GUIRecoveryService* service);

    /**
     * @brief Set the selected device path
     * @param devicePath Path to the selected storage device
     */
    void setSelectedDevice(const QString& devicePath);

    /**
     * @brief Get the current recovery operation configuration
     * @return RecoveryOperation model with current configuration
     */
    recoverysuite::application::service::models::RecoveryOperation getRecoveryOperation() const;

signals:
    /**
     * Emitted when the configuration is validated and ready for execution
     * @param operation The configured recovery operation
     */
    void configurationReady(const recoverysuite::application::service::models::RecoveryOperation& operation);

private slots:
    void onCapabilityChanged(int index);
    void onDetectCapabilities();
    void onValidateConfiguration();
    void onConfigureRecovery();

private:
    // UI Components
    QGroupBox* configurationGroupBox_;
    QVBoxLayout* configurationLayout_;
    QFormLayout* formLayout_;

    QLabel* deviceLabel_;
    QComboBox* capabilityComboBox_;
    QLineEdit* startSectorEdit_;
    QLineEdit* numSectorsEdit_;
    QLineEdit* outputPathEdit_;
    QCheckBox* verifyAfterRecoveryCheckBox_;
    QCheckBox* createLogFileCheckBox_;
    QPushButton* browseOutputButton_;
    QPushButton* validateConfigurationButton_;
    QPushButton* configureRecoveryButton_;
    QTextEdit* statusTextEdit_;

    // Service and data
    recoverysuite::application::service::GUIRecoveryService* recoveryService_;
    QString selectedDevicePath_;
    recoverysuite::recovery::RecoveryCapability registeredCapabilities_;

    // Private methods
    void setupUI();
    void setupConnections();
    void updateCapabilityList();
    void updateStatus(const QString& message);
    bool validateInputs();
};

} // namespace widgets
} // namespace gui
} // namespace recoverysuite

#endif // RECOVERYSUITE_GUI_WIDGETS_RECOVERYCONFIGURATIONWIDGET_H