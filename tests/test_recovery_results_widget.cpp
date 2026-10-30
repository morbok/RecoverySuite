#include <QTest>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include "GUI/widgets/RecoveryResultsWidget.h"
#include "GUI/core/MainWindow.h"
#include "Application/Service/Models/GUIModels.h"
using recoverysuite::gui::widgets::RecoveryResultsWidget;
using recoverysuite::application::service::models::RecoveryResult;
class TestRecoveryResultsWidget : public QObject {
    Q_OBJECT
    QString field(RecoveryResultsWidget& widget, const char* name) {
        auto* label = widget.findChild<QLabel*>(name);
        return label ? label->text() : QString("MISSING FIELD");
    }
private slots:
    void initialAndReset() {
        RecoveryResultsWidget widget;
        QCOMPARE(field(widget, "resultStatus"), QString("No operation completed"));
        RecoveryResult result{};
        result.success = true;
        result.recoveredItems = {{"photo.jpg", 42}};
        result.recoveredItemsCount = 1;
        result.errorMessage = "old details";
        widget.setRecoveryResult(result);
        widget.clear();
        QCOMPARE(field(widget, "resultStatus"), QString("No operation completed"));
        QVERIFY(widget.findChild<QListWidget*>("resultItems")->count() == 0);
        QVERIFY(widget.findChild<QPlainTextEdit*>("resultDetails")->toPlainText().isEmpty());
    }
    void outcomes_data() {
        QTest::addColumn<bool>("success"); QTest::addColumn<bool>("cancelled");
        QTest::addColumn<qulonglong>("recovered"); QTest::addColumn<qulonglong>("failed");
        QTest::addColumn<QString>("expected");
        QTest::newRow("complete") << true << false << 1ULL << 0ULL << QString("Completed");
        QTest::newRow("empty") << true << false << 0ULL << 0ULL << QString("Completed");
        QTest::newRow("failure") << false << false << 0ULL << 0ULL << QString("Failed");
        QTest::newRow("partial-success") << true << false << 2ULL << 1ULL << QString("Partial recovery");
        QTest::newRow("partial-failure") << false << false << 2ULL << 0ULL << QString("Partial recovery");
        QTest::newRow("cancelled-with-data") << false << true << 2ULL << 0ULL << QString("Cancelled");
        QTest::newRow("cancelled-empty") << false << true << 0ULL << 0ULL << QString("Cancelled");
    }
    void outcomes() {
        QFETCH(bool, success); QFETCH(bool, cancelled); QFETCH(qulonglong, recovered);
        QFETCH(qulonglong, failed); QFETCH(QString, expected);
        RecoveryResult result{}; result.success = success; result.cancelled = cancelled;
        result.recoveredItemsCount = recovered; result.failedItemsCount = failed;
        RecoveryResultsWidget widget; widget.setRecoveryResult(result);
        QCOMPARE(field(widget, "resultStatus"), expected);
    }
    void actualItemsAndUnknownMetrics() {
        RecoveryResultsWidget widget;
        RecoveryResult result{}; result.success = true;
        result.destination = "<b>/tmp/output</b>";
        result.errorMessage = "<script>error</script>";
        result.recoveredItemsCount = 1; result.recoveredBytes = 123;
        result.recoveredItems = {{"photo.jpg", 123}};
        widget.setRecoveryResult(result);
        QCOMPARE(field(widget, "resultProcessedBytes"), QString("Not reported"));
        QCOMPARE(field(widget, "resultFailedBytes"), QString("Not reported"));
        QCOMPARE(field(widget, "resultElapsed"), QString("Not reported"));
        QCOMPARE(field(widget, "resultExport"), QString("Not performed"));
        QCOMPARE(widget.findChild<QLabel*>("resultDestination")->textFormat(), Qt::PlainText);
        QCOMPARE(widget.findChild<QPlainTextEdit*>("resultDetails")->toPlainText(), QString("<script>error</script>"));
        QCOMPARE(widget.findChild<QListWidget*>("resultItems")->item(0)->text(), QString("photo.jpg (123 bytes)"));
        result.processedBytesKnown = true; result.processedBytes = 4096;
        result.failedBytesKnown = true; result.failedBytes = 0;
        result.startTime = std::chrono::system_clock::now();
        result.endTime = result.startTime + std::chrono::seconds(3);
        widget.setRecoveryResult(result);
        QCOMPARE(field(widget, "resultProcessedBytes"), QString("4096"));
        QCOMPARE(field(widget, "resultFailedBytes"), QString("0"));
        QCOMPARE(field(widget, "resultElapsed"), QString("3 s"));
    }
    void serviceFailureAndCancellation() {
        recoverysuite::application::service::GUIRecoveryService service(nullptr);
        for (int operation = 0; operation != 3; ++operation) {
            auto invoke = [&](bool cancel) {
                auto token = [cancel] { return cancel; };
                if (operation == 0) return service.recoverFiles("source", 0, 1, "output", {}, token);
                if (operation == 1) return service.recoverMetadata("source", 0, 1, "output", {}, token);
                return service.carveFiles("source", 0, 1, "output", {}, token);
            };
            const auto failed = invoke(false);
            QVERIFY(!failed.success); QVERIFY(!failed.cancelled); QVERIFY(!failed.errorMessage.empty());
            QVERIFY(!failed.outputWritten); QVERIFY(!failed.processedBytesKnown);
            QCOMPARE(failed.recoveredBytes, uint64_t{0});
            QCOMPARE(failed.destination, std::string("output"));
            QVERIFY(failed.endTime >= failed.startTime);
            const auto cancelled = invoke(true);
            QVERIFY(cancelled.cancelled); QVERIFY(!cancelled.success);
        }
    }
    void cancelledWorkerAlwaysFinishes() {
        using recoverysuite::gui::core::RecoveryWorker;
        recoverysuite::application::service::GUIRecoveryService service(nullptr);
        QThread thread;
        auto* worker = new RecoveryWorker(&service, "source", 0, 1, "output");
        QSignalSpy finished(worker, &RecoveryWorker::finished);
        worker->moveToThread(&thread);
        connect(&thread, &QThread::started, worker, &RecoveryWorker::process);
        connect(worker, &RecoveryWorker::finished, &thread, &QThread::quit, Qt::DirectConnection);
        connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
        worker->cancel(); // Safe before or during the blocking backend call.
        thread.start();
        const bool stopped = thread.wait(3000);
        if (!stopped) { thread.quit(); thread.wait(); }
        QVERIFY(stopped);
        QCOMPARE(finished.count(), 1);
        const auto result = qvariant_cast<RecoveryResult>(finished.at(0).at(0));
        QVERIFY(result.cancelled); QVERIFY(!result.success);
    }
};
QTEST_MAIN(TestRecoveryResultsWidget)
#include "test_recovery_results_widget.moc"
