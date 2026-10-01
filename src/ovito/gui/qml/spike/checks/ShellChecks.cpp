// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// The checks that assert the state of the workbench shell: the menus, the commands, the recent files, the dialogs,
// the library lists, the icons, the settings, the keyboard focus order, the window state and the session workflow.
// Everything they share is in SpikeHarness.h; the helpers of one check stay in this file.

#include <ovito/gui/qml/spike/SpikeHarness.h>

namespace Ovito::Spike {

/// Probes the "Play Animation" command: the checkable command has to start and stop the playback of the data set, which
/// is what the menu entry and the toolbar button of the classic frontend toggle. The scene must hold an animation with
/// more than one frame; a single-frame scene never starts a playback (SceneAnimationPlayback::startAnimationPlayback).
///
/// This is verified where a trajectory exists - either because the check was given one on the command line (the command
/// check probes it then) or right after the import check has imported one.
void probeAnimationPlaybackCommand(QmlMainWindowUI* ui, Command* playbackCommand)
{
    OVITO_ASSERT(playbackCommand);
    playbackCommand->setChecked(true);
    if(!ui->datasetContainer().isPlaybackActive())
        reportVerificationFailure(QStringLiteral("playback command: checking the command did not start the playback"));
    playbackCommand->setChecked(false);
    if(ui->datasetContainer().isPlaybackActive())
        reportVerificationFailure(QStringLiteral("playback command: unchecking the command did not stop the playback"));
    qInfo() << "COMMAND_TEST the playback command controls the animation playback";
}

static /// Verifies the entries of a QML menu against the rule the shell's menus follow: an entry presents a command of the
/// shared command layer, or it is one of the frontend side actions its own check exercises, or it is disabled and names
/// the phase that will deliver it. An entry that none of this applies to would look clickable while doing nothing, so
/// it fails the check.
/// \param allowFrontendActions  True for a menu whose entries call into the frontend instead of triggering a command,
///                              such as the viewport context menu (its actions are driven by verifyContextMenu()).
void verifyMenuEntries(QObject* menu, const QString& what, bool allowFrontendActions)
{
    if(menu == nullptr) {
        reportVerificationFailure(QStringLiteral("the workbench has no %1").arg(what));
        return;
    }

    int entries = 0;
    int commands = 0;
    int frontendActions = 0;
    int pending = 0;
    std::function<void(QObject*)> walk = [&](QObject* parent) {
        for(QObject* child : parent->children()) {
            // The entries are the Qt Quick Controls menu items; a separator carries neither a label nor a state. The
            // placeholder that the recent files submenu shows while the list is empty is not an entry of its own, so it
            // names itself and is skipped here. (Note that 'visible' cannot be used for this: a closed menu reports all
            // of its items as invisible.)
            if(child->inherits("QQuickMenuItem") && !child->inherits("QQuickMenuSeparator")
                    && child->objectName() != QStringLiteral("emptyRecentFilesEntry")) {
                const QString text = child->property("text").toString();
                const bool enabled = child->property("enabled").toBool();
                Command* command = qmlProperty(child, QStringLiteral("command")).value<Command*>();
                const QString ownerPhase = qmlProperty(child, QStringLiteral("ownerPhase")).toString();
                entries++;
                QString wiring;
                if(text.isEmpty()) {
                    reportVerificationFailure(QStringLiteral("the %1 holds an entry without a label").arg(what));
                    wiring = QStringLiteral("<without a label>");
                }
                else if(command != nullptr) {
                    commands++;
                    wiring = command->id();
                }
                else if(child->property("subMenu").value<QObject*>() != nullptr) {
                    // An entry that opens a submenu is a container, not an action of its own: what it holds is verified
                    // on its own (the recent files by verifyRecentFiles(), the view types by the context menu check).
                    frontendActions++;
                    wiring = QStringLiteral("<submenu of the frontend>");
                }
                else if(enabled && allowFrontendActions) {
                    frontendActions++;
                    wiring = QStringLiteral("<action of the frontend>");
                }
                else if(!enabled && !ownerPhase.isEmpty()) {
                    pending++;
                    wiring = QStringLiteral("<delivered by> ") + ownerPhase;
                }
                else {
                    wiring = QStringLiteral("<nothing>");
                    reportVerificationFailure(QStringLiteral("the entry \"%1\" of the %2 is %3 without belonging to a command of the shared layer%4")
                        .arg(text, what, enabled ? QStringLiteral("enabled") : QStringLiteral("disabled"),
                             ownerPhase.isEmpty() ? QString() : QStringLiteral(" (it names %1)").arg(ownerPhase)));
                }
                qInfo() << "PARITY_TEST  entry" << text << (enabled ? "[enabled]" : "[disabled]") << wiring
                        << QStringLiteral("(%1)").arg(child->metaObject()->className());
            }
            walk(child);
        }
    };
    walk(menu);

    qInfo() << "PARITY_TEST the" << what << "holds" << entries << "entries:" << commands << "of the shared command layer,"
            << frontendActions << "of the frontend and" << pending << "that name the phase which will deliver them";
    if(entries < 3)
        reportVerificationFailure(QStringLiteral("the %1 lists only %2 entries").arg(what).arg(entries));
}

static /// The About command of the shared command layer is presented by whichever frontend runs: the classic one opens a widget
/// dialog, so the shell brings its own dialog, and this verifies that the command reaches it and that it names the
/// application.
void verifyAboutDialog(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    QmlWorkbenchController* controller = ui->workbenchController();
    QQuickItem* rootObject = ui->view() ? ui->view()->rootObject() : nullptr;
    QObject* dialog = rootObject ? rootObject->findChild<QObject*>(QStringLiteral("aboutDialog")) : nullptr;
    if(controller == nullptr || dialog == nullptr) {
        reportVerificationFailure(QStringLiteral("the shell has no About dialog"));
        continuation();
        return;
    }

    controller->showAboutDialog();
    pollUntil(ui, 50, 2000, [dialog]() { return dialog->property("visible").toBool(); }, [dialog, continuation](bool opened) {
        const QString title = dialog->property("title").toString();
        qInfo() << "PARITY_TEST the About dialog:" << title;
        if(!opened)
            reportVerificationFailure(QStringLiteral("the About command did not open the About dialog of the shell"));
        else if(!title.contains(Application::applicationName()))
            reportVerificationFailure(QStringLiteral("the About dialog does not name the application"));
        QMetaObject::invokeMethod(dialog, "close");
        continuation();
    });
}

static /// Opens the viewport context menu the way a right-click on the title label of a pane does, and exercises what it
/// offers. Everything it changes is restored afterwards, because the following steps render the same scene.
void verifyContextMenu(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    QmlViewportMenu* menu = ui->viewportMenu();
    QQuickItem* rootObject = ui->view() ? ui->view()->rootObject() : nullptr;
    QObject* menuPopup = rootObject ? rootObject->findChild<QObject*>(QStringLiteral("viewportContextMenu")) : nullptr;
    if(menu == nullptr || menuPopup == nullptr) {
        reportVerificationFailure(QStringLiteral("the viewport context menu is not available"));
        continuation();
        return;
    }

    // The title label of a viewport belongs to the frame graph the viewport rendered last, and the viewport items are
    // rebuilt when the data set or the layout changes, so wait for a viewport that can actually be clicked instead of
    // reading the state a moment too early.
    pollUntil(ui, 100, 5000, [ui]() {
        QuickViewportItem* item = firstViewportItem(ui);
        QuickViewportWindow* viewportWindow = item ? item->viewportWindow() : nullptr;
        return viewportWindow && !viewportWindow->contextMenuArea().isEmpty();
    }, [ui, menu, menuPopup, continuation](bool ready) {
        QuickViewportItem* item = firstViewportItem(ui);
        QuickViewportWindow* viewportWindow = item ? item->viewportWindow() : nullptr;
        Viewport* viewport = viewportWindow ? viewportWindow->viewport() : nullptr;
        ViewportConfiguration* config = ui->datasetContainer().activeViewportConfig();
        if(!ready || viewport == nullptr || config == nullptr) {
            reportVerificationFailure(QStringLiteral("no viewport of the workbench offers a title label to click on"));
            continuation();
            return;
        }

        const QRectF captionArea = viewportWindow->contextMenuArea();
        // The caption area belongs to the rendered frame graph, which is laid out in device pixels, while the viewport item
        // receives its mouse events in device independent coordinates.
        const qreal devicePixelRatio = item->window() ? item->window()->devicePixelRatio() : 1.0;
        const QPointF captionCenter = captionArea.center() / devicePixelRatio;

        Viewport* previousActiveViewport = config->activeViewport();
        const QString previousViewportTitle = viewport->objectTitle();
        const QPointF globalPos = item->mapToGlobal(captionCenter);
        QMouseEvent pressEvent(QEvent::MouseButtonPress, captionCenter, globalPos, Qt::RightButton, Qt::RightButton, Qt::NoModifier);
        QCoreApplication::sendEvent(item, &pressEvent);

        if(!menu->isOpen())
            reportVerificationFailure(QStringLiteral("a right-click on the title label of a viewport did not open its context menu"));
        if(menu->item() != item)
            reportVerificationFailure(QStringLiteral("the context menu belongs to another viewport than the one that was clicked"));
        if(!menuPopup->property("visible").toBool())
            reportVerificationFailure(QStringLiteral("the context menu of the viewport did not appear"));
        if(config->activeViewport() != viewport)
            reportVerificationFailure(QStringLiteral("clicking a viewport did not make it the active one"));

        verifyMenuEntries(menuPopup, QStringLiteral("viewport context menu"), true);

        // Show Grid, the view type and the constraint on the camera rotation are viewport state; the section level setting
        // of the rotation constraint is shared with the classic frontend, so it has to be saved and put back.
        const bool gridVisible = viewport->isGridVisible();
        menu->setGridVisible(!gridVisible);
        if(viewport->isGridVisible() != !gridVisible)
            reportVerificationFailure(QStringLiteral("the context menu did not change the visibility of the construction grid"));

        ViewportSettings& viewportSettings = ViewportSettings::getSettings();
        const bool constrainRotation = viewportSettings.constrainCameraRotation();
        menu->setConstrainRotation(!constrainRotation);
        if(viewportSettings.constrainCameraRotation() != !constrainRotation)
            reportVerificationFailure(QStringLiteral("the context menu did not change the camera rotation constraint"));

        const int defaultMaximizedType = viewportSettings.defaultMaximizedViewportType();
        const Viewport::ViewType viewType = viewport->viewType();
        menu->setViewType(Viewport::VIEW_FRONT);
        if(viewport->viewType() != Viewport::VIEW_FRONT)
            reportVerificationFailure(QStringLiteral("the context menu did not switch the viewport to another view type"));
        menu->setViewType(viewType);

        const bool wasMaximized = config->maximizedViewport() != nullptr;
        menu->toggleMaximize();
        if((config->maximizedViewport() != nullptr) == wasMaximized)
            reportVerificationFailure(QStringLiteral("the context menu did not maximize the viewport"));
        menu->toggleMaximize();
        if((config->maximizedViewport() != nullptr) != wasMaximized)
            reportVerificationFailure(QStringLiteral("the context menu left the viewport maximized"));

        viewportSettings.setConstrainCameraRotation(constrainRotation);
        viewportSettings.setDefaultMaximizedViewportType(static_cast<Viewport::ViewType>(defaultMaximizedType));
        viewportSettings.save();
        QMetaObject::invokeMethod(menuPopup, "close");
        menu->close();
        config->setActiveViewport(previousActiveViewport);

        qInfo() << "PARITY_TEST the viewport context menu of the" << previousViewportTitle
                << "viewport offers the view type, the construction grid, the rotation constraint and maximizing, and all of it was restored";

        continuation();
    });
}

static /// The status line lists one row per running task, fed by the shared task progress model. No operation of this prototype
/// runs long enough to be observed together with another one, so two progress records stand in for two concurrent
/// operations here - which is also the case the classic status bar (one aggregate bar) cannot show.
void verifyTaskRows(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    TaskProgressModel* model = ui->taskProgressModel();
    QQuickItem* rootObject = ui->view() ? ui->view()->rootObject() : nullptr;
    QObject* rows = rootObject ? rootObject->findChild<QObject*>(QStringLiteral("taskProgressRows")) : nullptr;
    if(model == nullptr || rows == nullptr) {
        reportVerificationFailure(QStringLiteral("the status line does not present the running tasks"));
        continuation();
        return;
    }

    // Reporting progress touches OVITO objects, which needs the task context of this user interface: a Qt timer
    // callback runs in no task of its own (see open item O8 of the audit).
    GuiTaskScope taskScope(*ui);

    // The records are shared, because they have to outlive this function: the model is read again once the status line
    // has caught up with it.
    auto firstTask = std::make_shared<TaskProgress>(ui);
    auto secondTask = std::make_shared<TaskProgress>(ui);
    firstTask->setText(QStringLiteral("Verifying the status line"));
    firstTask->setMaximum(4);
    firstTask->setValue(1);
    secondTask->setText(QStringLiteral("Verifying the task list"));
    secondTask->setMaximum(2);
    secondTask->setValue(1);

    pollUntil(ui, 100, 3000, [model]() { return model->rowCount() == 2; }, [model, rows, firstTask, secondTask, continuation](bool listed) {
        const int qmlRows = rows->property("count").toInt();
        qInfo() << "PARITY_TEST the status line shows" << qmlRows << "row(s) for" << model->rowCount()
                << "running task(s), busy" << model->isBusy() << "text" << model->activeText();
        if(!listed)
            reportVerificationFailure(QStringLiteral("the task progress model did not list the two running tasks"));
        else if(qmlRows != 2)
            reportVerificationFailure(QStringLiteral("the status line shows %1 row(s) for two running tasks").arg(qmlRows));
        else if(!model->isBusy() || model->activeText().isEmpty())
            reportVerificationFailure(QStringLiteral("the aggregate state of the running tasks is empty"));
        continuation();
    });
}

static /// The size and position of the workbench window are remembered in the shared settings store: a resize is written back
/// after a short delay, and the frontend can apply the remembered state again on the next start.
void verifyWindowState(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    QQuickWindow* window = ui->view();
    if(window == nullptr) {
        reportVerificationFailure(QStringLiteral("the shell has no window"));
        continuation();
        return;
    }

    const QRect storedGeometry = GuiSettings::instance().workbenchWindowGeometry();
    const bool storedMaximized = GuiSettings::instance().isWorkbenchWindowMaximized();

    // A window manager can adjust a requested size - the CI runners have small virtual displays and macOS clamped the
    // height of the window that was asked for below - so the check asks for a modest size and then works with what the
    // window actually became: the store is compared against the window, not against the request.
    const QSize requested(900, 600);
    window->resize(requested);
    pollUntil(ui, 100, 3000, [window]() { return GuiSettings::instance().workbenchWindowGeometry().size() == window->size(); },
        [ui, window, requested, storedGeometry, storedMaximized, continuation](bool saved) {
            const QSize actualSize = window->size();
            if(!saved)
                reportVerificationFailure(QStringLiteral("resizing the workbench window was not written to the settings store"));
            else if(actualSize != requested)
                qInfo() << "PARITY_TEST the platform adjusted the requested window size" << requested << "to" << actualSize;

            // The other direction: a remembered geometry is applied by the frontend.
            const QRect remembered(QPoint(60, 40), requested);
            GuiSettings::instance().setWorkbenchWindowGeometry(remembered);
            const bool applied = ui->applyStoredWindowState();
            qInfo() << "PARITY_TEST the shell restored the remembered window state:" << applied << window->size();
            if(!applied)
                reportVerificationFailure(QStringLiteral("the shell did not apply the remembered window state"));
            else if(actualSize == requested && window->size() != remembered.size())
                // Only asserted while the platform is known to honour this size, which the resize above established.
                reportVerificationFailure(QStringLiteral("the remembered window size was not applied (expected %1x%2, got %3x%4)")
                    .arg(remembered.width()).arg(remembered.height()).arg(window->width()).arg(window->height()));
            else if(window->size() != remembered.size())
                qInfo() << "PARITY_TEST the platform adjusted the remembered window size to" << window->size();

            // Leave the settings store as it was found - a developer machine runs the spike against its real settings.
            GuiSettings::instance().setWorkbenchWindowGeometry(storedGeometry);
            GuiSettings::instance().setWorkbenchWindowMaximized(storedMaximized);
            window->resize(1280, 800);
            continuation();
        });
}

static /// An import reports which format the file was understood as and how many source frames it holds. The format is known
/// right away, while the number of frames is discovered when the file source is evaluated, so the notice grows into its
/// final form while the pipeline is being processed.
void verifyImportNotice(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    QmlWorkbenchController* controller = ui->workbenchController();
    if(controller == nullptr) {
        reportVerificationFailure(QStringLiteral("the workbench has no shell controller"));
        continuation();
        return;
    }

    const QString directory = QDir::tempPath() + QStringLiteral("/ovito-qml-parity-test");
    QDir().mkpath(directory);

    // The file that OVITO's importer autodetection hands to the LAMMPS Data importer although it is an XYZ file: a
    // comment line mentioning atoms is all the LAMMPS importer needs (defect F6 of the spike report). Such a file
    // imports without an error but leaves an empty scene, so the notice reporting the format it was read as is the only
    // hint the user gets.
    const QString path = directory + QStringLiteral("/misdetected.xyz");
    {
        QFile file(path);
        if(!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            reportVerificationFailure(QStringLiteral("the import check could not write its test file"));
            continuation();
            return;
        }
        QTextStream stream(&file);
        stream << "8\n";
        // The comment line is the trigger: the LAMMPS Data importer accepts any line within the first 20 lines that
        // mentions atoms and starts with a number, and it takes precedence over the XYZ importer.
        stream << "8 atoms\n";
        for(int atom = 0; atom < 8; atom++)
            stream << "Ar 0 0 0\n";
    }

    controller->importFiles({QUrl::fromLocalFile(path)});
    pollUntil(ui, 100, 10000, [controller]() { return controller->notice().contains(QStringLiteral("source frame")); },
        [ui, controller, continuation](bool reported) {
            const QString message = controller->notice();
            const FileSource* fileSource = firstFileSource(ui);
            const QString formatUsed = fileSource && fileSource->importer() ? fileSource->importer()->objectTitle() : QString();
            qInfo() << "PARITY_TEST the import notice reads:" << message
                    << "| the file was read as" << formatUsed << "and left" << sceneObjectCount(ui) << "object(s) in the scene";
            if(!reported)
                reportVerificationFailure(QStringLiteral("the notice did not report the number of source frames"));
            else if(formatUsed.isEmpty() || !message.contains(formatUsed))
                reportVerificationFailure(QStringLiteral("the notice does not name the format the file was imported as (\"%1\")").arg(formatUsed));
            // Informational: the trap of defect F6 is that this file leaves no object behind, which the notice now makes
            // visible instead of the scene simply looking empty.
            else if(sceneObjectCount(ui) == 0)
                qInfo() << "PARITY_TEST the notice is the only visible outcome of that import: the scene holds no object";
            continuation();
        });
}

void runLibraryTest(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    declareCheckPhases({ "modifier library", "viewport layer library" });

    // Creating the models instantiates OVITO objects, which requires a task context when this runs from a Qt callback.
    GuiTaskScope taskScope(*ui);

    auto* modifiersModel = workbenchModels(ui).modifiers;
    auto* overlaysModel = workbenchModels(ui).overlays;

    int entries = 0;
    int categories = 0;
    for(AvailableModifiersModel* model : { modifiersModel }) {
        for(int category = 0; category < model->rowCount(); category++) {
            categories++;
            const QModelIndex categoryIndex = model->index(category, 0);
            for(int row = 0; row < model->rowCount(categoryIndex); row++) {
                const QModelIndex index = model->index(row, 0, categoryIndex);
                entries++;
                Command* command = model->data(index, AvailableModifiersModel::CommandRole).value<Command*>();
                if(command == nullptr) {
                    reportVerificationFailure(QStringLiteral("the modifier library row %1/%2 has no command").arg(category).arg(row));
                    continue;
                }
                if(command->id().isEmpty() || command->text().isEmpty())
                    reportVerificationFailure(QStringLiteral("the modifier library command of row %1/%2 is incomplete").arg(category).arg(row));
                if(ui->actionManager()->findCommand(command->id()) != command)
                    reportVerificationFailure(QStringLiteral("the modifier library command \"%1\" is not registered with the action manager").arg(command->id()));
                QAction* actionView = model->data(index, AvailableModifiersModel::ActionRole).value<QAction*>();
                if(actionView == nullptr || actionView != ui->actionManager()->actionView(command))
                    reportVerificationFailure(QStringLiteral("the modifier library row \"%1\" does not present the QAction view of its command").arg(command->id()));
            }
        }
    }
    qInfo() << "LIBRARY_TEST modifier library:" << categories << "categories," << entries << "commands";
    reportCheckPhase("modifier library");

    int overlayEntries = 0;
    int overlayCategories = 0;
    // The viewport layer library enumerates the layer classes of the plugins that are loaded in this run.
    for(int category = 0; category < overlaysModel->rowCount(); category++) {
        overlayCategories++;
        const QModelIndex categoryIndex = overlaysModel->index(category, 0);
        for(int row = 0; row < overlaysModel->rowCount(categoryIndex); row++) {
            const QModelIndex index = overlaysModel->index(row, 0, categoryIndex);
            overlayEntries++;
            Command* command = overlaysModel->data(index, AvailableOverlaysModel::CommandRole).value<Command*>();
            if(command == nullptr || command->id().isEmpty())
                reportVerificationFailure(QStringLiteral("the viewport layer library row %1/%2 has no command").arg(category).arg(row));
            else if(ui->actionManager()->findCommand(command->id()) != command)
                reportVerificationFailure(QStringLiteral("the viewport layer command \"%1\" is not registered with the action manager").arg(command->id()));
        }
    }
    qInfo() << "LIBRARY_TEST viewport layer library:" << overlayCategories << "categories," << overlayEntries << "commands";
    reportCheckPhase("viewport layer library");

    if(entries == 0)
        reportVerificationFailure(QStringLiteral("the modifier library is empty, so nothing was verified"));

    // The flags of a row follow the enabled state of its command, which is what the views of the libraries present.
    if(Command* first = modifiersModel->commandAt(0, 0)) {
        const QModelIndex firstIndex = modifiersModel->index(0, 0, modifiersModel->index(0, 0));
        const bool enabled = modifiersModel->flags(firstIndex).testFlag(Qt::ItemIsEnabled);
        if(enabled != first->isEnabled())
            reportVerificationFailure(QStringLiteral("the row flags of \"%1\" do not follow the state of its command").arg(first->id()));
    }

    // The insert commands of both libraries run inside performTransaction and are reached through Command::triggered, as
    // the classic frontend does when the user picks an entry. That path is not exercised here: inserting a viewport
    // layer switches the viewport into render preview mode, which the Qt Quick viewport does not implement yet (it is
    // an open item of Phase 5), and inserting a modifier needs a selected pipeline in the pipeline list model, which
    // only a selection operation of the frontend establishes.

    // Note: the library models are parented to the window and their commands stay registered with the action manager,
    // exactly as the frontends use them; unregistering them again would mean reimplementing the tool that owns them.
    continuation();
}

/// One icon image the QML scene of the workbench currently displays.
struct IconImage
{
    QUrl source;
    int status = 0;
};

/// QQuickImageBase::Status: the image is loaded and ready to be displayed.
constexpr int ImageStatusReady = 1;

/// Returns the icon images of the shell, i.e. the plain QML Image items whose source is an icon of the given image
/// provider. The source and the load status are read through the meta object of the item, which keeps this independent of
/// Qt Quick's private headers.
static QVector<IconImage> shellIconImages(QmlMainWindowUI* ui, const QString& host)
{
    QVector<IconImage> images;
    QQuickItem* root = ui->view() ? ui->view()->rootObject() : nullptr;
    if(!root)
        return images;

    std::function<void(QQuickItem*)> collect = [&](QQuickItem* parent) {
        for(QQuickItem* child : parent->childItems()) {
            if(child->inherits("QQuickImage")) {
                const QUrl source = child->property("source").toUrl();
                if(source.host() == host)
                    images.push_back({source, child->property("status").toInt()});
            }
            collect(child);
        }
    };
    collect(root);
    return images;
}

/******************************************************************************
* Verifies that the shell shows the icons of the shared icon set.
*
* Both frontends take their icons from the same pair of icon themes of the shared GUI layer (IconTheme), and the Qt Quick
* frontend reaches them through the image provider of QmlIcons. The check verifies both halves: that the shared rule
* resolves the icons the shell and its commands name, in either color scheme, and that the QML side really displays one.
******************************************************************************/
void runIconTest(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    declareCheckPhases({ "shared rule", "other theme", "qml provider", "displayed icons", "maximize switch",
                         "theme-dependent url" });

    // 1. The shared rule resolves the icons of the icon set itself and those the commands of the shell carry.
    const QString themeName = IconTheme::currentThemeName();
    const QString expectedTheme = IconTheme::themeName(GuiSettings::instance().usingDarkTheme());
    if(themeName != expectedTheme)
        reportVerificationFailure(QStringLiteral("the icon theme of the process is \"%1\" while the color scheme calls for \"%2\"")
            .arg(themeName, expectedTheme));

    QStringList iconPaths = {QStringLiteral("viewport_maximize"), QStringLiteral("viewport_restore")};
    for(const QString& commandId : {QStringLiteral("FileImport"), QStringLiteral("EditDelete"), QStringLiteral("ViewportMaximize")}) {
        if(Command* command = ui->actionManager() ? ui->actionManager()->findCommand(commandId) : nullptr) {
            if(!command->iconPath().isEmpty())
                iconPaths.push_back(command->iconPath());
        }
    }
    for(const QString& iconPath : iconPaths) {
        const QImage image = IconTheme::image(iconPath, QSize(16, 16), 1);
        if(image.isNull() || image.size() != QSize(16, 16))
            reportVerificationFailure(QStringLiteral("the shared icon set does not resolve \"%1\" (%2x%3 px)")
                .arg(iconPath).arg(image.size().width()).arg(image.size().height()));
    }

    reportCheckPhase("shared rule");

    // 2. The other theme of the set resolves them as well: both frontends follow the color scheme, so both themes have
    // to hold the icons the shell uses.
    IconTheme::apply(true);
    for(const QString& iconPath : iconPaths) {
        if(IconTheme::image(iconPath, QSize(16, 16), 1).isNull())
            reportVerificationFailure(QStringLiteral("the dark icon theme does not hold \"%1\"").arg(iconPath));
    }
    IconTheme::apply(GuiSettings::instance().usingDarkTheme());

    reportCheckPhase("other theme");



    // 3. The QML side: the singleton reports the theme of the shared layer and builds URLs of its image provider.
    const QVariant reportedTheme = evaluateInQml(ui, QStringLiteral("Icons.themeName"));
    if(reportedTheme.toString() != themeName)
        reportVerificationFailure(QStringLiteral("QML sees the icon theme \"%1\" while the process uses \"%2\"")
            .arg(reportedTheme.toString(), themeName));
    const QUrl restoreUrl = evaluateInQml(ui, QStringLiteral("Icons.url(\"viewport_restore\", Icons.themeName).toString()")).toString();
    const QUrl maximizeUrl = evaluateInQml(ui, QStringLiteral("Icons.url(\"viewport_maximize\", Icons.themeName).toString()")).toString();

    reportCheckPhase("qml provider");



    // 4. The icons of the shell are displayed: the maximize button of every pane shows the icon of the state of its pane,
    //    and no icon of the shell failed to load.
    QVector<IconImage> images = shellIconImages(ui, restoreUrl.host());
    int paneButtons = 0;
    for(const IconImage& image : images) {
        if(image.status != ImageStatusReady)
            reportVerificationFailure(QStringLiteral("the shell could not load its icon \"%1\"").arg(image.source.toString()));
        if(image.source == restoreUrl || image.source == maximizeUrl)
            paneButtons++;
    }
    if(images.isEmpty())
        reportVerificationFailure(QStringLiteral("the shell displays no icon of the shared icon set"));
    if(paneButtons == 0)
        reportVerificationFailure(QStringLiteral("the maximize button of no pane shows an icon of the shared icon set"));

    reportCheckPhase("displayed icons");



    // 5. Maximizing a viewport switches the button of that pane to the restore icon, which is the asset this frontend
    //    needed and the classic icon set did not have; the layout is put back before the check is done.
    if(!evaluateInQml(ui, QStringLiteral("viewportLayout.maximizable")).toBool()) {
        reportVerificationFailure(QStringLiteral("no viewport of the workbench can be maximized"));
        continuation();
        return;
    }
    evaluateInQml(ui, QStringLiteral("viewportLayout.toggleMaximize(viewportLayout.activeViewportIndex)"));

    pollUntil(ui, 50, 5000,
        [ui, restoreUrl]() {
            return std::ranges::any_of(shellIconImages(ui, restoreUrl.host()),
                [&restoreUrl](const IconImage& image) { return image.source == restoreUrl && image.status == ImageStatusReady; });
        },
        [ui, continuation = std::move(continuation), restoreUrl, themeName, iconPaths = iconPaths, paneButtons](bool restored) {

        if(!restored) {
            reportVerificationFailure(QStringLiteral("the maximize button of a maximized pane does not show the restore icon \"%1\"")
                .arg(restoreUrl.toString()));
        }
        evaluateInQml(ui, QStringLiteral("viewportLayout.toggleMaximize(viewportLayout.activeViewportIndex)"));

        reportCheckPhase("maximize switch");

    // 6. An icon reaches the scene graph as an Image, which caches its pixmap by URL - so the URL has to depend on
        //    the theme, or the shell would keep the icons of the old color scheme when the user switches between light
        //    and dark. (Whether the operating system reports such a switch is out of the hands of the shell; only the
        //    URL contract can be verified here.)
        const QString otherTheme = IconTheme::themeName(!GuiSettings::instance().usingDarkTheme());
        const QUrl otherThemeUrl = evaluateInQml(ui, QStringLiteral("Icons.url(\"viewport_restore\", \"%1\").toString()").arg(otherTheme)).toString();
        if(otherThemeUrl == restoreUrl)
            reportVerificationFailure(QStringLiteral("the URL of an icon does not depend on the theme, so an Image cannot refresh its pixmap when the color scheme changes"));

        qInfo() << "ICON_TEST theme" << themeName << "resolved" << iconPaths.size() << "icons," << "pane buttons"
                << paneButtons << "of which switched to the restore icon:" << restored
                << "| the URL of an icon follows the theme:" << (otherThemeUrl != restoreUrl);
        reportCheckPhase("theme-dependent url");
        continuation();
    });
}

/// Verifies the keyboard focus order of the shell: a keyboard user starts at the import control, and the chain continues
/// into the viewports - the pane that takes the focus is the one keyboard events of the viewport input modes arrive at.
///
/// The order itself can only be read from a window that has the keyboard focus: a window manager that never gave it to
/// the window (the headless macOS runner of the CI does not) leaves the tab chain degenerate, which says nothing about
/// the shell. The structural precondition - the import control and the viewports are part of the tab focus chain - is
/// therefore always verified, and the order is verified as well whenever the chain can be walked.
void verifyFocusOrder(QmlMainWindowUI* ui)
{
    QQuickView* view = ui->view();
    QQuickWindow* window = view;
    QQuickItem* rootObject = view ? view->rootObject() : nullptr;
    QQuickItem* importButton = rootObject ? rootObject->findChild<QQuickItem*>(QStringLiteral("importButton")) : nullptr;
    if(!rootObject || !importButton) {
        reportVerificationFailure(QStringLiteral("the shell has no import control to put the keyboard focus on"));
        return;
    }

    QVector<QQuickItem*> tabbable;
    std::function<void(QQuickItem*)> collectTabbable = [&collectTabbable, &tabbable](QQuickItem* item) {
        if(item->activeFocusOnTab())
            tabbable.push_back(item);
        const QList<QQuickItem*> children = item->childItems();
        for(QQuickItem* child : children)
            collectTabbable(child);
    };
    collectTabbable(rootObject);
    const bool importControlIsTabbable = tabbable.contains(importButton);
    const bool viewportIsTabbable = std::ranges::any_of(tabbable, [](QQuickItem* item) {
        return qobject_cast<QuickViewportItem*>(item) != nullptr;
    });

    // nextItemInFocusChain() follows the chain the scene declares, which is the order Tab walks.
    QQuickItem* first = window->contentItem()->nextItemInFocusChain();
    QVector<QQuickItem*> visited;
    for(QQuickItem* item = first; item && visited.size() < 32 && !visited.contains(item); item = item->nextItemInFocusChain())
        visited.push_back(item);
    const bool reachesViewport = std::ranges::any_of(visited, [](QQuickItem* item) {
        for(QQuickItem* parent = item; parent; parent = parent->parentItem())
            if(qobject_cast<QuickViewportItem*>(parent))
                return true;
        return false;
    });
    const bool chainIsWalkable = visited.size() > 1;

    qInfo() << "PARITY_TEST the keyboard focus chain holds" << visited.size() << "stop(s); the first one is the import"
            << "control:" << (first == importButton) << "and it reaches a viewport:" << reachesViewport
            << "| tabbable items of the shell:" << tabbable.size() << "; the import control:" << importControlIsTabbable
            << "and a viewport among them:" << viewportIsTabbable;
    if(!importControlIsTabbable)
        reportVerificationFailure(QStringLiteral("the import control of the shell cannot be reached with the Tab key"));
    if(!viewportIsTabbable)
        reportVerificationFailure(QStringLiteral("no viewport of the shell can be reached with the Tab key"));
    if(chainIsWalkable) {
        if(first != importButton)
            reportVerificationFailure(QStringLiteral("the first keyboard focus stop of the shell is not the import control"));
        if(!reachesViewport)
            reportVerificationFailure(QStringLiteral("the keyboard focus chain of the shell does not reach any viewport"));
    }
    else
        qInfo() << "PARITY_TEST the focus chain of the window cannot be walked in this environment (the window did not get"
                << "the keyboard focus), so the order is verified by the tabbable items above";
}

static /// Answers the next pipeline question of the workbench. Like the message dialog, the question blocks the main thread and
/// is answered from the timer of this poll, which the nested event loop keeps running.
void answerNextPipelineChoice(QmlMainWindowUI* ui, QmlWorkbenchController* controller, int index)
{
    pollUntil(ui, 50, 10000,
        [ui, controller]() {
            if(!controller->pipelineChoiceVisible())
                return false;
            // The question is presented by the QML scene, so the dialog has to be there while the frontend waits.
            QQuickItem* rootObject = ui->view() ? ui->view()->rootObject() : nullptr;
            QObject* dialog = rootObject ? rootObject->findChild<QObject*>(QStringLiteral("pipelineChooser")) : nullptr;
            if(dialog == nullptr || !dialog->property("visible").toBool())
                reportVerificationFailure(QStringLiteral("the pipeline question is not visible in the workbench window"));
            return true;
        },
        [controller, index](bool appeared) {
            if(appeared)
                controller->answerPipelineChoice(index);
            else
                reportVerificationFailure(QStringLiteral("the workbench did not ask which pipeline to keep"));
        });
}

static /// Verifies the question the workbench asks when a session holds several pipelines. A session file with two file source
/// pipelines cannot be produced by this frontend (OVITO Basic imports one pipeline at a time, which is the reason the
/// question exists), so the check asks the question itself and answers it in both ways - the same way the check of the
/// task list fabricates the tasks it displays.
void verifyPipelineChooser(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    QmlWorkbenchController* controller = ui->workbenchController();
    const QStringList pipelines{ QStringLiteral("first.xyz [XYZ]"), QStringLiteral("second.xyz [XYZ]") };

    // Picking the second pipeline hands that index back to the caller that is waiting on the answer.
    answerNextPipelineChoice(ui, controller, 1);
    const int picked = controller->choosePipeline(pipelines);
    if(picked != 1)
        reportVerificationFailure(QStringLiteral("the pipeline question answered with index 1 returned %1").arg(picked));

    // Aborting the question returns no index, which makes the caller refuse to load the session.
    answerNextPipelineChoice(ui, controller, -1);
    const int aborted = controller->choosePipeline(pipelines);
    if(aborted != -1)
        reportVerificationFailure(QStringLiteral("an aborted pipeline question returned %1 instead of -1").arg(aborted));

    if(controller->pipelineChoiceVisible())
        reportVerificationFailure(QStringLiteral("the pipeline question stayed visible after it was answered"));
    if(controller->pipelineChoiceItems().size() != pipelines.size())
        reportVerificationFailure(QStringLiteral("the pipeline question lists %1 pipelines instead of %2")
            .arg(controller->pipelineChoiceItems().size()).arg(pipelines.size()));

    qInfo() << "PARITY_TEST the pipeline question of a session with several pipelines is presented by the workbench and"
            << "both answers reach the caller that is waiting for them";
    continuation();
}

static /// Verifies that the File menu offers the recent files of the shared list and that the file selection dialog reopens
/// where the last import was - two places where the shell has to use the shared layer rather than keep its own state.
void verifyRecentFiles(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    QmlWorkbenchController* controller = ui->workbenchController();
    const QList<RecentFilesList::Entry>& entries = RecentFilesList::instance().entries();
    if(entries.isEmpty()) {
        reportVerificationFailure(QStringLiteral("the run imported and saved several files, but the recent files list is empty"));
    }
    else {
        const QVariantList shellList = controller->recentFiles();
        if(shellList.size() != entries.size())
            reportVerificationFailure(QStringLiteral("the shell reports %1 recent files while the shared list holds %2")
                .arg(shellList.size()).arg(entries.size()));

        const QString expectedTitle = entries.front().urls.front().toDisplayString(QUrl::PreferLocalFile | QUrl::NormalizePathSegments);
        const QString shellTitle = shellList.isEmpty() ? QString() : shellList.front().toMap().value(QStringLiteral("title")).toString();
        if(shellTitle != expectedTitle)
            reportVerificationFailure(QStringLiteral("the shell lists \"%1\" as the most recent file instead of \"%2\"").arg(shellTitle, expectedTitle));
        if(!shellList.isEmpty() && shellList.front().toMap().value(QStringLiteral("isSession")).toBool())
            reportVerificationFailure(QStringLiteral("the shell marks a session file of the recent files list as a data file"));

        // The menu is built from that list, so it has to hold one entry per file.
        QQuickItem* rootObject = ui->view() ? ui->view()->rootObject() : nullptr;
        QObject* submenu = rootObject ? rootObject->findChild<QObject*>(QStringLiteral("recentFilesMenu")) : nullptr;
        if(submenu == nullptr) {
            reportVerificationFailure(QStringLiteral("the File menu of the workbench has no recent files submenu"));
        }
        else {
            // The entries are the delegates of the Repeater of the submenu (a menu's items are not its QObject
            // children when they are created by a Repeater), so they are read through the Repeater itself.
            QObject* repeater = submenu->findChild<QObject*>(QStringLiteral("recentFilesRepeater"));
            int items = 0;
            if(repeater == nullptr) {
                reportVerificationFailure(QStringLiteral("the recent files submenu has no repeater to build its entries"));
            }
            else {
                const int count = repeater->property("count").toInt();
                for(int i = 0; i < count; i++) {
                    QQuickItem* entry = nullptr;
                    if(!QMetaObject::invokeMethod(repeater, "itemAt", Q_RETURN_ARG(QQuickItem*, entry), Q_ARG(int, i)) || entry == nullptr)
                        continue;
                    items++;
                    const QString text = entry->property("text").toString();
                    if(text.isEmpty())
                        reportVerificationFailure(QStringLiteral("the recent files submenu holds an entry without a label"));
                    else if(!entry->property("enabled").toBool())
                        reportVerificationFailure(QStringLiteral("the recent files submenu offers \"%1\" although the list is not empty").arg(text));
                }
            }
            if(items != entries.size())
                reportVerificationFailure(QStringLiteral("the recent files submenu lists %1 entries while the shared list holds %2")
                    .arg(items).arg(entries.size()));
        }
    }

    // The import dialog starts in the directory of the last import, which is the history the shared GuiSettings keeps and
    // the classic frontend's file dialog reads as well.
    const QStringList importDirectories = GuiSettings::instance().recentDirectories(QStringLiteral("import"));
    if(importDirectories.isEmpty()) {
        reportVerificationFailure(QStringLiteral("the imported file did not leave its directory in the file dialog history"));
    }
    else if(controller->importDirectoryUrl() != QUrl::fromLocalFile(importDirectories.front())) {
        reportVerificationFailure(QStringLiteral("the file selection dialog would open in \"%1\" instead of in \"%2\"")
            .arg(controller->importDirectoryUrl().toLocalFile(), importDirectories.front()));
    }
    else {
        qInfo() << "PARITY_TEST the File menu lists" << entries.size() << "recent file(s) and the file selection dialog"
                << "reopens in" << importDirectories.front();
    }

    continuation();
}

/// Verifies the features the shell gained for parity with the classic frontend: the menus of the workbench present the
/// shared commands, the viewport has a context menu, the status line lists the running tasks, the window state is
/// remembered and an import reports what it did with the file.
void runParityTest(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    declareCheckPhases({ "menu bar", "focus order", "context menu", "about dialog", "pipeline chooser", "task rows",
                         "window state", "import notice", "recent files" });

    QQuickItem* rootObject = ui->view() ? ui->view()->rootObject() : nullptr;
    verifyMenuEntries(rootObject ? rootObject->findChild<QObject*>(QStringLiteral("workbenchMenuBar")) : nullptr,
        QStringLiteral("menu bar of the workbench"), false);
    reportCheckPhase("menu bar");
    verifyFocusOrder(ui);
    reportCheckPhase("focus order");

    verifyContextMenu(ui, [ui, continuation]() {
        reportCheckPhase("context menu");
        verifyAboutDialog(ui, [ui, continuation]() {
            reportCheckPhase("about dialog");
            verifyPipelineChooser(ui, [ui, continuation]() {
                reportCheckPhase("pipeline chooser");
            verifyTaskRows(ui, [ui, continuation]() {
                reportCheckPhase("task rows");
                verifyWindowState(ui, [ui, continuation]() {
                    reportCheckPhase("window state");
                    verifyImportNotice(ui, [ui, continuation]() {
                        reportCheckPhase("import notice");
                        verifyRecentFiles(ui, [continuation]() {
                            reportCheckPhase("recent files");
                            continuation();
                        });
                    });
                });
            });
            });
        });
    });
}

/// Verifies the viewport area of the workbench shell: the panes come from the layout tree of the dataset, dragging a
/// handle resizes them through the undo system, and maximizing a viewport keeps exactly one pane visible.
void runLayoutTest(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    declareCheckPhases({ "panes", "drag and undo", "maximize" });

    QmlViewportLayout* layout = ui->viewportLayout();
    QQuickView* view = ui->view();
    QQuickWindow* window = view;
    // The viewport area is the item the layout model places the panes in; its coordinates are the ones the handles
    // report, which is why the mouse events are translated into window coordinates through it.
    QQuickItem* hostItem = view && view->rootObject() ?
        view->rootObject()->findChild<QQuickItem*>(QStringLiteral("viewportHost")) : nullptr;
    if(!layout || !window || !hostItem) {
        reportVerificationFailure(QStringLiteral("layout check: the workbench exposes no viewport area"));
        continuation();
        return;
    }

    reportCameras(ui, QStringLiteral("in the layout check"));

    const QVariantList panes = layout->panes();
    const QVariantList splitters = layout->splitters();
    qInfo() << "LAYOUT_TEST the viewport area holds" << panes.size() << "panes and" << splitters.size() << "handles"
            << "in an area of" << hostItem->size();
    for(QuickViewportItem* item : viewportItems(ui)) {
        qInfo() << "LAYOUT_ITEM" << item->size() << item->mapToScene(QPointF(0, 0)) << "visible=" << item->isVisible()
                << "viewportWindow=" << (item->viewportWindow() != nullptr);
    }
    for(int index = 0; index < panes.size(); index++) {
        if(auto* pane = qobject_cast<QmlViewportPane*>(panes[index].value<QObject*>())) {
            qInfo() << "LAYOUT_PANE" << index << QStringLiteral("rect=(%1,%2 %3x%4)").arg(pane->x()).arg(pane->y()).arg(pane->width()).arg(pane->height())
                    << "active=" << pane->isActive() << "maximized=" << pane->isMaximized() << "visible=" << pane->isVisible();
        }
    }
    for(int index = 0; index < splitters.size(); index++) {
        if(auto* splitter = qobject_cast<QmlViewportSplitter*>(splitters[index].value<QObject*>())) {
            qInfo() << "LAYOUT_SPLITTER" << index << (splitter->isHorizontal() ? "horizontal" : "vertical")
                    << QStringLiteral("rect=(%1,%2 %3x%4)").arg(splitter->x()).arg(splitter->y()).arg(splitter->width()).arg(splitter->height());
        }
    }

    // The panes must add up to the layout tree of the dataset and must not overlap the handles.
    if(panes.size() < 2 || splitters.isEmpty()) {
        reportVerificationFailure(QStringLiteral("layout check: the default viewport layout should have several panes and handles"));
        continuation();
        return;
    }

    reportCheckPhase("panes");

    const QString before = layoutSnapshot(layout);

    // Drag the first handle with synthetic mouse events, i.e. through the QML mouse area a user would grab - not by
    // calling the layout model directly, so that the QML wiring is part of what is verified.
    auto* handle = qobject_cast<QmlViewportSplitter*>(splitters.front().value<QObject*>());
    const QPointF handleCenter(handle->x() + 0.5 * handle->width(), handle->y() + 0.5 * handle->height());
    const QPointF dragOffset = handle->isHorizontal() ? QPointF(100, 0) : QPointF(0, 100);
    const auto sendMouseEvent = [ui, window, hostItem](QEvent::Type type, Qt::MouseButton button, Qt::MouseButtons buttons, const QPointF& panelPosition) {
        const QPointF windowPosition = hostItem->mapToScene(panelPosition);
        QMouseEvent event(type, windowPosition, window->mapToGlobal(windowPosition), button, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &event);
    };
    sendMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton, handleCenter);
    sendMouseEvent(QEvent::MouseMove, Qt::NoButton, Qt::LeftButton, handleCenter + 0.5 * dragOffset);
    sendMouseEvent(QEvent::MouseMove, Qt::NoButton, Qt::LeftButton, handleCenter + dragOffset);
    sendMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton, handleCenter + dragOffset);

    const QString afterDrag = layoutSnapshot(layout);
    if(afterDrag == before)
        reportVerificationFailure(QStringLiteral("layout check: dragging a handle did not change the pane sizes"));

    // One drag is one undoable operation, and undoing it has to restore the exact pane sizes.
    UndoStack* undoStack = ui->undoStack();
    if(!undoStack || !undoStack->canUndo())
        reportVerificationFailure(QStringLiteral("layout check: the drag was not recorded as an undoable operation"));
    else
        qInfo() << "LAYOUT_UNDO the drag produced the undo step" << undoStack->undoText();

    ui->actionManager()->triggerCommand(ACTION_EDIT_UNDO);
    if(layoutSnapshot(layout) != before)
        reportVerificationFailure(QStringLiteral("layout check: undo did not restore the pane sizes of the drag"));
    ui->actionManager()->triggerCommand(ACTION_EDIT_REDO);
    if(layoutSnapshot(layout) != afterDrag)
        reportVerificationFailure(QStringLiteral("layout check: redo did not restore the dragged pane sizes"));
    ui->actionManager()->triggerCommand(ACTION_EDIT_UNDO);
    if(layoutSnapshot(layout) != before)
        reportVerificationFailure(QStringLiteral("layout check: a second undo did not return to the layout from before the drag"));
    qInfo() << "LAYOUT_UNDO the drag was undone and redone with the pane sizes restored";
    reportCheckPhase("drag and undo");

    // Maximizing keeps exactly one pane visible and restores the layout afterwards.
    const int activeIndex = layout->activeViewportIndex();
    QString visiblePanes;
    layout->toggleMaximize(activeIndex);
    layoutSnapshot(layout, &visiblePanes);
    if(visiblePanes != QStringLiteral("1"))
        reportVerificationFailure(QStringLiteral("layout check: maximizing left %1 panes visible").arg(visiblePanes));
    else
        qInfo() << "LAYOUT_MAXIMIZE viewport" << activeIndex << "fills the viewport area while the other panes are hidden";
    layout->toggleMaximize(activeIndex);
    if(layoutSnapshot(layout) != before)
        reportVerificationFailure(QStringLiteral("layout check: restoring the layout did not bring the pane sizes back"));

    reportCheckPhase("maximize");
    continuation();
}


/******************************************************************************
* Verifies the shared command layer of the two frontends.

*
* The commands are the frontend-neutral description of everything the user can invoke, and the classic frontend
* presents them as QActions. This check verifies that the QML workbench sees the same commands with the same state,
* that the state rules of the frontend (the undo stack, the animation playback, the viewport layout) drive the
* commands, and that a command can be invoked through the QML engine.
******************************************************************************/
void runCommandTest(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    // The numbered sections of this check, in the order they run. A section that is skipped for a static scene still
    // walks its phase, which is what "the check finished" means.
    declareCheckPhases({ "catalog", "qml state", "qml invocation", "undo stack", "playback", "input modes", "maximize" });

    ActionManager* actionManager = ui->actionManager();
    QQuickView* view = ui->view();
    if(!actionManager || !view || !view->rootObject()) {
        reportVerificationFailure(QStringLiteral("command check: the workbench exposes no command layer"));
        continuation();
        return;
    }

    // 1. Every command the classic frontend knows about is available here, and its QAction view carries the state of
    //    the command, which is what the menus and toolbars of the classic frontend display.
    const QStringList ids = {
        QStringLiteral("EditUndo"), QStringLiteral("EditRedo"), QStringLiteral("EditDelete"),
        QStringLiteral("ViewportMaximize"), QStringLiteral("ViewportZoomSceneExtents"),
        QStringLiteral("ViewportPan"), QStringLiteral("SelectionMode"), QStringLiteral("AnimationTogglePlayback")
    };
    for(const QString& id : ids) {
        Command* command = actionManager->findCommand(id);
        if(!command) {
            reportVerificationFailure(QStringLiteral("command check: the command %1 does not exist").arg(id));
            continue;
        }
        QAction* actionView = actionManager->actionView(command);
        if(!actionView) {
            reportVerificationFailure(QStringLiteral("command check: the command %1 has no QAction view").arg(id));
            continue;
        }
        if(actionView->text() != command->text() || actionView->isEnabled() != command->isEnabled()
           || actionView->isCheckable() != command->isCheckable() || actionView->isChecked() != command->isChecked()) {
            reportVerificationFailure(QStringLiteral("command check: the QAction view of %1 does not mirror the command").arg(id));
        }
    }
    qInfo() << "COMMAND_TEST the command layer provides" << actionManager->commands().size() << "commands,"
            << "including all" << ids.size() << "commands of this check";

    reportCheckPhase("catalog");

    // 2. The QML side of the workbench sees the same objects, and can read the state of a command. The expressions are
    //    evaluated by the QML engine, so the context property, the registered type and the properties are all covered.
    Command* undoCommand = actionManager->findCommand(QStringLiteral("EditUndo"));
    const QVariant qmlUndoText = evaluateInQml(ui, QStringLiteral("workbench.undoCommand.text"));
    // The title of the Undo command carries the name of the operation it would revert, so only compare the prefix.
    if(!qmlUndoText.toString().startsWith(QStringLiteral("Undo")))
        reportVerificationFailure(QStringLiteral("command check: QML reads \"%1\" as the title of the Undo command").arg(qmlUndoText.toString()));
    const QVariant qmlCommandCount = evaluateInQml(ui, QStringLiteral("commandManager.commandList.length"));
    if(qmlCommandCount.toInt() != actionManager->commands().size())
        reportVerificationFailure(QStringLiteral("command check: QML sees %1 of %2 commands").arg(qmlCommandCount.toInt()).arg(actionManager->commands().size()));
    qInfo() << "COMMAND_TEST QML reads the Undo command as" << qmlUndoText.toString() << "and sees" << qmlCommandCount.toInt() << "commands";

    reportCheckPhase("qml state");

    // 3. Invoking a command through the QML engine has to run the handler the two frontends share.
    ViewportConfiguration* viewportConfig = ui->datasetContainer().activeViewportConfig();
    if(viewportConfig) {
        evaluateInQml(ui, QStringLiteral("commandManager.triggerCommand('ViewportMaximize')"));
        if(viewportConfig->maximizedViewport() != viewportConfig->activeViewport())
            reportVerificationFailure(QStringLiteral("command check: invoking a command through QML did not run its handler"));
        evaluateInQml(ui, QStringLiteral("commandManager.triggerCommand('ViewportMaximize')"));
        if(viewportConfig->maximizedViewport())
            reportVerificationFailure(QStringLiteral("command check: invoking a command through QML did not restore the layout"));
        qInfo() << "COMMAND_TEST QML can invoke a command and run its handler";
    }

    reportCheckPhase("qml invocation");

    // 4. The state rules of the frontend drive the commands. The undo stack is the most important one: its state has to
    //    reach the Undo/Redo commands (and their QAction views) without the frontend wiring them up again.
    Scene* scene = ui->datasetContainer().activeScene();
    SceneNode* node = scene && !scene->children().empty() ? scene->children().front() : nullptr;
    if(!node) {
        reportVerificationFailure(QStringLiteral("command check: the scene has no object to rename"));
    }
    else {
        const QString originalTitle = node->objectTitle();
        GuiTaskScope taskScope(*ui);
        ui->performTransaction(QStringLiteral("Rename pipeline"), [&]() {
            node->setSceneNodeName(QStringLiteral("renamed by the command check"));
        });
        if(node->objectTitle() != QStringLiteral("renamed by the command check"))
            reportVerificationFailure(QStringLiteral("command check: the test transaction did not rename the object"));
        Command* redoCommand = actionManager->findCommand(QStringLiteral("EditRedo"));
        if(!undoCommand || !undoCommand->isEnabled() || !actionManager->actionView(undoCommand)->isEnabled())
            reportVerificationFailure(QStringLiteral("command check: the Undo command did not become enabled after an operation"));
        else
            qInfo() << "COMMAND_TEST the Undo command is" << undoCommand->text();
        actionManager->triggerCommand(QStringLiteral("EditUndo"));
        if(node->objectTitle() != originalTitle)
            reportVerificationFailure(QStringLiteral("command check: the Undo command did not revert the operation"));
        if(!redoCommand || !redoCommand->isEnabled())
            reportVerificationFailure(QStringLiteral("command check: the Redo command did not become enabled after an undo"));
        actionManager->triggerCommand(QStringLiteral("EditRedo"));
        if(node->objectTitle() != QStringLiteral("renamed by the command check"))
            reportVerificationFailure(QStringLiteral("command check: the Redo command did not reapply the operation"));
        actionManager->triggerCommand(QStringLiteral("EditUndo"));
        qInfo() << "COMMAND_TEST the shared undo stack drives the Undo/Redo commands and their QAction views";
    }

    reportCheckPhase("undo stack");

    // 5. A checkable command that mirrors program state: starting and stopping the animation playback goes through the
    //    command, which is what the menu entry of the classic frontend toggles. A playback needs an animation with more
    //    than one frame, so the probe is skipped for a static scene instead of importing data here: this step must leave
    //    the scene as it found it, and the playback is probed where a trajectory is imported anyway (runImportTest()).
    {
        const AnimationSettings* animSettings = ui->datasetContainer().activeAnimationSettings();
        Command* playbackCommand = actionManager->findCommand(QStringLiteral("AnimationTogglePlayback"));
        if(!animSettings || animSettings->isSingleFrame()) {
            qInfo() << "COMMAND_TEST (skipped) the scene holds no animation, so the playback command cannot be probed";
        }
        else if(playbackCommand) {
            probeAnimationPlaybackCommand(ui, playbackCommand);
        }
    }

    reportCheckPhase("playback");

    // 6. Viewport input modes are commands as well, including the rule that an exclusive mode stays active.
    ViewportInputManager* inputManager = ui->viewportInputManager();
    Command* panCommand = actionManager->findCommand(QStringLiteral("ViewportPan"));
    Command* selectionCommand = actionManager->findCommand(QStringLiteral("SelectionMode"));
    if(!inputManager || !panCommand || !selectionCommand) {
        reportVerificationFailure(QStringLiteral("command check: the viewport input modes are not available as commands"));
    }
    else {
        panCommand->trigger();
        if(inputManager->activeMode() != inputManager->panMode() || !panCommand->isChecked())
            reportVerificationFailure(QStringLiteral("command check: invoking the pan command did not activate the pan mode"));
        panCommand->trigger();
        if(inputManager->activeMode() == inputManager->panMode() || panCommand->isChecked())
            reportVerificationFailure(QStringLiteral("command check: invoking the pan command again did not deactivate the mode"));
        selectionCommand->trigger();
        if(inputManager->activeMode() != inputManager->selectionMode() || !selectionCommand->isChecked())
            reportVerificationFailure(QStringLiteral("command check: invoking the selection command did not activate the selection mode"));
        // The selection mode is exclusive: the user cannot turn it off, so it stays active and stays checked.
        selectionCommand->trigger();
        if(inputManager->activeMode() != inputManager->selectionMode() || !selectionCommand->isChecked())
            reportVerificationFailure(QStringLiteral("command check: the exclusive selection mode was deactivated"));
        qInfo() << "COMMAND_TEST the viewport input modes are commands, including the exclusive selection mode";
    }

    reportCheckPhase("input modes");

    // 7. Maximizing the active viewport is the same command the classic frontend has in its toolbar.
    if(Command* maximizeCommand = actionManager->findCommand(QStringLiteral("ViewportMaximize"))) {
        ViewportConfiguration* config = ui->datasetContainer().activeViewportConfig();
        if(config) {
            maximizeCommand->trigger();
            if(config->maximizedViewport() != config->activeViewport() || !maximizeCommand->isChecked())
                reportVerificationFailure(QStringLiteral("command check: the maximize command did not maximize the active viewport"));
            maximizeCommand->trigger();
            if(config->maximizedViewport() || maximizeCommand->isChecked())
                reportVerificationFailure(QStringLiteral("command check: the maximize command did not restore the layout"));
            qInfo() << "COMMAND_TEST the maximize command toggles the maximized viewport";
        }
    }

    reportCheckPhase("maximize");
    continuation();
}

/******************************************************************************
* Verifies the settings facade the two workbenches share.
*
* Both frontends persist the same things - the color scheme, the window state, how the file dialogs behave - and have to
* agree on the key, the default and the meaning of each value, or the classic settings dialog and the Qt Quick shell drift
* apart. This check covers that the QML scene really reaches the facade, that the shell's theme takes its palette from it,
* and that every accessor round-trips through the settings store.
*
* Every value this check writes is written back before it returns, so a verification run does not change how the
* developer's frontends behave; only the history of a file dialog class that exists solely for this check is left behind.
* Run the spike with an isolated XDG_CONFIG_HOME (docs/design/UI_TEST_ENV.md) for a fully clean run.
******************************************************************************/
void runSettingsTest(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    declareCheckPhases({ "facade and theme", "window state", "window blobs", "file dialogs", "first-start flags",
                         "round trip" });

    GuiSettings& settings = GuiSettings::instance();

    // 1. The QML scene reaches the same facade object, and the theme of the shell resolves to it. Both values are read
    //    through the QML engine, which covers the context property, the property bindings and the palette binding.
    const bool cppDarkTheme = settings.usingDarkTheme();
    const QVariant qmlDarkTheme = evaluateInQml(ui, QStringLiteral("guiSettings.usingDarkTheme"));
    const QVariant qmlThemeDark = evaluateInQml(ui, QStringLiteral("workbench.darkTheme"));
    if(qmlDarkTheme.toBool() != cppDarkTheme)
        reportVerificationFailure(QStringLiteral("settings check: QML reads %1 as the color scheme of the shared settings while C++ reads %2")
            .arg(qmlDarkTheme.toBool()).arg(cppDarkTheme));
    if(qmlThemeDark.toBool() != cppDarkTheme)
        reportVerificationFailure(QStringLiteral("settings check: the shell's theme resolved to %1 while the shared settings report %2 - the theme does not follow the shared policy")
            .arg(qmlThemeDark.toBool()).arg(cppDarkTheme));

    reportCheckPhase("facade and theme");


    // 2. The window placement of a frontend without widgets. The Qt Quick shell restores its window from these, which the
    //    classic frontend does not use (it stores QWidget geometry blobs instead).
    auto expect = [](const QString& what, const QVariant& actual, const QVariant& expected) {
        if(actual != expected)
            reportVerificationFailure(QStringLiteral("settings check: %1 is '%2' after writing '%3'")
                .arg(what, actual.toString(), expected.toString()));
    };

    const QRect originalWindowGeometry = settings.workbenchWindowGeometry();
    settings.setWorkbenchWindowGeometry(QRect(120, 80, 1024, 768));
    expect(QStringLiteral("the remembered window rectangle"), settings.workbenchWindowGeometry(), QRect(120, 80, 1024, 768));
    settings.setWorkbenchWindowGeometry(originalWindowGeometry);
    expect(QStringLiteral("the window rectangle after restoring the original"), settings.workbenchWindowGeometry(), originalWindowGeometry);

    const bool originalMaximized = settings.isWorkbenchWindowMaximized();
    settings.setWorkbenchWindowMaximized(!originalMaximized);
    expect(QStringLiteral("the maximized flag"), settings.isWorkbenchWindowMaximized(), !originalMaximized);
    settings.setWorkbenchWindowMaximized(originalMaximized);
    expect(QStringLiteral("the maximized flag after restoring the original"), settings.isWorkbenchWindowMaximized(), originalMaximized);

    reportCheckPhase("window state");


    // 3. The opaque blobs of the classic main window. They are not converted into rectangles on purpose: a QWidget
    //    geometry blob also carries the screen and the maximized state of the window.
    const QByteArray originalGeometry = settings.mainWindowGeometry();
    const QByteArray originalState = settings.mainWindowState();
    settings.setMainWindowGeometry(QByteArrayLiteral("spike-geometry-blob"));
    settings.setMainWindowState(QByteArrayLiteral("spike-state-blob"));
    expect(QStringLiteral("the main window geometry blob"), settings.mainWindowGeometry(), QByteArrayLiteral("spike-geometry-blob"));
    expect(QStringLiteral("the main window layout blob"), settings.mainWindowState(), QByteArrayLiteral("spike-state-blob"));
    settings.setMainWindowGeometry(originalGeometry);
    settings.setMainWindowState(originalState);

    reportCheckPhase("window blobs");


    // 4. The behavior of the file dialogs, which both frontends follow.
    const bool originalKeepHistory = settings.keepDirectoryHistory();
    const bool originalPreferQt = settings.preferQtFileDialog();
    const QString originalSessionDirectory = settings.sessionFileDirectory();
    settings.setKeepDirectoryHistory(!originalKeepHistory);
    settings.setPreferQtFileDialog(!originalPreferQt);
    settings.setSessionFileDirectory(QStringLiteral("/tmp/ovito-settings-check"));
    expect(QStringLiteral("the directory-history flag"), settings.keepDirectoryHistory(), !originalKeepHistory);
    expect(QStringLiteral("the preferred file dialog"), settings.preferQtFileDialog(), !originalPreferQt);
    expect(QStringLiteral("the session file directory"), settings.sessionFileDirectory(), QStringLiteral("/tmp/ovito-settings-check"));
    settings.setKeepDirectoryHistory(originalKeepHistory);
    settings.setPreferQtFileDialog(originalPreferQt);
    settings.setSessionFileDirectory(originalSessionDirectory);
    expect(QStringLiteral("the directory-history flag after restoring the original"), settings.keepDirectoryHistory(), originalKeepHistory);
    expect(QStringLiteral("the preferred file dialog after restoring the original"), settings.preferQtFileDialog(), originalPreferQt);
    expect(QStringLiteral("the session file directory after restoring the original"), settings.sessionFileDirectory(), originalSessionDirectory);

    // The directory history of one kind of file dialog: the most recent directory moves to the front, and an empty
    // directory name is ignored. This uses a dialog class of its own, so no real history is touched.
    const QString dialogClass = QStringLiteral("spike-settings-check");
    settings.rememberDirectory(dialogClass, QStringLiteral("/tmp/ovito-check-a"));
    settings.rememberDirectory(dialogClass, QStringLiteral("/tmp/ovito-check-b"));
    expect(QStringLiteral("the directory history"), settings.recentDirectories(dialogClass), QStringList{QStringLiteral("/tmp/ovito-check-b")});
    settings.rememberDirectory(dialogClass, QStringLiteral("/tmp/ovito-check-a"));
    expect(QStringLiteral("the directory history after returning to the first directory"), settings.recentDirectories(dialogClass), QStringList{QStringLiteral("/tmp/ovito-check-a")});

    reportCheckPhase("file dialogs");


    // 5. The flags the first start sets: the classic frontend shows its GPU adapter dialog while this one is not set.
    const bool originalSetupDone = settings.graphicsAdapterSetupDone();
    settings.setGraphicsAdapterSetupDone(true);
    expect(QStringLiteral("the graphics-adapter setup flag"), settings.graphicsAdapterSetupDone(), true);
    settings.setGraphicsAdapterSetupDone(false);
    expect(QStringLiteral("the graphics-adapter setup flag after clearing it"), settings.graphicsAdapterSetupDone(), false);
    settings.setGraphicsAdapterSetupDone(originalSetupDone);

#ifdef OVITO_BUILD_PROFESSIONAL
    // The multi-file import mode is a setting of the professional edition only; elsewhere it is fixed.
    const FileImporter::MultiFileImportMode originalImportMode = settings.multiFileImportMode();
    settings.setMultiFileImportMode(FileImporter::ImportAsSeparateObjects);
    expect(QStringLiteral("the multi-file import mode"), static_cast<int>(settings.multiFileImportMode()), static_cast<int>(FileImporter::ImportAsSeparateObjects));
    settings.setMultiFileImportMode(originalImportMode);
#endif

    // The color-scheme policy itself: following the system is compulsory on Linux and macOS and a user decision
    // elsewhere, which is where the check can exercise the stored flag.
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    if(!settings.followsSystemColorScheme())
        reportVerificationFailure(QStringLiteral("settings check: following the system color scheme is not compulsory on this platform"));
    qInfo() << "SETTINGS_TEST the color scheme always follows the system on this platform, dark theme is" << cppDarkTheme;
#else
    const bool originalFollowSystem = settings.followsSystemColorScheme();
    settings.setFollowsSystemColorScheme(!originalFollowSystem);
    expect(QStringLiteral("the automatic color-scheme flag"), settings.followsSystemColorScheme(), !originalFollowSystem);
    settings.setFollowsSystemColorScheme(originalFollowSystem);
    qInfo() << "SETTINGS_TEST the automatic color-scheme flag round-trips, dark theme is" << cppDarkTheme;
#endif

    // Reported outside the platform branch above: the phases of a check are the same on every platform, so a phase that
    // is only exercised on some of them is still reported there.
    reportCheckPhase("first-start flags");

    qInfo() << "SETTINGS_TEST the shared settings facade round-trips the window state, the file dialog behaviour and"
            << "the first-start flags; the shell's theme follows it";
    reportCheckPhase("round trip");
    continuation();
}

static /// Verifies the session workflow of the workbench, which both frontends share (WorkbenchUI::saveSessionFile(),
/// saveSession(), loadSessionFile() and isSessionModified()): saving a session writes a file and clears the modified
/// state, a change to the scene marks the session as modified, and loading the session brings the saved content back.
///
/// The file dialog in front of the workflow is frontend specific (the classic main window shows a QFileDialog, the Qt
/// Answers the next message dialog of the workbench, so that an unattended run never waits for a user. The dialog of a
/// verification step that blocks the main thread is answered from the timer of this poll, which the nested event loop of
/// the dialog keeps running.
void answerNextMessageBox(QmlMainWindowUI* ui, QmlWorkbenchController* controller, UserInterface::MessageBoxButton button)
{
    pollUntil(ui, 50, 10000,
        [controller]() { return controller->messageBoxVisible(); },
        [controller, button](bool appeared) {
            if(appeared)
                controller->answerMessageBox(static_cast<int>(button));
            else
                reportVerificationFailure(QStringLiteral("the workbench did not ask about the modified session"));
        });
}

/// Answers the file selection dialog of the session workflow with the given file as soon as the workbench asks for one.
/// An empty file cancels the dialog. The shared session workflow blocks in the dialog, so the answer has to arrive from
/// an event loop turn - which is what this helper arranges, the same way answerNextMessageBox() answers the question
/// about unsaved changes.
void answerNextFileDialog(QmlMainWindowUI* ui, QmlWorkbenchController* controller, const QString& filePath)
{
    pollUntil(ui, 50, 10000,
        [controller]() { return controller->fileDialogVisible(); },
        [controller, filePath](bool appeared) {
            if(!appeared) {
                reportVerificationFailure(QStringLiteral("the workbench did not ask for a session state file"));
                return;
            }
            // Asking is not presenting: the scene has to have opened the dialog it is waiting on.
            if(!controller->fileDialogPresented())
                reportVerificationFailure(QStringLiteral("the workbench asked for a session state file without presenting the file dialog"));
            if(filePath.isEmpty())
                controller->cancelFileDialog();
            else
                controller->answerFileDialog(QUrl::fromLocalFile(filePath));
        });
}

/// Quick shell has no session commands yet), so this check drives the operations that do not need a file name.
void runSessionTest(QmlMainWindowUI* ui, std::function<void()> continuation)
{
    // The phases of the session workflow, in the order its numbered sections run.
    declareCheckPhases({ "save", "modified state", "reload", "recent files", "file dialog", "unwritable save",
                         "open through the dialog", "working directory", "close question", "recent files reopened" });

    const QString sessionFile = QDir::tempPath() + QStringLiteral("/ovito-qml-session-test/session.ovito");
    QDir().mkpath(QFileInfo(sessionFile).absolutePath());
    QFile::remove(sessionFile);

    // The check saves the scene of the workbench, so there has to be one: an earlier step may have canceled an import
    // and left the scene empty.
    {
        GuiTaskScope taskScope(*ui);
        if(Scene* scene = ui->datasetContainer().activeScene(); !scene || scene->children().empty()) {
            const QString dataFile = QDir::tempPath() + QStringLiteral("/ovito-qml-session-test/lattice.xyz");
            if(!writeLatticeFile(dataFile, 8, 3.6).isEmpty())
                ui->workbenchController()->importFiles(QVariantList{ QUrl::fromLocalFile(dataFile) });
        }
    }

    Scene* scene = ui->datasetContainer().activeScene();
    SceneNode* node = scene && !scene->children().empty() ? scene->children().front() : nullptr;
    if(!node) {
        reportVerificationFailure(QStringLiteral("session check: the scene holds no object to save"));
        continuation();
        return;
    }

    GuiTaskScope taskScope(*ui);
    QmlWorkbenchController* controller = ui->workbenchController();
    ui->handleExceptions([&]() {
        // 1. Saving a session writes the file, remembers it and clears the modified state.
        ui->saveSessionFile(sessionFile);
        if(!QFileInfo::exists(sessionFile))
            reportVerificationFailure(QStringLiteral("session check: saving wrote no file"));
        if(ui->sessionFilePath() != QFileInfo(sessionFile).absoluteFilePath())
            reportVerificationFailure(QStringLiteral("session check: the workbench does not remember the file it saved (%1)").arg(ui->sessionFilePath()));
        if(ui->isSessionModified())
            reportVerificationFailure(QStringLiteral("session check: a freshly saved session is reported as modified"));
        qInfo() << "SESSION_TEST saved the scene to" << QFileInfo(sessionFile).fileName()
                << QStringLiteral("(%1 bytes)").arg(QFileInfo(sessionFile).size());

        reportCheckPhase("save");


        // 2. A change marks the session as modified, and saving it again (into the remembered file) clears that.
        ui->performTransaction(QStringLiteral("Rename pipeline"), [&]() { node->setSceneNodeName(QStringLiteral("renamed after saving")); });
        if(!ui->isSessionModified())
            reportVerificationFailure(QStringLiteral("session check: a change to the scene did not mark the session as modified"));
        ui->saveSession();
        if(ui->isSessionModified())
            reportVerificationFailure(QStringLiteral("session check: the modified session is still reported as modified after saving it again"));

        reportCheckPhase("modified state");


        // 3. Loading the session file has to replace the current scene by the saved one. Change the scene again without
        //    saving it first, so that the reload has something to undo.
        Scene* currentScene = ui->datasetContainer().activeScene();
        SceneNode* currentNode = currentScene && !currentScene->children().empty() ? currentScene->children().front() : nullptr;
        if(currentNode) {
            ui->performTransaction(QStringLiteral("Rename pipeline"), [&]() { currentNode->setSceneNodeName(QStringLiteral("renamed without saving")); });
        }

        const bool loaded = ui->loadSessionFile(QUrl::fromLocalFile(sessionFile));
        if(!loaded) {
            reportVerificationFailure(QStringLiteral("session check: the frontend rejected the session file it had written"));
        }
        else {
            Scene* reloadedScene = ui->datasetContainer().activeScene();
            SceneNode* reloadedNode = reloadedScene && !reloadedScene->children().empty() ? reloadedScene->children().front() : nullptr;
            if(!reloadedNode)
                reportVerificationFailure(QStringLiteral("session check: loading the session file produced an empty scene"));
            else if(reloadedNode->objectTitle() != QStringLiteral("renamed after saving"))
                reportVerificationFailure(QStringLiteral("session check: loading the session file did not restore the saved scene (title: %1)").arg(reloadedNode->objectTitle()));
            if(ui->isSessionModified())
                reportVerificationFailure(QStringLiteral("session check: a freshly loaded session is reported as modified"));
            if(ui->sessionFilePath() != QFileInfo(sessionFile).absoluteFilePath())
                reportVerificationFailure(QStringLiteral("session check: the loaded session does not remember its file"));
        }

        reportCheckPhase("reload");


        // 4. The session file ends up in the recently opened files, which is the list the frontends offer to the user.
        const auto& recentEntries = RecentFilesList::instance().entries();
        const bool isMostRecent = !recentEntries.isEmpty() && recentEntries.front().urls.size() == 1
            && recentEntries.front().urls.front() == QUrl::fromLocalFile(sessionFile);
        if(!isMostRecent)
            reportVerificationFailure(QStringLiteral("session check: the session file is not the most recently opened file"));
        qInfo() << "SESSION_TEST the session workflow saved, modified and reloaded the scene;"
                << RecentFilesList::instance().entries().size() << "recent file(s)";

        reportCheckPhase("recent files");


        // 5. A session without a file name asks the frontend for one. The frontend presents the file dialog of the QML
        //    scene (the controller owns its state so that this check can answer it, exactly like the message box), and
        //    the answer decides where the session is written.
        OORef<DataSet> dataset = ui->datasetContainer().currentSet();
        const QString rememberedPath = ui->sessionFilePath();
        const QString saveAsFile = QDir::tempPath() + QStringLiteral("/ovito-qml-session-test/session-as.ovito");
        QFile::remove(saveAsFile);
        if(dataset)
            dataset->setFilePath({});

        // 5a. A cancelled question changes nothing: no file, no session path, and the session stays modified.
        answerNextFileDialog(ui, controller, QString());
        const bool savedAfterCancel = ui->saveSession();
        if(savedAfterCancel)
            reportVerificationFailure(QStringLiteral("session check: a cancelled file dialog still reported a saved session"));
        if(!ui->sessionFilePath().isEmpty())
            reportVerificationFailure(QStringLiteral("session check: a cancelled file dialog gave the session a file"));
        if(!QFileInfo::exists(saveAsFile))
            qInfo() << "SESSION_TEST a cancelled file dialog wrote no file and left the session without one";

        reportCheckPhase("file dialog");

        // 5b. An answered question (and the "Save As" command that goes through it) writes the session to that file.
        answerNextFileDialog(ui, controller, saveAsFile);
        if(!ui->saveSession())
            reportVerificationFailure(QStringLiteral("session check: the session was not saved to the file the dialog answered with"));
        if(ui->sessionFilePath() != QFileInfo(saveAsFile).absoluteFilePath())
            reportVerificationFailure(QStringLiteral("session check: the session does not remember the file the dialog answered with (%1)").arg(ui->sessionFilePath()));
        if(!QFileInfo::exists(saveAsFile))
            reportVerificationFailure(QStringLiteral("session check: the file the dialog answered with was not written"));
        if(ui->isSessionModified())
            reportVerificationFailure(QStringLiteral("session check: a session saved through the file dialog is reported as modified"));

        // "Save As" asks again even though the session now has a file of its own, which is the whole difference to Save.
        const QString saveAsFile2 = QDir::tempPath() + QStringLiteral("/ovito-qml-session-test/session-as-2.ovito");
        QFile::remove(saveAsFile2);
        Command* saveAsCommand = ui->actionManager()->findCommand(QStringLiteral(ACTION_FILE_SAVEAS));
        if(!saveAsCommand)
            reportVerificationFailure(QStringLiteral("session check: the Save As command does not exist"));
        else {
            answerNextFileDialog(ui, controller, saveAsFile2);
            saveAsCommand->trigger();
            if(ui->sessionFilePath() != QFileInfo(saveAsFile2).absoluteFilePath())
                reportVerificationFailure(QStringLiteral("session check: the Save As command did not save to the file the dialog answered with (%1)").arg(ui->sessionFilePath()));
            if(!QFileInfo::exists(saveAsFile2))
                reportVerificationFailure(QStringLiteral("session check: the Save As command wrote no file"));
            qInfo() << "SESSION_TEST the session file dialog answered by the user writes the session where it points;"
                    << "cancelling it changes nothing and Save As asks again";
        }

        // 5c. A session that cannot be written keeps its file and its dirty state and tells the user why. The parent of
        //     the target path does not exist, so the write fails for every user.
        const QString unwritableFile = QDir::tempPath() + QStringLiteral("/ovito-qml-session-test/no-such-directory/session.ovito");
        ui->performTransaction(QStringLiteral("Rename pipeline"), [&]() { node->setSceneNodeName(QStringLiteral("modified before the failed save")); });
        const QString pathBeforeFailure = ui->sessionFilePath();
        const bool wasModified = ui->isSessionModified();
        Command* saveCommand = ui->actionManager()->findCommand(QStringLiteral(ACTION_FILE_SAVEAS));
        if(!saveCommand) {
            reportVerificationFailure(QStringLiteral("session check: the Save As command does not exist"));
        }
        else {
            // The frontend reports the failure the shared workflow throws, in the status line and in an error dialog.
            answerNextMessageBox(ui, controller, UserInterface::MessageBoxButton::Ok);
            answerNextFileDialog(ui, controller, unwritableFile);
            saveCommand->trigger();
            if(QFileInfo::exists(unwritableFile))
                reportVerificationFailure(QStringLiteral("session check: a session was written to a path that cannot be written"));
            if(ui->sessionFilePath() != pathBeforeFailure)
                reportVerificationFailure(QStringLiteral("session check: a failed save changed the file of the session to %1").arg(ui->sessionFilePath()));
            if(!ui->isSessionModified() || !wasModified)
                reportVerificationFailure(QStringLiteral("session check: a failed save cleared the modified state of the session"));
            if(controller->statusMessage().isEmpty())
                reportVerificationFailure(QStringLiteral("session check: a failed save reported nothing in the status line"));
            else
                qInfo() << "SESSION_TEST a save that cannot be written keeps the session and reports:" << controller->statusMessage();
        }

        reportCheckPhase("unwritable save");

        // 5d. Opening a session asks the same question, and the file it answers with becomes the current session.
        answerNextFileDialog(ui, controller, saveAsFile2);
        if(!ui->openSession())
            reportVerificationFailure(QStringLiteral("session check: the session was not opened through the file dialog"));
        if(ui->sessionFilePath() != QFileInfo(saveAsFile2).absoluteFilePath())
            reportVerificationFailure(QStringLiteral("session check: opening a session through the file dialog did not adopt its file (%1)").arg(ui->sessionFilePath()));
        if(ui->isSessionModified())
            reportVerificationFailure(QStringLiteral("session check: an opened session is reported as modified"));
        qInfo() << "SESSION_TEST opening a session through the file dialog loads the file it answered with";

        // The phases that follow work with the session file of the check again, and the data set they get is the one the
        // load just installed - not the one the earlier phases had.
        if(OORef<DataSet> reloaded = ui->datasetContainer().currentSet())
            reloaded->setFilePath(rememberedPath);

        reportCheckPhase("open through the dialog");

        // 5e. A directory is not a file to import but the directory to work in: the shared import path makes it the
        //     working directory of the process and opens the file dialog there, which is what the command line does.
        const QString directory = QDir::tempPath() + QStringLiteral("/ovito-qml-session-test/working");
        QDir().mkpath(directory);
        const QString previousWorkingDirectory = QDir::currentPath();
        controller->importFiles(QVariantList{ QUrl::fromLocalFile(directory) });
        // Compared as canonical paths: a working directory is reported in the form the operating system resolved it,
        // and a temporary directory behind a symbolic link therefore does not look like the path that was asked for -
        // on macOS QDir::tempPath() is /var/folders/..., which resolves to /private/var/folders/..., so comparing the
        // strings failed there although the working directory had in fact been changed (UI_TEST_ENV.md 6.2).
        if(QFileInfo(QDir::currentPath()).canonicalFilePath() != QFileInfo(directory).canonicalFilePath())
            reportVerificationFailure(QStringLiteral("session check: a directory handed to the import path did not become the working directory (%1)").arg(QDir::currentPath()));
        else
            qInfo() << "SESSION_TEST a directory handed to the import path becomes the working directory";
        QDir::setCurrent(previousWorkingDirectory);

        reportCheckPhase("working directory");

        // 6. Closing a workbench with unsaved changes asks about them, which is the question the classic frontend asks
        //    in its close event (MainWindow::closeEvent); the title marks the modified session in the meantime.
        Scene* currentSceneAfterReload = ui->datasetContainer().activeScene();
        if(!ui->isSessionModified() && currentSceneAfterReload && !currentSceneAfterReload->children().empty()) {
            SceneNode* nodeToRename = currentSceneAfterReload->children().front();
            ui->performTransaction(QStringLiteral("Rename pipeline"), [&]() { nodeToRename->setSceneNodeName(QStringLiteral("renamed before closing")); });
        }
        if(!ui->isSessionModified()) {
            reportVerificationFailure(QStringLiteral("session check: the scene cannot be modified, so the close prompt cannot be checked"));
        }
        else {
            const QString modifiedTitle = controller->windowTitle();
            if(!modifiedTitle.contains(QLatin1Char('*')))
                reportVerificationFailure(QStringLiteral("session check: the window title does not mark the modified session (%1)").arg(modifiedTitle));

            // Cancelling the question keeps the workbench and the session exactly as they were.
            answerNextMessageBox(ui, controller, UserInterface::MessageBoxButton::Cancel);
            if(ui->canCloseWorkbench())
                reportVerificationFailure(QStringLiteral("session check: cancelling the save question allowed the workbench to close"));
            if(!ui->isSessionModified())
                reportVerificationFailure(QStringLiteral("session check: cancelling the save question discarded the changes"));

            // The window's own close path asks the same question, so a cancelled close leaves the window open.
            answerNextMessageBox(ui, controller, UserInterface::MessageBoxButton::Cancel);
            ui->view()->close();
            if(!ui->view()->isVisible())
                reportVerificationFailure(QStringLiteral("session check: a cancelled close closed the window anyway"));

            // Discarding the changes lets the workbench close without touching the session file, and saving them writes
            // the file and leaves a clean session - which the title stops marking.
            answerNextMessageBox(ui, controller, UserInterface::MessageBoxButton::No);
            if(!ui->canCloseWorkbench())
                reportVerificationFailure(QStringLiteral("session check: discarding the changes did not allow the workbench to close"));

            answerNextMessageBox(ui, controller, UserInterface::MessageBoxButton::Yes);
            if(!ui->canCloseWorkbench())
                reportVerificationFailure(QStringLiteral("session check: saving the changes did not allow the workbench to close"));
            if(ui->isSessionModified())
                reportVerificationFailure(QStringLiteral("session check: the session is still modified after saving it from the close prompt"));
            if(controller->windowTitle().contains(QLatin1Char('*')))
                reportVerificationFailure(QStringLiteral("session check: the title still marks a session that was just saved"));

            qInfo() << "SESSION_TEST closing the workbench with unsaved changes asks three ways: cancelling keeps the"
                    << "window open, discarding closes it, saving writes the session and clears the modified marker";
        }

        reportCheckPhase("close question");

        // 7. The session file is the most recent entry, and opening that entry reads it back - including through the
        //    import path, which redirects a .ovito file to the session loader as the classic frontend does.
        const QVariantList recentFiles = controller->recentFiles();
        if(recentFiles.isEmpty() || !recentFiles.front().toMap().value(QStringLiteral("isSession")).toBool())
            reportVerificationFailure(QStringLiteral("session check: the saved session is not the most recent session file"));
        else {
            controller->openRecentFile(0);
            Scene* sceneAfterReopening = ui->datasetContainer().activeScene();
            SceneNode* nodeAfterReopening = sceneAfterReopening && !sceneAfterReopening->children().empty() ? sceneAfterReopening->children().front() : nullptr;
            if(!nodeAfterReopening || nodeAfterReopening->objectTitle() != QStringLiteral("renamed before closing"))
                reportVerificationFailure(QStringLiteral("session check: opening the recent session file did not restore it (title: %1)")
                    .arg(nodeAfterReopening ? nodeAfterReopening->objectTitle() : QStringLiteral("<no object>")));

            controller->importFiles(QVariantList{ QUrl::fromLocalFile(sessionFile) });
            Scene* sceneAfterImport = ui->datasetContainer().activeScene();
            SceneNode* nodeAfterImport = sceneAfterImport && !sceneAfterImport->children().empty() ? sceneAfterImport->children().front() : nullptr;
            if(!nodeAfterImport || nodeAfterImport->objectTitle() != QStringLiteral("renamed before closing"))
                reportVerificationFailure(QStringLiteral("session check: importing a .ovito file did not load it as a session (title: %1)")
                    .arg(nodeAfterImport ? nodeAfterImport->objectTitle() : QStringLiteral("<no object>")));
            qInfo() << "SESSION_TEST the recent files entry of the session and a .ovito file handed to the import path"
                    << "both restore the saved session";
        }
    reportCheckPhase("recent files reopened");
    });

    continuation();
}

}   // namespace Ovito::Spike
