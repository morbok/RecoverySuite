#ifndef RECOVERYSUITE_GUI_CORE_MAINWINDOW_H
#define RECOVERYSUITE_GUI_CORE_MAINWINDOW_H

#include <QMainWindow>
#include <QMenuBar>
#include <QStatusBar>
#include <QWidget>
#include <QVBoxLayout>

#include "../widgets/RecoveryWorkflowWidget.h"
#include "../widgets/OperationProgressWidget.h"

namespace recoverysuite {
namespace gui {
namespace core {

/**
 * MainWindow
 *
 * Main application window that provides the primary user interface
 * for RecoverySuite. Implements a modern forensic-grade interface
 * with dockable panels, workflow controls, and progress monitoring.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /**
     * Constructor
     */
    explicit MainWindow(QWidget *parent = nullptr);

    /**
     * Destructor
     */
    ~MainWindow() override;

public slots:
    /**
     * Handle workflow operation requests from UI widgets
     * @param operationType Type of recovery operation requested
     */
    void handleWorkflowOperation(const QString& operationType);

    /**
     * Update operation progress display
     * @param operationName Name of the operation
     * @param progress Percentage progress (0-100)
     * @param statusText Detailed status message
     */
    void updateOperationProgress(const QString& operationName, int progress, const QString& statusText);

    /**
     * Handle operation completion
     * @param operationName Name of the completed operation
     * @param success Whether operation was successful
     * @param resultMessage Result or error message
     */
    void handleOperationCompleted(const QString& operationName, bool success, const QString& resultMessage);

private:
    /**
     * Setup the user interface components
     */
    void setupUI();

    /**
     * Create menu bar and menu items
     */
    void setupMenuBar();

    /**
     * Create status bar
     */
    void setupStatusBar();

    /**
     * Setup central widget area with workflow and progress widgets
     */
    void setupCentralWidget();

    /**
     * Connect signals and slots
     */
    void setupConnections();

private:
    // UI Components
    QMenuBar* menuBar_;
    QStatusBar* statusBar_;
    QWidget* centralWidget_;
    QVBoxLayout* mainLayout_;

    // Custom Widgets
    RecoveryWorkflowWidget* workflowWidget_;
    OperationProgressWidget* progressWidget_;

    // Application State
    bool isOperationRunning_;
};

} // namespace core
} // namespace gui
} // namespace recoverysuite

#endif // RECOVERYSUITE_GUI_CORE_MAINWINDOW_H