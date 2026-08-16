#ifndef RECOVERYSUITE_GUI_CORE_MAINWINDOW_H
#define RECOVERYSUITE_GUI_CORE_MAINWINDOW_H

#include <QMainWindow>
#include <QMenuBar>
#include <QStatusBar>
#include <QWidget>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <memory>
#include "../../Application/Service/GUIRecoveryService.h"
#include "ApplicationState.h"
#include "StateManager.h"
#include "widgets/RecoveryWorkflowWidget.h"
#include "widgets/OperationProgressWidget.h"
#include "widgets/SourceDiscoveryWidget.h"

namespace recoverysuite {
namespace gui {
namespace core {

/**
 * @brief MainWindow
 *
 * Main application window that provides the primary user interface
 * for RecoverySuite. Implements a modern forensic-grade interface
 * with dockable panels, workflow controls, and progress monitoring.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param recoveryService Pointer to the GUI recovery service
     */
    explicit MainWindow(recoverysuite::application::service::GUIRecoveryService* recoveryService, QWidget *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~MainWindow() override;

public slots:
    /**
     * @brief Handle workflow operation requests from UI widgets
     * @param operationType Type of recovery operation requested
     */
    void handleWorkflowOperation(const QString& operationType);

    /**
     * @brief Update operation progress display
     * @param operationName Name of the current operation
     * @param progress Percentage progress (0-100)
     * @param statusText Detailed status message
     */
    void updateOperationProgress(const QString& operationName, int progress, const QString& statusText);

    /**
     * @brief Handle operation completion
     * @param operationName Name of the completed operation
     * @param success Whether operation was successful
     * @param resultMessage Result or error message
     */
    void handleOperationCompleted(const QString& operationName, bool success, const QString& resultMessage);

    /**
     * @brief Handle source selection from source discovery widget
     * @param devicePath The path of the selected device
     */
    void onSourceSelected(const QString& devicePath);

private slots:
    /**
     * @brief Handle list storage sources operation
     */
    void handleListStorageSources();

    /**
     * @brief Handle inspect partitions operation
     */
    void handleInspectPartitions();

    /**
     * @brief Handle detect filesystem operation
     */
    void handleDetectFilesystem();

    /**
     * @brief Handle analyze filesystem operation
     */
    void handleAnalyzeFilesystem();

    /**
     * @brief Handle validate recovery operation
     */
    void handleValidateRecovery();

    /**
     * @brief Handle start recovery operation
     */
    void handleStartRecovery();

    /**
     * @brief Handle cancel operation
     */
    void handleCancelOperation();

    /**
     * @brief Update progress from service callbacks
     * @param operationName Name of the operation
     * @param progress Percentage progress (0-100)
     * @param statusText Detailed status message
     */
    void updateProgressFromService(const QString& operationName, int progress, const QString& statusText);

private:
    /**
     * @brief Setup the user interface components
     */
    void setupUI();

    /**
     * @brief Create menu bar and menu items
     */
    void setupMenuBar();

    /**
     * @brief Create status bar
     */
    void setupStatusBar();

    /**
     * @brief Setup central widget area with stacked widget for different views
     */
    void setupCentralWidget();

    /**
     * @brief Setup the workflow container widget (contains workflow and progress widgets)
     */
    void setupWorkflowContainer();

    /**
     * @brief Connect signals and slots
     */
    void setupConnections();

    /**
     * @brief Update the UI based on the current application state
     */
    void updateUIForState();

private:
    // UI Components
    QMenuBar* menuBar_;
    QStatusBar* statusBar_;
    QStackedWidget* stackedWidget_;
    QLabel* statusLabel_;
    QLabel* versionLabel_;

    // Custom Widgets
    recoverysuite::gui::widgets::SourceDiscoveryWidget* sourceDiscoveryWidget_;
    recoverysuite::gui::widgets::RecoveryWorkflowWidget* workflowWidget_;
    recoverysuite::gui::widgets::OperationProgressWidget* progressWidget_;
    QWidget* workflowContainer_;

    // Application State Management
    recoverysuite::gui::core::StateManager stateManager_;
    bool isOperationRunning_;

    // Services
    recoverysuite::application::service::GUIRecoveryService* recoveryService_;

    // Selected device path
    QString selectedDevice_;
};

} // namespace core
} // namespace gui
} // namespace recoverysuite

#endif // RECOVERYSUITE_GUI_CORE_MAINWINDOW_H