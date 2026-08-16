#include "SourceDiscoveryWidget.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>
#include "Logging/Logger.h"

namespace recoverysuite {
namespace gui {
namespace widgets {

SourceDiscoveryWidget::SourceDiscoveryWidget(QWidget *parent)
    : QWidget(parent),
      recoveryService_(nullptr),
      selectedDevicePath_("") {
    setupUI();
    setupConnections();
}

SourceDiscoveryWidget::~SourceDiscoveryWidget() {
    // Destructor
}

void SourceDiscoveryWidget::setRecoveryService(recoverysuite::application::service::GUIRecoveryService* service) {
    recoveryService_ = service;
    // Initially populate the device table
    populateDeviceTable();
}

void SourceDiscoveryWidget::setupUI() {
    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // Status label
    statusLabel_ = new QLabel("Ready", this);
    statusLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    statusLabel_->setStyleSheet(
        "QLabel { "
        "   padding: 4px; "
        "   background-color: #f0f0f0; "
        "   border: 1px solid #ccc; "
        "   border-radius: 4px; "
        "}");
    mainLayout->addWidget(statusLabel_);

    // Device table view
    deviceTableView_ = new QTableView(this);
    deviceTableView_->setAlternatingRowColors(true);
    deviceTableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    deviceTableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    deviceTableView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    deviceTableView_->horizontalHeader()->setStretchLastSection(true);
    deviceTableView_->verticalHeader()->setVisible(false);
    deviceTableView_->setSortingEnabled(true);
    mainLayout->addWidget(deviceTableView_);

    // Button layout
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);

    refreshButton_ = new QPushButton("&Refresh", this);
    refreshButton_->setToolTip("Refresh the list of storage devices");
    buttonLayout->addWidget(refreshButton_);

    selectButton_ = new QPushButton("&Select Source", this);
    selectButton_->setToolTip("Select the highlighted device for recovery operations");
    selectButton_->setEnabled(false); // Initially disabled until a device is selected
    buttonLayout->addWidget(selectButton_);

    buttonLayout->addStretch();

    mainLayout->addLayout(buttonLayout);

    // Set the model for the table view
    deviceModel_ = new QStandardItemModel(0, 5, this);
    deviceModel_->setHorizontalHeaderLabels({
        QString("Device"),
        QString("Capacity"),
        QString("Sector Size"),
        QString("Partitions"),
        QString("Status")
    });
    deviceTableView_->setModel(deviceModel_);

    // Adjust column widths
    deviceTableView_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    deviceTableView_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    deviceTableView_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    deviceTableView_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    deviceTableView_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    setLayout(mainLayout);
}

void SourceDiscoveryWidget::setupConnections() {
    connect(refreshButton_, &QPushButton::clicked, this, &SourceDiscoveryWidget::onRefreshClicked);
    connect(selectButton_, &QPushButton::clicked, this, &SourceDiscoveryWidget::onSelectClicked);
    connect(deviceTableView_, &QTableView::doubleClicked, this, &SourceDiscoveryWidget::onDeviceDoubleClicked);
    connect(deviceTableView_->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this](const QItemSelection& selected, const QItemSelection&) {
        selectButton_->setEnabled(!selected.isEmpty());
        if (!selected.isEmpty()) {
            QModelIndex index = selected.indexes().first();
            int row = index.row();
            selectedDevicePath_ = deviceModel_->index(row, 0).data().toString();
        }
    });
}

void SourceDiscoveryWidget::onRefreshClicked() {
    updateStatus("Refreshing storage devices...");
    refreshButton_->setEnabled(false);
    selectButton_->setEnabled(false);

    // Use a timer to simulate non-blocking operation (in a real implementation, we would use threads or async)
    QTimer::singleShot(100, this, [this]() {
        populateDeviceTable();
        refreshButton_->setEnabled(true);
        updateStatus("Ready");
    });
}

void SourceDiscoveryWidget::onSelectClicked() {
    if (selectedDevicePath_.isEmpty()) {
        QMessageBox::warning(this, "No Selection", "Please select a device from the list.");
        return;
    }

    // Validate the selected source (in a real implementation, we would call the service to validate)
    // For now, we assume the selected device is valid.
    updateStatus(tr("Validating source %1...").arg(selectedDevicePath_));

    // Simulate validation delay
    QTimer::singleShot(500, this, [this]() {
        // In a real implementation, we would check if the device is available, readable, etc.
        // For now, we assume validation passes.
        updateStatus(tr("Source %1 validated.").arg(selectedDevicePath_));
        emit sourceSelected(selectedDevicePath_);
    });
}

void SourceDiscoveryWidget::onDeviceDoubleClicked(const QModelIndex& index) {
    int row = index.row();
    selectedDevicePath_ = deviceModel_->index(row, 0).data().toString();
    onSelectClicked(); // Trigger selection on double click
}

void SourceDiscoveryWidget::populateDeviceTable() {
    if (!recoveryService_) {
        updateStatus("Recovery service not available.");
        return;
    }

    // Clear the existing model
    deviceModel_->removeRows(0, deviceModel_->rowCount());

    // In a real implementation, we would get the list of storage devices from the service.
    // For now, we will use placeholder data.

    // We are going to simulate a few devices.
    // Note: In a real application, we would call recoveryService_->enumerateStorageDevices()
    // and then for each device, get detailed information.

    // Since we don't have a real implementation of enumerateStorageDevices in the GUIRecoveryService,
    // we will use mock data for now.

    // Mock data: list of devices with their properties.
    struct MockDevice {
        QString devicePath;
        QString modelName;
        QString serialNumber;
        uint64_t totalSizeBytes;
        uint32_t sectorSizeBytes;
        bool isRemovable;
        bool isReadOnly;
    };

    QList<MockDevice> mockDevices = {
        {"/dev/sda", "Samsung SSD 860 EVO", "S2H3NF0J900000", 500ULL * 1024 * 1024 * 1024, 512, false, false},
        {"/dev/sdb", "Seagate Barracuda", "WMAP987654321", 2ULL * 1024 * 1024 * 1024 * 1024, 512, false, false},
        {"/dev/sdc", "SanDisk Ultra Fit", "1234567890ABCDEF", 32ULL * 1024 * 1024 * 1024, 512, true, false},
        {"/dev/sdd", "Dummy Read-Only Disk", "READONLY000000", 10ULL * 1024 * 1024 * 1024, 512, false, true}
    };

    for (const MockDevice& device : mockDevices) {
        int row = deviceModel_->rowCount();
        deviceModel_->insertRow(row);

        // Device path
        deviceModel_->setData(deviceModel_->index(row, 0), device.devicePath);

        // Capacity
        QString capacityStr;
        if (device.totalSizeBytes >= 1024ULL * 1024 * 1024) {
            capacityStr = QString("%1 GB").arg(device.totalSizeBytes / (1024ULL * 1024 * 1024));
        } else if (device.totalSizeBytes >= 1024ULL * 1024) {
            capacityStr = QString("%1 MB").arg(device.totalSizeBytes / (1024ULL * 1024));
        } else {
            capacityStr = QString("%1 KB").arg(device.totalSizeBytes / 1024);
        }
        deviceModel_->setData(deviceModel_->index(row, 1), capacityStr);

        // Sector size
        deviceModel_->setData(deviceModel_->index(row, 2), QString("%1 bytes").arg(device.sectorSizeBytes));

        // Partitions (we don't have real partition data, so we show a placeholder)
        deviceModel_->setData(deviceModel_->index(row, 3), "Scan required");

        // Status
        QString statusStr;
        if (device.isReadOnly) {
            statusStr = "Read-Only";
        } else {
            statusStr = "Ready";
        }
        if (!device.isRemovable) {
            statusStr += ", Fixed";
        } else {
            statusStr += ", Removable";
        }
        deviceModel_->setData(deviceModel_->index(row, 4), statusStr);
    }

    // If no devices were found, show a message
    if (mockDevices.isEmpty()) {
        deviceModel_->insertRow(0);
        deviceModel_->setData(deviceModel_->index(0, 0), "No devices detected");
        deviceModel_->setData(deviceModel_->index(0, 1), "");
        deviceModel_->setData(deviceModel_->index(0, 2), "");
        deviceModel_->setData(deviceModel_->index(0, 3), "");
        deviceModel_->setData(deviceModel_->index(0, 4), "");
    }

    updateStatus(tr("Found %1 device(s).").arg(mockDevices.size()));
}

void SourceDiscoveryWidget::updateStatus(const QString& message) {
    statusLabel_->setText(message);
    recoverysuite::logging::Logger::instance().info("SourceDiscoveryWidget: " + message.toStdString());
}

} // namespace widgets
} // namespace gui
} // namespace recoverysuite