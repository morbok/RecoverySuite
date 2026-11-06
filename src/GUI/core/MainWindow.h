#pragma once
#include <QMainWindow>
#include <QPointer>
#include <QThread>
#include <QMetaType>
#include <atomic>
#include "Application/Service/GUIRecoveryService.h"
#include "StateManager.h"
class QStackedWidget;
class QLabel;
class QPushButton;
class QCloseEvent;
namespace recoverysuite::gui::widgets {
class SourceDiscoveryWidget;
class RecoveryConfigurationWidget;
class OperationProgressWidget;
class RecoveryResultsWidget;
}
namespace recoverysuite::gui::core {
/** Drive scan/review/recovery workflow. Service outlives window and workers. */
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(application::service::GUIRecoveryService* service, QWidget* parent = nullptr);
    ~MainWindow() override;
public slots:
    void rescueImageAs(const QString& path);
    void saveScanAs(const QString& path);
    void openSavedScan(const QString& path);
    void handleWorkflowOperation(const QString& operation);
    void onSourceSelected(const QString& path);
    void onConfigurationReady(const application::service::models::RecoveryOperation& operation);
    void updateOperationProgress(const QString& name, int percent, const QString& text);
    void handleOperationCompleted(const QString& name, bool success, const QString& message);
signals:
    void cancelRecovery();
private slots:
    void handleStartRecovery();
    void handleCancelOperation();
    void handleRecoveryProgress(const application::service::models::RecoveryProgress& progress);
    void handleRecoveryFinished(const application::service::models::RecoveryResult& result);
protected:
    void closeEvent(QCloseEvent* event) override;
private:
    void showSources();
    void updateControls();
    application::service::GUIRecoveryService* service_;
    application::service::models::RecoveryOperation operation_{};
    StateManager state_;
    QStackedWidget* pages_;
    widgets::SourceDiscoveryWidget* source_;
    widgets::RecoveryConfigurationWidget* configuration_;
    widgets::OperationProgressWidget* progress_;
    widgets::RecoveryResultsWidget* results_;
    QLabel* status_;
    QPushButton* chooseImage_;
    QPushButton* settings_;
    QPushButton* cancel_;
    QPointer<QThread> thread_;
    bool running_{};
    bool closeWhenFinished_{};
};
/** Executes scan/recovery on a worker thread. cancel() only sets an atomic flag. */
class RecoveryWorker : public QObject {
    Q_OBJECT
public:
    RecoveryWorker(application::service::GUIRecoveryService* service,
                   const application::service::models::RecoveryOperation& operation);
    RecoveryWorker(application::service::GUIRecoveryService* service, const std::string& path,
                   uint64_t start, uint64_t count, const std::string& output);
public slots:
    void process();
    void cancel();
signals:
    void progressUpdated(const application::service::models::RecoveryProgress& progress);
    void finished(const application::service::models::RecoveryResult& result);
private:
    application::service::GUIRecoveryService* service_;
    application::service::models::RecoveryOperation operation_;
    std::atomic_bool cancelled_{false};
};
}
Q_DECLARE_METATYPE(recoverysuite::application::service::models::RecoveryProgress)
Q_DECLARE_METATYPE(recoverysuite::application::service::models::RecoveryResult)
