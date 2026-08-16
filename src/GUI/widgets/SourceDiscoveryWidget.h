#ifndef RECOVERYSUITE_GUI_WIDGETS_SOURCEDISCOVERYWIDGET_H
#define RECOVERYSUITE_GUI_WIDGETS_SOURCEDISCOVERYWIDGET_H

#include <QWidget>
#include <QTableView>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QStandardItemModel>
#include <QStandardItem>

#include "../../Application/Service/GUIRecoveryService.h"

namespace recoverysuite {
namespace gui {
namespace widgets {

/**
 * SourceDiscoveryWidget
 *
 * Widget for discovering and selecting storage sources for recovery operations.
 */
class SourceDiscoveryWidget : public QWidget {
    Q_OBJECT

public:
    explicit SourceDiscoveryWidget(QWidget *parent = nullptr);
    ~SourceDiscoveryWidget() override;

    /**
     * @brief Set the GUI recovery service
     * @param service Pointer to the GUI recovery service
     */
    void setRecoveryService(recoverysuite::application::service::GUIRecoveryService* service);

signals:
    /**
     * Emitted when a source is selected and validated
     * @param devicePath The path of the selected device (e.g., "/dev/sda")
     */
    void sourceSelected(const QString& devicePath);

public slots:
    void onRefreshClicked();
private slots:
    void onSelectClicked();
    void onDeviceDoubleClicked(const QModelIndex& index);

private:
    // UI Components
    QTableView* deviceTableView_;
    QPushButton* refreshButton_;
    QPushButton* selectButton_;
    QLabel* statusLabel_;

    // Model for the table view
    QStandardItemModel* deviceModel_;

    // Service
    recoverysuite::application::service::GUIRecoveryService* recoveryService_;

    // Selected device path
    QString selectedDevicePath_;

    // Private methods
    void setupUI();
    void setupConnections();
    void populateDeviceTable();
    void updateStatus(const QString& message);
};

} // namespace widgets
} // namespace gui
} // namespace recoverysuite

#endif // RECOVERYSUITE_GUI_WIDGETS_SOURCEDISCOVERYWIDGET_H