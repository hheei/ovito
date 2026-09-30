// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// The checks that drive the import path of the workbench: a trajectory import, an unsupported file and the
// cancellation of a running import.

#include <ovito/gui/qml/spike/SpikeHarness.h>

namespace Ovito::Spike {

static /// Step 3 of the import check: imports a large file and cancels the operation as soon as it is running, verifying that
/// the shell reports the cancellation and that the data set contains no partially loaded pipeline afterwards.
void verifyCancelledImport(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    QmlWorkbenchController* controller = ui->workbenchController();
    const int objectsBefore = sceneObjectCount(ui);

    const QString poscarFile = QDir::tempPath() + QStringLiteral("/ovito-qml-import-test/large.poscar");
    if(writePoscarFile(poscarFile, 140).isEmpty()) {
        continuation();
        return;
    }

    // The import blocks the main thread while it loads the data, but it keeps processing events, so a timer observes the
    // running operation and requests its cancellation - as soon as the task progress model reports the running import,
    // and at the latest after 200 ms, which the check below relies on: the file has to be large enough that the import
    // is still running then (a larger file is the knob to turn if that ever stops holding).
    //
    // Whether the model reported the import by then is logged rather than asserted: how soon an importer begins to
    // report progress depends on the machine and the file format, which made this check fail on the slower CI runners
    // although the import was cancelled correctly there. The per-task display of the model is verified deterministically
    // by the parity check, which drives the model itself; what this check asserts is what is deterministic here - the
    // canceled import reports the cancellation, leaves no partially loaded pipeline and leaves the model idle.
    constexpr int cancelAfterMs = 200;
    auto observedProgress = std::make_shared<bool>(false);
    QElapsedTimer elapsed;
    elapsed.start();
    auto* timer = new QTimer(ui->view());
    QObject::connect(timer, &QTimer::timeout, controller, [controller, timer, ui, elapsed, observedProgress]() {
        if(TaskProgressModel* model = ui->taskProgressModel(); model && model->isBusy()) {
            if(!*observedProgress)
                reportTaskProgress(ui, QStringLiteral("while the import is running"));
            *observedProgress = true;
        }
        if(controller->cancellable() && (elapsed.elapsed() >= cancelAfterMs || *observedProgress)) {
            timer->stop();
            qInfo() << "IMPORT_TEST requesting the cancellation of the running import";
            controller->cancelCurrentOperation();
        }
    });
    timer->start(10);

    controller->importFiles(QVariantList{ QUrl::fromLocalFile(poscarFile) });
    const qint64 duration = elapsed.elapsed();
    timer->stop();
    timer->deleteLater();

    qInfo() << "IMPORT_TEST canceled import: took" << duration << "ms," << sceneObjectCount(ui) << "object(s) in the scene"
            << "(was" << objectsBefore << "before), status" << controller->statusMessage()
            << ", reported by the task model:" << *observedProgress;
    // The cancelled import must not leave its own pipeline behind. (A ResetScene import deletes the previous objects
    // before it creates the new pipeline, so they are gone by then - the same order of events the classic frontend's
    // import follows; what matters is that no pipeline with a data source that was never filled remains.)
    if(sceneReadsFile(ui, QFileInfo(poscarFile).fileName()))
        reportVerificationFailure(QStringLiteral("the canceled import left a partially loaded pipeline in the scene"));
    if(!controller->statusMessage().startsWith(QStringLiteral("Import cancelled")))
        reportVerificationFailure(QStringLiteral("the canceled import did not report the cancelled state (status: %1)").arg(controller->statusMessage()));

    // The workbench refreshes the model at most every 100 ms, so the row of the canceled import can still be there right
    // after the import has returned. It has to disappear once the operation has wound down - a progress record that
    // stayed behind would keep the status line busy forever.
    pollUntil(ui, 50, 2000, [ui]() {
        TaskProgressModel* model = ui->taskProgressModel();
        return !model || !model->isBusy();
    }, [ui, continuation = std::move(continuation)](bool idle) {
        if(!idle)
            reportVerificationFailure(QStringLiteral("the task progress model still reports the canceled import"));
        reportTaskProgress(ui, QStringLiteral("after the cancelled import"));
        continuation();
    });
}

static /// Step 2 of the import check: imports a file of an unsupported format, which must report an error and leave the scene
/// alone, and answers the error dialog the shell shows for it.
///
/// The frontend presents that dialog from the event loop and waits for it, so the run continues only after the dialog
/// has been answered - which is exactly what this verifies.
void verifyUnsupportedFileImport(QmlMainWindowUI* ui, const QString& directory, std::function<void()> continuation)
{
    QmlWorkbenchController* controller = ui->workbenchController();
    const QString unsupportedFile = directory + QStringLiteral("/unsupported.dat");
    {
        QFile file(unsupportedFile);
        if(!file.open(QIODevice::WriteOnly)) {
            reportVerificationFailure(QStringLiteral("cannot write the unsupported data file"));
            continuation();
            return;
        }
        file.write(QByteArray(4096, '\x01'));
    }

    const int objectsBefore = sceneObjectCount(ui);
    controller->importFiles(QVariantList{ QUrl::fromLocalFile(unsupportedFile) });
    qInfo() << "IMPORT_TEST unsupported format:" << sceneObjectCount(ui) << "object(s) in the scene, status"
            << controller->statusMessage();
    if(sceneObjectCount(ui) != objectsBefore)
        reportVerificationFailure(QStringLiteral("the failed import changed the scene"));
    if(controller->statusMessage().isEmpty())
        reportVerificationFailure(QStringLiteral("the failed import did not report anything"));

    pollUntil(ui, 50, 10000, [controller]() { return controller->messageBoxVisible(); }, [ui, controller, continuation](bool appeared) {
        if(appeared) {
            qInfo() << "IMPORT_TEST error dialog:" << controller->messageBoxTitle() << "/"
                    << controller->messageBoxText().split(QLatin1Char('\n')).first();
            controller->answerMessageBox(static_cast<int>(UserInterface::MessageBoxButton::Ok));
        }
        else {
            reportVerificationFailure(QStringLiteral("the failed import did not show an error dialog"));
        }
        verifyCancelledImport(ui, std::move(continuation));
    });
}

void runImportTest(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    QmlWorkbenchController* controller = ui->workbenchController();
    if(controller == nullptr) {
        reportVerificationFailure(QStringLiteral("the workbench has no shell controller"));
        continuation();
        return;
    }

    const QString directory = QDir::tempPath() + QStringLiteral("/ovito-qml-import-test");
    QDir().mkpath(directory);

    // ---------------------------------------------------------------------------------------------
    // 1. A trajectory of three files becomes a single pipeline spanning three animation frames.
    // ---------------------------------------------------------------------------------------------
    QVariantList trajectoryUrls;
    for(int frame = 0; frame < 3; frame++) {
        const QString path = writeLatticeFile(QStringLiteral("%1/frame_%2.xyz").arg(directory).arg(frame), 4, 3.6 + 0.05 * frame);
        if(path.isEmpty()) {
            continuation();
            return;
        }
        trajectoryUrls.push_back(QUrl::fromLocalFile(path));
    }

    const int objectsBefore = sceneObjectCount(ui);
    const bool hadDataBefore = controller->hasData();
    qInfo() << "IMPORT_TEST before:" << objectsBefore << "object(s) in the scene, hasData" << hadDataBefore;
    controller->importFiles(trajectoryUrls);

    // The importer sets up a file source and returns; the list of frames of a multi-file trajectory is discovered when
    // the pipeline is evaluated for the first time, which the viewports trigger. So the result of the import has to be
    // polled for instead of being read right away.
    pollUntil(ui, 100, 20000, [ui]() {
        const FileSource* fileSource = firstFileSource(ui);
        return fileSource && fileSource->numberOfSourceFrames() == 3;
    }, [ui, controller, directory, continuation](bool ready) {
        const FileSource* fileSource = firstFileSource(ui);
        const int frameCount = fileSource ? fileSource->numberOfSourceFrames() : -1;
        const Scene* scene = ui->datasetContainer().activeScene();
        const AnimationSettings* animation = scene ? scene->animationSettings() : nullptr;
        qInfo() << "IMPORT_TEST trajectory:" << sceneObjectCount(ui) << "object(s) in the scene, first pipeline has"
                << frameCount << "frame(s), animation interval"
                << (animation ? animation->firstFrame() : -1) << ".." << (animation ? animation->lastFrame() : -1);
        if(frameCount != 3)
            reportVerificationFailure(QStringLiteral("importing three files did not produce one pipeline with three animation frames (got %1)").arg(frameCount));
        if(animation && animation->numberOfFrames() != 3)
            reportVerificationFailure(QStringLiteral("the animation interval does not span the three imported frames"));
        if(!controller->hasData())
            reportVerificationFailure(QStringLiteral("the empty state was not left after the import"));
        // The imported trajectory is the natural place to probe the playback command: a static scene cannot play.
        if(Command* playbackCommand = ui->actionManager() ? ui->actionManager()->findCommand(QStringLiteral("AnimationTogglePlayback")) : nullptr)
            probeAnimationPlaybackCommand(ui, playbackCommand);

        verifyUnsupportedFileImport(ui, directory, std::move(continuation));
    });
    return;
}

}   // namespace Ovito::Spike
