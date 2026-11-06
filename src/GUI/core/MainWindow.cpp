#include "MainWindow.h"
#include "GUI/widgets/SourceDiscoveryWidget.h"
#include "GUI/widgets/RecoveryConfigurationWidget.h"
#include "GUI/widgets/OperationProgressWidget.h"
#include "GUI/widgets/RecoveryResultsWidget.h"
#include <QStackedWidget>
#include <QBuffer>
#include <QImageReader>
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
    file->addAction(tr("Rescue drive to image (logs unreadable sectors)…"),this,[this] {
        if(running_)return;
        const auto path=QFileDialog::getSaveFileName(this,tr("Rescue to another drive; unreadable sectors will be zero-filled and logged"),{},tr("Rescue image (*.img)"));
        if(!path.isEmpty())rescueImageAs(path);
    });
    file->addAction(tr("Continue interrupted rescue…"),this,[this] {
        if(running_)return;
        const auto path=QFileDialog::getOpenFileName(this,tr("Continue rescue for the selected source"),{},tr("Rescue report (*.rescue.json)"));
        if(path.endsWith(".rescue.json"))rescueImageAs(path.left(path.size()-12),true);
    });
    file->addAction(tr("Save scan results…"),this,[this] {
        if(running_)return;
        const auto path=QFileDialog::getSaveFileName(this,tr("Save scan on another drive"),{},tr("Saved drive scan (*.rscan)"));
        if(!path.isEmpty())saveScanAs(path);
    });
    file->addAction(tr("Open saved scan…"),this,[this] {
        if(running_)return;
        const auto path=QFileDialog::getOpenFileName(this,tr("Open saved drive scan"),{},tr("Saved drive scan (*.rscan);;All files (*)"));
        if(!path.isEmpty())openSavedScan(path);
    });
    file->addAction(tr("Continue interrupted scan"),this,[this] {
        if(running_)return;
        operation_.operationType="disk_scan";operation_.resumeScan=true;
        state_.forceTransitionTo(ApplicationState::RECOVERY_CONFIGURATION);handleStartRecovery();
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
void MainWindow::rescueImageAs(const QString& path, bool resume) {
    if(running_||path.isEmpty())return;
    operation_.operationType="disk_rescue";operation_.outputPath=path.toStdString();operation_.resumeRescue=resume;
    state_.forceTransitionTo(ApplicationState::RECOVERY_CONFIGURATION);handleStartRecovery();
}
void MainWindow::saveScanAs(const QString& path) {
    if(running_||path.isEmpty())return;
    operation_.operationType="disk_save_scan";operation_.sessionPath=path.toStdString();
    state_.forceTransitionTo(ApplicationState::RECOVERY_CONFIGURATION);handleStartRecovery();
}
void MainWindow::openSavedScan(const QString& path) {
    if(running_||path.isEmpty())return;
    operation_.operationType="disk_load_scan";operation_.sessionPath=path.toStdString();
    state_.forceTransitionTo(ApplicationState::RECOVERY_CONFIGURATION);handleStartRecovery();
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
    const bool preview=operation_.operationType=="disk_preview" || operation_.operationType=="disk_save_scan";
    const bool special=preview || operation_.operationType=="disk_load_scan" || operation_.operationType=="disk_rescue";
    const auto error = special ? std::string{} : service_->validateOperation(operation_);
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
    if(result.operationType=="disk_save_scan") {
        state_.transitionTo(ApplicationState::RESULTS);
        status_->setText(result.success ? tr("Scan saved; recoverable file contents were not copied") : QString::fromStdString(result.errorMessage));return;
    }
    if(result.operationType=="disk_load_scan" && result.success) {
        operation_={};operation_.targetDevicePath=result.details.at("Source");operation_.operationType="disk_scan";
        operation_.numSectors=service_->getDiskInfo(operation_.targetDevicePath).totalSectors;
        configuration_->setSelectedDevice(QString::fromStdString(operation_.targetDevicePath));
    }
    if(result.operationType=="disk_preview") {
        results_->setPreview(result);state_.transitionTo(ApplicationState::RESULTS);
        status_->setText(result.success ? tr("Preview ready; no files saved") : QString::fromStdString(result.errorMessage));return;
    }
    results_->setRecoveryResult(result); state_.transitionTo(ApplicationState::RESULTS);
    pages_->setCurrentWidget(results_);
    status_->setText(result.cancelled && result.scanOnly ? tr("Scan paused. Save scan results to continue later, or select found files to recover.") : result.scanOnly ? tr("Review found files, or continue a partial scan from the File menu.") : result.cancelled ? tr("Cancelled; review saved items below") : (result.success ? tr("Scan complete") : tr("Scan finished with errors; review results")));
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
        if(operation_.operationType=="disk_rescue") {
            result.operationType=operation_.operationType;
            result=service_->rescueToImage(operation_.outputPath,[this](uint64_t bytes){
                application::service::models::RecoveryProgress p{};p.operationType="disk_rescue";p.currentStep="Copying readable sectors; gaps are logged";p.processedBytes=bytes;p.isCancellable=true;emit progressUpdated(p);
            },[this]{return cancelled_.load();},operation_.resumeRescue);
        } else if(operation_.operationType=="disk_save_scan") {
            result.operationType=operation_.operationType;
            service_->saveScan(operation_.sessionPath,[this]{return cancelled_.load();});result.success=true;
        } else if(operation_.operationType=="disk_load_scan") {
            result.operationType=operation_.operationType;
            result=service_->loadScan(operation_.sessionPath,[this]{return cancelled_.load();});result.operationType="disk_load_scan";
        } else if(operation_.operationType=="disk_preview") {
            result.operationType="disk_preview";
            if(operation_.selectedPaths.size()!=1)throw std::runtime_error("Choose one file to inspect");
            result.previewBytes=service_->previewCandidate(operation_.selectedPaths.front(),[this]{return cancelled_.load();},16*1024*1024);
            QByteArray input(reinterpret_cast<const char*>(result.previewBytes.data()),qsizetype(result.previewBytes.size()));
            QBuffer buffer(&input);buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer);
            const auto format=reader.format();
            const auto size=reader.size();
            // Raster formats only, with bounds before decoding. Never launch a viewer,
            // render SVG/HTML, resolve external resources or decode on the GUI thread.
            if ((format=="png" || format=="jpeg" || format=="bmp" || format=="gif" || format=="webp") && size.isValid() &&
                size.width()<=20000 && size.height()<=20000 && uint64_t(size.width())*uint64_t(size.height())<=16000000 && !cancelled_.load()) {
                QImageReader::setAllocationLimit(64);
                reader.setAutoTransform(true);
                reader.setScaledSize(size.scaled(800,450,Qt::KeepAspectRatio));
                const auto image=reader.read();
                if (!image.isNull() && !cancelled_.load()) {
                    QByteArray png;QBuffer output(&png);output.open(QIODevice::WriteOnly);
                    if (image.save(&output,"PNG") && png.size()<=4*1024*1024)
                        result.previewImagePNG.assign(png.begin(),png.end());
                }
            }
            if(cancelled_.load())throw std::runtime_error("Preview cancelled");
            if(result.previewBytes.size()>65536)result.previewBytes.resize(65536);
            result.success=true;
        } else result = service_->executeOperation(operation_, [this](const auto& progress) { emit progressUpdated(progress); }, [this] { return cancelled_.load(); });
    } catch (const std::exception& failure) { result.errorMessage = failure.what(); result.cancelled = cancelled_.load(); }
    emit finished(result);
}
}
