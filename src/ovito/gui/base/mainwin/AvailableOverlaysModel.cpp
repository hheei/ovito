// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/base/GUIBase.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/gui/base/mainwin/templates/OverlayTemplates.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include "AvailableOverlaysModel.h"
#include "OverlayListModel.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
OverlayAction::OverlayAction(const QString& id, const QString& text, const QString& iconPath, const QString& statusTip)
    : Command(id, text, iconPath, statusTip)
{
}

/******************************************************************************
* Constructs the command for a built-in viewport layer class.
******************************************************************************/
OverlayAction* OverlayAction::createForClass(const ViewportOverlay::OOMetaClass* clazz)
{
    OverlayAction* action = new OverlayAction(QStringLiteral("InsertViewportLayer.%1.%2").arg(clazz->pluginId(), clazz->name()),
        clazz->displayName(), QStringLiteral("overlay_action_icon"), clazz->descriptionString());
    action->_layerClass = clazz;
    action->_category = clazz->viewportOverlayCategory();

    // Layers without a description fall back to a generic status bar text.
    if(action->statusTip().isEmpty())
        action->setStatusTip(tr("Insert this viewport layer."));

    return action;
}

/******************************************************************************
* Constructs the command for a viewport layer template.
******************************************************************************/
OverlayAction* OverlayAction::createForTemplate(const QString& templateName)
{
    OverlayAction* action = new OverlayAction(QStringLiteral("InsertViewportLayerTemplate.%1").arg(templateName),
        templateName, QStringLiteral("overlay_action_icon"), tr("Insert this viewport layer template."));
    action->_templateName = templateName;

    return action;
}

/******************************************************************************
* Constructor.
******************************************************************************/
AvailableOverlaysModel::AvailableOverlaysModel(QObject* parent, UserInterface& ui, OverlayListModel* overlayListModel) : QAbstractItemModel(parent), UserInterfaceComponent<UserInterface>(ui), _overlayListModel(overlayListModel)
{
    OVITO_ASSERT(actionManager());

    // Enumerate all built-in viewport layer classes.
    for(auto clazz : PluginManager::instance().metaclassMembers<ViewportOverlay>()) {

        // Skip overlays that want to be hidden from the user.
        // Do not add it to the list of available overlays.
        if(clazz->viewportOverlayCategory() == QStringLiteral("-"))
            continue;

        // Create the command for the viewport layer class.
        OverlayAction* action = OverlayAction::createForClass(clazz);

        // Register it with the global ActionManager, which creates its QAction view.
        actionManager()->addCommand(action);
        OVITO_ASSERT(action->parent() == actionManager());

        // Handle the insertion command.
        connect(action, &Command::triggered, this, &AvailableOverlaysModel::insertViewportLayer);

        // Sort the command into categories.
        const QString& category = !action->category().isEmpty() ? action->category() : tr("Standard layers");
        auto categoryIter = std::ranges::find(_categoryNames, category);
        int categoryIndex = std::distance(_categoryNames.begin(), categoryIter);
        if(categoryIter == _categoryNames.end()) {
            // New category.
            _categoryNames.push_back(category);
            _commandsPerCategory.emplace_back();
        }
        _commandsPerCategory[categoryIndex].push_back(action);
    }

    // Sort commands by name within each category.
    for(std::vector<Command*>& categoryCommands : _commandsPerCategory) {
        std::sort(categoryCommands.begin(), categoryCommands.end(), [](Command* a, Command* b) { return QString::localeAwareCompare(a->text(), b->text()) < 0; });
    }

    // Create the category for the saved viewport layer templates, which holds the commands for the templates themselves
    // and the command that opens the dialog for managing them.
    _categoryNames.push_back(tr("Layer templates"));
    _commandsPerCategory.emplace_back();
    _manageTemplatesCommand = actionManager()->findCommand(ACTION_VIEWPORT_MANAGE_OVERLAY_TEMPLATES);
    for(const QString& templateName : OverlayTemplates::get()->templateList()) {
        // Create the command for the template.
        OverlayAction* action = OverlayAction::createForTemplate(templateName);
        _commandsPerCategory.back().push_back(action);

        // Register it with the global ActionManager, which creates its QAction view.
        actionManager()->addCommand(action);
        OVITO_ASSERT(action->parent() == actionManager());

        // Handle the command.
        connect(action, &Command::triggered, this, &AvailableOverlaysModel::insertViewportLayer);
    }
    if(_manageTemplatesCommand)
        _commandsPerCategory.back().push_back(_manageTemplatesCommand);

    // Listen for changes to the underlying modifier template list.
    connect(OverlayTemplates::get(), &QAbstractItemModel::rowsInserted, this, &AvailableOverlaysModel::refreshTemplates);
    connect(OverlayTemplates::get(), &QAbstractItemModel::rowsRemoved, this, &AvailableOverlaysModel::refreshTemplates);
    connect(OverlayTemplates::get(), &QAbstractItemModel::modelReset, this, &AvailableOverlaysModel::refreshTemplates);
    connect(OverlayTemplates::get(), &QAbstractItemModel::dataChanged, this, &AvailableOverlaysModel::refreshTemplates);

    // Extend the list when a new Python extension is being registered at runtime.
    connect(&PluginManager::instance(), &PluginManager::extensionClassAdded, this, &AvailableOverlaysModel::extensionClassAdded);
}

/******************************************************************************
* Returns the model index for the item at the given row and column.
******************************************************************************/
QModelIndex AvailableOverlaysModel::index(int row, int column, const QModelIndex& parent) const
{
    if(column != 0)
        return {};

    if(!parent.isValid()) {
        // Root level: categories
        if(row >= 0 && row < (int)_categoryNames.size())
            return createIndex(row, 0, quintptr(-1));
    }
    else if(parent.internalId() == quintptr(-1)) {
        // Child level: overlays within a category
        int categoryIndex = parent.row();
        if(categoryIndex >= 0 && categoryIndex < (int)_commandsPerCategory.size()) {
            if(row >= 0 && row < (int)_commandsPerCategory[categoryIndex].size())
                return createIndex(row, 0, quintptr(categoryIndex));
        }
    }
    return {};
}

/******************************************************************************
* Returns the parent of the model item with the given index.
******************************************************************************/
QModelIndex AvailableOverlaysModel::parent(const QModelIndex& index) const
{
    if(!index.isValid())
        return {};

    quintptr id = index.internalId();
    if(id == quintptr(-1)) {
        // This is a category item at the root level.
        return {};
    }
    else {
        // This is an overlay item; its parent is the category.
        int categoryIndex = (int)id;
        return createIndex(categoryIndex, 0, quintptr(-1));
    }
}

/******************************************************************************
* Returns the number of rows under the given parent.
******************************************************************************/
int AvailableOverlaysModel::rowCount(const QModelIndex& parent) const
{
    if(!parent.isValid()) {
        // Root level: number of categories
        return (int)_categoryNames.size();
    }
    else if(parent.internalId() == quintptr(-1)) {
        // Category level: number of overlays in this category
        int categoryIndex = parent.row();
        if(categoryIndex >= 0 && categoryIndex < (int)_commandsPerCategory.size())
            return (int)_commandsPerCategory[categoryIndex].size();
    }
    // Overlays don't have children.
    return 0;
}

/******************************************************************************
* Returns the number of columns for the children of the given parent.
******************************************************************************/
int AvailableOverlaysModel::columnCount(const QModelIndex& parent) const
{
    return 1;
}

/******************************************************************************
* Returns the data associated with an item.
******************************************************************************/
QVariant AvailableOverlaysModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid())
        return {};

    if(index.internalId() == quintptr(-1)) {
        // Category item.
        int categoryIndex = index.row();
        if(categoryIndex < 0 || categoryIndex >= (int)_categoryNames.size())
            return {};

        if(role == Qt::DisplayRole)
            return _categoryNames[categoryIndex];
    }
    else {
        // Overlay item.
        int categoryIndex = (int)index.internalId();
        int overlayIndex = index.row();
        if(categoryIndex < 0 || categoryIndex >= (int)_commandsPerCategory.size())
            return {};
        if(overlayIndex < 0 || overlayIndex >= (int)_commandsPerCategory[categoryIndex].size())
            return {};

        Command* command = _commandsPerCategory[categoryIndex][overlayIndex];
        if(role == Qt::DisplayRole)
            return command->text();
        else if(role == ActionRole)
            return QVariant::fromValue(static_cast<QObject*>(actionManager()->actionView(command)));
        else if(role == CommandRole)
            return QVariant::fromValue(static_cast<QObject*>(command));
    }
    return {};
}

/******************************************************************************
* Returns the flags for an item.
******************************************************************************/
Qt::ItemFlags AvailableOverlaysModel::flags(const QModelIndex& index) const
{
    if(!index.isValid())
        return Qt::NoItemFlags;

    if(index.internalId() == quintptr(-1)) {
        // Category item: enabled but not selectable.
        return Qt::ItemIsEnabled;
    }
    else {
        // Overlay item.
        if(Command* command = commandFromIndex(index))
            return command->isEnabled() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable) : Qt::NoItemFlags;
    }
    return Qt::NoItemFlags;
}

/******************************************************************************
* Returns the action for an overlay at a given category and row.
******************************************************************************/
Command* AvailableOverlaysModel::commandAt(int categoryIndex, int overlayIndex) const
{
    if(categoryIndex >= 0 && categoryIndex < (int)_commandsPerCategory.size()) {
        if(overlayIndex >= 0 && overlayIndex < (int)_commandsPerCategory[categoryIndex].size())
            return _commandsPerCategory[categoryIndex][overlayIndex];
    }
    return nullptr;
}

/******************************************************************************
* Returns the command for an overlay from a model index.
******************************************************************************/
Command* AvailableOverlaysModel::commandFromIndex(const QModelIndex& index) const
{
    if(!index.isValid() || index.internalId() == quintptr(-1))
        return nullptr;

    int categoryIndex = (int)index.internalId();
    int overlayIndex = index.row();
    return commandAt(categoryIndex, overlayIndex);
}

/******************************************************************************
* Rebuilds the list of actions for the viewport layer templates.
******************************************************************************/
void AvailableOverlaysModel::refreshTemplates()
{
    std::vector<Command*>& templateCommands = _commandsPerCategory[templatesCategory()];

    // Discard the old commands of the templates. The command that manages the templates is not one of them; it belongs
    // to the frontend and outlives this list.
    for(Command* command : templateCommands) {
        if(qobject_cast<OverlayAction*>(command))
            actionManager()->deleteCommand(command);
    }
    templateCommands.clear();

    // Create new commands for the templates.
    int count = OverlayTemplates::get()->templateList().size();
    if(count != 0) {
        for(const QString& templateName : OverlayTemplates::get()->templateList()) {
            // Create the command for the template.
            OverlayAction* action = OverlayAction::createForTemplate(templateName);
            templateCommands.push_back(action);

            // Register it with the ActionManager, which creates its QAction view.
            actionManager()->addCommand(action);
            OVITO_ASSERT(action->parent() == actionManager());

            // Handle the command.
            connect(action, &Command::triggered, this, &AvailableOverlaysModel::insertViewportLayer);
        }
    }
    if(_manageTemplatesCommand)
        templateCommands.push_back(_manageTemplatesCommand);

    // Notify views that the model has been reset.
    beginResetModel();
    endResetModel();
}

/******************************************************************************
* Signal handler that inserts the selected viewport layer into the active viewport.
******************************************************************************/
void AvailableOverlaysModel::insertViewportLayer()
{
    // Get the action that emitted the signal.
    OverlayAction* action = qobject_cast<OverlayAction*>(sender());
    OVITO_ASSERT(action);

    // Get the current dataset and viewport.
    Viewport* vp = _overlayListModel->selectedViewport();
    if(!vp) return;

    // Instantiate the new layer and add it to the active viewport.
    performTransaction(tr("Insert viewport layer"), [&]() {
        int overlayIndex = -1;
        int underlayIndex = -1;
        if(OverlayListItem* item = _overlayListModel->selectedItem()) {
            overlayIndex = vp->overlays().indexOf(item->overlay());
            underlayIndex = vp->underlays().indexOf(item->overlay());
        }
        if(overlayIndex == -1 && underlayIndex == -1)
            overlayIndex = vp->overlays().size() - 1;
        if(action->layerClass()) {
            // Create an instance of the overlay class.
            OORef<ViewportOverlay> layer = static_object_cast<ViewportOverlay>(action->layerClass()->createInstance());
            // Make sure the new overlay gets selected in the UI.
            _overlayListModel->setNextToSelectObject(layer);
            // Insert it into either the overlays or the underlays list.
            if(underlayIndex >= 0)
                vp->insertUnderlay(underlayIndex+1, layer);
            else
                vp->insertOverlay(overlayIndex+1, layer);
        }
        else if(!action->templateName().isEmpty()) {
            // Load template from the store.
            QVector<OORef<ViewportOverlay>> layerSet = OverlayTemplates::get()->instantiateTemplate(action->templateName());
            // Insert the layer(s) into either the overlays or the underlays list.
            for(OORef<ViewportOverlay>& layer : layerSet) {
                // Make sure the new overlay gets selected in the UI.
                _overlayListModel->setNextToSelectObject(layer);
                if(underlayIndex >= 0)
                    vp->insertUnderlay(underlayIndex+1, std::move(layer));
                else
                    vp->insertOverlay(overlayIndex+1, std::move(layer));
            }
        }
        else return;

        // Automatically activate preview mode to make the overlay visible.
        vp->setRenderPreviewMode(true);

        // Show the overlays tab of the command panel.
        if(Command* overlaysCommand = actionManager()->findCommand(ACTION_COMMAND_PANEL_OVERLAYS))
            overlaysCommand->trigger();
    });
}

/******************************************************************************
* This handler is called whenever a new extension class has been registered at runtime.
******************************************************************************/
void AvailableOverlaysModel::extensionClassAdded(OvitoClassPtr cls)
{
    // Skip classes that are not viewport layers.
    if(!cls->isDerivedFrom(ViewportOverlay::OOClass()))
        return;
    const ViewportOverlay::OOMetaClass* clazz = static_cast<const ViewportOverlay::OOMetaClass*>(cls);

    // Skip modifiers that want to be hidden from the user.
    // Do not add it to the list of available modifiers.
    if(clazz->viewportOverlayCategory() == QStringLiteral("-"))
        return;

    // Create the command for the viewport layer class.
    OverlayAction* action = OverlayAction::createForClass(clazz);

    // Register it with the global ActionManager, which creates its QAction view.
    actionManager()->addCommand(action);

    // Handle the insertion command.
    connect(action, &Command::triggered, this, &AvailableOverlaysModel::insertViewportLayer);

    // Insert the command into the right category. Or create a new category if necessary.
    auto categoryIter = std::ranges::find(_categoryNames, action->category());
    int categoryIndex = std::distance(_categoryNames.begin(), categoryIter);
    if(categoryIter == _categoryNames.end()) {
        _categoryNames.push_back(action->category());
        _commandsPerCategory.emplace_back();
    }
    _commandsPerCategory[categoryIndex].push_back(action);

    // Notify views that the model has been reset.
    beginResetModel();
    endResetModel();
}

}   // End of namespace
