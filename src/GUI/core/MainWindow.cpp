#include "MainWindow.h"
#include "GUI/widgets/SourceDiscoveryWidget.h"
#include "GUI/widgets/RecoveryConfigurationWidget.h"
#include "GUI/widgets/OperationProgressWidget.h"
#include "GUI/widgets/RecoveryResultsWidget.h"
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QPushButton>
#include <QCloseEvent>
#include <QTimer>
#include <QFileDialog>
#include "Application/Service/recovery_session.h"
namespace recoverysuite::gui::core {
MainWindow::MainWindow(application::service::GUIRecoveryService* service, QWidget* parent)
    : QMainWindow(parent), service_(service) {
    qRegisterMetaType<application::service::models::RecoveryProgress>();
    qRegisterMetaType<application::service::models::RecoveryResult>();
    setWindowTitle(tr("RecoverySuite — Recover lost data")); resize(1100, 800); setMinimumSize(800, 600);
    setStyleSheet("QPushButton { padding: 7px 14px; } QLineEdit, QComboBox { padding: 5px; } QTreeWidget { alternate-background-color: palette(alternate-base); } QGroupBox { margin-top: 12px; padding-top: 12px; }");
    auto* file = menuBar()->addMenu(tr("File"));
    file->addAction(tr("Choose source drive…"), this, &MainWindow::showSources);
    file->addAction(tr("Resume session…"),this,[this] {
        if(running_)return;
        const auto path=QFileDialog::getOpenFileName(this,tr("Resume recovery session"),{},tr("Recovery session (*.json);;All files (*)"));if(path.isEmpty())return;
        try{const auto saved=application::service::RecoverySession::inspect(path.toStdString());service_->openImage(saved.operation.targetDevicePath,saved.sectorSize);onSourceSelected(QString::fromStdString(saved.operation.targetDevicePath));operation_=saved.operation;configuration_->setOperation(saved.operation);updateControls();}
        catch(const std::exception& failure){status_->setText(QString::fromUtf8(failure.what()));}
    });
    file->addAction(tr("Exit"), this, &QWidget::close);
    auto* central = new QWidget(this); auto* layout = new QVBoxLayout(central);
    auto* actions = new QHBoxLayout;
    chooseImage_ = new QPushButton(tr("Drives"), central); chooseImage_->setObjectName("chooseImage");
    settings_ = new QPushButton(tr("Scan options"), central); settings_->setObjectName("recoverySettings");
    cancel_ = new QPushButton(tr("Cancel"), central); cancel_->setObjectName("cancelImageRecovery");
    actions->addWidget(chooseImage_); actions->addWidget(settings_); actions->addStretch(); actions->addWidget(cancel_);
    layout->addLayout(actions);
    pages_ = new QStackedWidget(central);
    source_ = new widgets::SourceDiscoveryWidget(pages_); source_->setRecoveryService(service_);
    configuration_ = new widgets::RecoveryConfigurationWidget(pages_); configuration_->setRecoveryService(service_);
    progress_ = new widgets::OperationProgressWidget(pages_);
    results_ = new widgets::RecoveryResultsWidget(pages_);
    pages_->addWidget(source_); pages_->addWidget(configuration_); pages_->addWidget(progress_); pages_->addWidget(results_);
    layout->addWidget(pages_); setCentralWidget(central);
    status_ = new QLabel(tr("Choose the drive that contained your missing files"), this); status_->setTextFormat(Qt::PlainText);
    statusBar()->addWidget(status_, 1);
    connect(chooseImage_, &QPushButton::clicked, this, &MainWindow::showSources);
    connect(settings_, &QPushButton::clicked, this, [this] {
        if (!running_) { state_.transitionTo(ApplicationState::RECOVERY_CONFIGURATION); pages_->setCurrentWidget(configuration_); }
    });
    connect(cancel_, &QPushButton::clicked, this, &MainWindow::handleCancelOperation);
    connect(source_, &widgets::SourceDiscoveryWidget::sourceSelected, this, &MainWindow::onSourceSelected);
    connect(configuration_, &widgets::RecoveryConfigurationWidget::configurationReady, this, &MainWindow::onConfigurationReady);
    connect(results_, &widgets::RecoveryResultsWidget::recoverSelection, this, [this](const QStringList& names,const QString& folder) {
        if(running_)return;
        operation_.operationType="disk_recovery";operation_.outputPath=folder.toStdString();operation_.verifyAfterRecovery=true;
        operation_.sessionPath.clear();operation_.resumeSession=false;operation_.selectedPaths.clear();
        for(const auto& name:names)operation_.selectedPaths.push_back(name.toStdString());
        state_.transitionTo(ApplicationState::RECOVERY_CONFIGURATION);handleStartRecovery();
    });
    connect(results_, &widgets::RecoveryResultsWidget::previewRequested, this, [this](const QString& name) {
        if(running_)return;
        operation_.operationType="disk_preview";operation_.selectedPaths={name.toStdString()};
        state_.transitionTo(ApplicationState::RECOVERY_CONFIGURATION);handleStartRecovery();
    });
    state_.transitionTo(ApplicationState::SOURCE_SELECTION); updateControls();
}
MainWindow::~MainWindow() {
    if (thread_ && thread_->isRunning()) { emit cancelRecovery(); thread_->quit(); thread_->wait(); }
}
void MainWindow::showSources() {
    if (running_) { status_->setText(tr("Cancel or wait for recovery before changing the source")); return; }
    state_.forceTransitionTo(ApplicationState::SOURCE_SELECTION);
    pages_->setCurrentWidget(source_); updateControls();
}
void MainWindow::updateControls() {
    chooseImage_->setEnabled(!running_); settings_->setEnabled(!running_ && !operation_.targetDevicePath.empty());
    source_->setEnabled(!running_); configuration_->setEnabled(!running_); results_->setEnabled(!running_);
    cancel_->setEnabled(running_ && !closeWhenFinished_);
}
void MainWindow::onSourceSelected(const QString& path) {
    if (running_) return;
    operation_ = {}; operation_.targetDevicePath = path.toStdString();
    configuration_->setSelectedDevice(path);
    state_.transitionTo(ApplicationState::RECOVERY_CONFIGURATION);
    pages_->setCurrentWidget(configuration_); results_->clear();
    status_->setText(tr("Source opened read-only. Start an automatic scan to find lost data.")); updateControls();
}
void MainWindow::onConfigurationReady(const application::service::models::RecoveryOperation& operation) {
    if (running_) return;
    operation_ = operation; handleStartRecovery();
}
void MainWindow::handleStartRecovery() {
    if (running_ || !service_) return;
    const bool preview=operation_.operationType=="disk_preview";
    const auto error = preview ? std::string{} : service_->validateOperation(operation_);
    if (!error.empty()) { status_->setText(QString::fromStdString(error)); return; }
    if (!state_.transitionTo(ApplicationState::VALIDATION) || !state_.transitionTo(ApplicationState::RECOVERY)) {
        status_->setText(tr("Return to recovery settings before starting another scan")); return;
    }
    running_ = true; updateControls();
    if(!preview) { results_->clear(); progress_->resetProgress();pages_->setCurrentWidget(progress_); }
    status_->setText(preview ? tr("Reading file preview…") : tr("Scanning source…"));
    thread_ = new QThread(this);
    auto* worker = new RecoveryWorker(service_, operation_); worker->moveToThread(thread_);
    connect(thread_, &QThread::started, worker, &RecoveryWorker::process);
    connect(this, &MainWindow::cancelRecovery, worker, &RecoveryWorker::cancel, Qt::DirectConnection);
    connect(worker, &RecoveryWorker::progressUpdated, this, &MainWindow::handleRecoveryProgress);
    connect(worker, &RecoveryWorker::finished, this, &MainWindow::handleRecoveryFinished);
    connect(worker, &RecoveryWorker::finished, thread_, &QThread::quit, Qt::DirectConnection);
    connect(thread_, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread_, &QThread::finished, this, [this] {
        running_ = false; updateControls();
        if (closeWhenFinished_) QTimer::singleShot(0, this, &QWidget::close);
    });
    connect(thread_, &QThread::finished, thread_, &QObject::deleteLater);
    thread_->start();
}
void MainWindow::handleCancelOperation() {
    if (!running_) return;
    emit cancelRecovery(); cancel_->setEnabled(false);
    status_->setText(tr("Cancelling; completed output files will be kept…"));
}
void MainWindow::handleRecoveryProgress(const application::service::models::RecoveryProgress& progress) {
    progress_->updateProgress(QString::fromStdString(progress.operationType), progress.percentage,
        QString::fromStdString(progress.currentStep), progress.processedBytes, progress.totalBytes,
        progress.elapsedSeconds, progress.recoveredItemsCount, progress.failedItemsCount, progress.isCancellable);
    status_->setText(QString::fromStdString(progress.currentStep));
}
void MainWindow::handleRecoveryFinished(const application::service::models::RecoveryResult& result) {
    if(result.operationType=="disk_preview") {
        results_->setPreview(result);state_.transitionTo(ApplicationState::RESULTS);
        status_->setText(result.success ? tr("Preview ready; no files saved") : QString::fromStdString(result.errorMessage));return;
    }
    results_->setRecoveryResult(result); state_.transitionTo(ApplicationState::RESULTS);
    pages_->setCurrentWidget(results_);
    status_->setText(result.scanOnly ? tr("Scan finished. Select files and a destination on another drive.") : result.cancelled ? tr("Cancelled; review saved items below") : (result.success ? tr("Scan complete") : tr("Scan finished with errors; review results")));
}
void MainWindow::handleWorkflowOperation(const QString& operation) {
    if (operation == "list_sources") showSources();
    else if (operation == "cancel_operation") handleCancelOperation();
    else if (operation == "start_recovery" || operation == "validate_recovery") configuration_->validateConfiguration();
    else status_->setText(tr("Choose a drive, scan for lost data, then select files to recover"));
}
void MainWindow::updateOperationProgress(const QString& name, int percent, const QString& text) { progress_->updateProgress(name, percent, text); }
void MainWindow::handleOperationCompleted(const QString&, bool, const QString& message) { status_->setText(message); }
void MainWindow::closeEvent(QCloseEvent* event) {
    if (running_) { closeWhenFinished_ = true; handleCancelOperation(); updateControls(); event->ignore(); }
    else event->accept();
}
RecoveryWorker::RecoveryWorker(application::service::GUIRecoveryService* service,
    const application::service::models::RecoveryOperation& operation) : service_(service), operation_(operation) {}
RecoveryWorker::RecoveryWorker(application::service::GUIRecoveryService* service, const std::string& path,
    uint64_t start, uint64_t count, const std::string& output) : service_(service) {
    operation_.operationType = "carving"; operation_.targetDevicePath = path;
    operation_.startSector = start; operation_.numSectors = count; operation_.outputPath = output;
}
void RecoveryWorker::cancel() { cancelled_ = true; }
void RecoveryWorker::process() {
    application::service::models::RecoveryResult result{};
    try {
        if (!service_) throw std::runtime_error("Recovery service is unavailable");
        if(operation_.operationType=="disk_preview") {
            result.operationType="disk_preview";
            if(operation_.selectedPaths.size()!=1)throw std::runtime_error("Choose one file to inspect");
            result.previewBytes=service_->previewCandidate(operation_.selectedPaths.front(),[this]{return cancelled_.load();});
            result.success=true;
        } else result = service_->executeOperation(operation_, [this](const auto& progress) { emit progressUpdated(progress); }, [this] { return cancelled_.load(); });
    } catch (const std::exception& failure) { result.errorMessage = failure.what(); result.cancelled = cancelled_.load(); }
    emit finished(result);
}
}
