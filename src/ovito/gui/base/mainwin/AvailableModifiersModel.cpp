// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/gui/base/mainwin/templates/ModifierTemplates.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include "AvailableModifiersModel.h"
#include "PipelineListModel.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
ModifierAction::ModifierAction(const QString& id, const QString& text, const QString& iconPath, const QString& statusTip)
    : Command(id, text, iconPath, statusTip)
{
}

/******************************************************************************
* Constructs the command for a built-in modifier class.
******************************************************************************/
ModifierAction* ModifierAction::createForClass(ModifierClassPtr clazz)
{
    ModifierAction* action = new ModifierAction(QStringLiteral("InsertModifier.%1.%2").arg(clazz->pluginId(), clazz->name()),
        clazz->displayName(), QStringLiteral("modify_modifier_action_icon"), clazz->descriptionString());
    action->_modifierClass = clazz;
    action->_category = clazz->modifierCategory();

    // Modifiers without a description fall back to a generic status bar text.
    if(action->statusTip().isEmpty())
        action->setStatusTip(tr("Insert this modifier into the data pipeline."));

    // Modifiers without a category are moved into the "Other" category.
    if(action->_category.isEmpty())
        action->_category = tr("Other");

    return action;
}

/******************************************************************************
* Constructs the command for a modifier template.
******************************************************************************/
ModifierAction* ModifierAction::createForTemplate(const QString& templateName)
{
    ModifierAction* action = new ModifierAction(QStringLiteral("InsertModifierTemplate.%1").arg(templateName),
        templateName, QStringLiteral("modify_modifier_action_icon"), tr("Insert this modifier template into the data pipeline."));
    action->_templateName = templateName;

    return action;
}

/******************************************************************************
* Updates the actions enabled/disabled state depending on the current data pipeline.
******************************************************************************/
bool ModifierAction::updateState(const PipelineFlowState& input)
{
    bool enable = input.data() && (!modifierClass() || modifierClass()->isApplicableTo(*input.data()));
    if(isEnabled() != enable) {
        setEnabled(enable);
        return true;
    }
    return false;
}

/******************************************************************************
* Constructor.
******************************************************************************/
AvailableModifiersModel::AvailableModifiersModel(QObject* parent, UserInterface& ui, PipelineListModel* pipelineListModel) : QAbstractItemModel(parent), UserInterfaceComponent<UserInterface>(ui), _pipelineListModel(pipelineListModel)
{
    OVITO_ASSERT(actionManager());

    // Update the state of this model's actions whenever the ActionManager requests it.
    connect(actionManager(), &ActionManager::actionUpdateRequested, this, &AvailableModifiersModel::updateActionState);

    // Enumerate all registered modifier classes.
    for(ModifierClassPtr clazz : PluginManager::instance().metaclassMembers<Modifier>()) {

        // Skip modifiers that want to be hidden from the user.
        // Do not add it to the list of available modifiers.
        if(clazz->modifierCategory() == QStringLiteral("-"))
            continue;

        // Create the command for the modifier class.
        ModifierAction* action = ModifierAction::createForClass(clazz);

        // Register it with the global ActionManager, which creates its QAction view.
        actionManager()->addCommand(action);
        OVITO_ASSERT(action->parent() == actionManager());

        // Handle the insertion action.
        connect(action, &Command::triggered, this, &AvailableModifiersModel::insertModifier);

        // Sort the command into categories.
        auto categoryIter = std::find(_categoryNames.begin(), _categoryNames.end(), action->category());
        if(categoryIter == _categoryNames.end()) {
            // New category.
            _categoryNames.push_back(action->category());
            _commandsPerCategory.emplace_back();
            categoryIter = _categoryNames.end() - 1;
        }
        int categoryIndex = (int)(categoryIter - _categoryNames.begin());
        _commandsPerCategory[categoryIndex].push_back(action);
    }

    // Sort commands by name within each category.
    for(std::vector<Command*>& categoryCommands : _commandsPerCategory) {
        std::sort(categoryCommands.begin(), categoryCommands.end(), [](Command* a, Command* b) { return QString::localeAwareCompare(a->text(), b->text()) < 0; });
    }

    // Create the category for the saved modifier templates, which holds the commands for the templates themselves and
    // the command that opens the dialog for managing them.
    _categoryNames.push_back(tr("Modifier templates"));
    _commandsPerCategory.emplace_back();
    _manageTemplatesCommand = actionManager()->findCommand(ACTION_MODIFIER_MANAGE_MODIFIER_TEMPLATES);
    for(const QString& templateName : ModifierTemplates::get()->templateList()) {
        // Create the command for the modifier template.
        ModifierAction* action = ModifierAction::createForTemplate(templateName);
        _commandsPerCategory.back().push_back(action);

        // Register it with the global ActionManager, which creates its QAction view.
        actionManager()->addCommand(action);
        OVITO_ASSERT(action->parent() == actionManager());

        // Handle the command.
        connect(action, &Command::triggered, this, &AvailableModifiersModel::insertModifier);
    }
    if(_manageTemplatesCommand)
        _commandsPerCategory.back().push_back(_manageTemplatesCommand);

    // Listen for changes to the underlying modifier template list.
    connect(ModifierTemplates::get(), &QAbstractItemModel::rowsInserted, this, &AvailableModifiersModel::refreshTemplates);
    connect(ModifierTemplates::get(), &QAbstractItemModel::rowsRemoved, this, &AvailableModifiersModel::refreshTemplates);
    connect(ModifierTemplates::get(), &QAbstractItemModel::modelReset, this, &AvailableModifiersModel::refreshTemplates);
    connect(ModifierTemplates::get(), &QAbstractItemModel::dataChanged, this, &AvailableModifiersModel::refreshTemplates);

    // Extend the list when a new Python extension is being registered at runtime.
    connect(&PluginManager::instance(), &PluginManager::extensionClassAdded, this, &AvailableModifiersModel::extensionClassAdded);
}

/******************************************************************************
* Returns the model index for the item at the given row and column.
******************************************************************************/
QModelIndex AvailableModifiersModel::index(int row, int column, const QModelIndex& parent) const
{
    if(column != 0)
        return {};

    if(!parent.isValid()) {
        // Root level: categories
        if(row >= 0 && row < (int)_categoryNames.size())
            return createIndex(row, 0, quintptr(-1));
    }
    else if(parent.internalId() == quintptr(-1)) {
        // Child level: modifiers within a category
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
QModelIndex AvailableModifiersModel::parent(const QModelIndex& index) const
{
    if(!index.isValid())
        return {};

    quintptr id = index.internalId();
    if(id == quintptr(-1)) {
        // This is a category item at the root level.
        return {};
    }
    else {
        // This is a modifier item; its parent is the category.
        int categoryIndex = (int)id;
        return createIndex(categoryIndex, 0, quintptr(-1));
    }
}

/******************************************************************************
* Returns the number of rows under the given parent.
******************************************************************************/
int AvailableModifiersModel::rowCount(const QModelIndex& parent) const
{
    if(!parent.isValid()) {
        // Root level: number of categories
        return (int)_categoryNames.size();
    }
    else if(parent.internalId() == quintptr(-1)) {
        // Category level: number of modifiers in this category
        int categoryIndex = parent.row();
        if(categoryIndex >= 0 && categoryIndex < (int)_commandsPerCategory.size())
            return (int)_commandsPerCategory[categoryIndex].size();
    }
    // Modifiers don't have children.
    return 0;
}

/******************************************************************************
* Returns the number of columns for the children of the given parent.
******************************************************************************/
int AvailableModifiersModel::columnCount(const QModelIndex& parent) const
{
    return 1;
}

/******************************************************************************
* Returns the data associated with an item.
******************************************************************************/
QVariant AvailableModifiersModel::data(const QModelIndex& index, int role) const
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
        // Modifier item.
        int categoryIndex = (int)index.internalId();
        int modifierIndex = index.row();
        if(categoryIndex < 0 || categoryIndex >= (int)_commandsPerCategory.size())
            return {};
        if(modifierIndex < 0 || modifierIndex >= (int)_commandsPerCategory[categoryIndex].size())
            return {};

        Command* command = _commandsPerCategory[categoryIndex][modifierIndex];
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
Qt::ItemFlags AvailableModifiersModel::flags(const QModelIndex& index) const
{
    if(!index.isValid())
        return Qt::NoItemFlags;

    if(index.internalId() == quintptr(-1)) {
        // Category item: enabled but not selectable.
        return Qt::ItemIsEnabled;
    }
    else {
        // Modifier item.
        if(Command* command = commandFromIndex(index))
            return command->isEnabled() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable) : Qt::NoItemFlags;
    }
    return Qt::NoItemFlags;
}

/******************************************************************************
* Returns the action for a modifier at a given category and row.
******************************************************************************/
Command* AvailableModifiersModel::commandAt(int categoryIndex, int modifierIndex) const
{
    if(categoryIndex >= 0 && categoryIndex < (int)_commandsPerCategory.size()) {
        if(modifierIndex >= 0 && modifierIndex < (int)_commandsPerCategory[categoryIndex].size())
            return _commandsPerCategory[categoryIndex][modifierIndex];
    }
    return nullptr;
}

/******************************************************************************
* Returns the command for a modifier from a model index.
******************************************************************************/
Command* AvailableModifiersModel::commandFromIndex(const QModelIndex& index) const
{
    if(!index.isValid() || index.internalId() == quintptr(-1))
        return nullptr;

    int categoryIndex = (int)index.internalId();
    int modifierIndex = index.row();
    return commandAt(categoryIndex, modifierIndex);
}

/******************************************************************************
* Signal handler that inserts the selected modifier into the current pipeline.
******************************************************************************/
void AvailableModifiersModel::insertModifier()
{
    // Get the command that emitted the signal.
    ModifierAction* action = qobject_cast<ModifierAction*>(sender());
    OVITO_ASSERT(action);

    // Instantiate the new modifier(s) and insert them into the pipeline.
    performTransaction(tr("Insert modifier"), [&]() {

        if(action->modifierClass()) {
            // Create an instance of the modifier.
            OORef<Modifier> modifier = static_object_cast<Modifier>(action->modifierClass()->createInstance());
            // Insert modifier into the data pipeline.
            _pipelineListModel->applyModifiers({modifier});
        }
        else if(!action->templateName().isEmpty()) {
            // Load modifier template from the store.
            QVector<OORef<Modifier>> modifierSet = ModifierTemplates::get()->instantiateTemplate(action->templateName());
            // Put the modifiers into a group if the template consists of two or more modifiers.
            OORef<ModifierGroup> modifierGroup;
            if(modifierSet.size() >= 2) {
                modifierGroup = OORef<ModifierGroup>::create();
                modifierGroup->setCollapsed(true);
                modifierGroup->setTitle(action->templateName());
            }
            // Insert modifier(s) into the data pipeline.
            _pipelineListModel->applyModifiers(modifierSet, modifierGroup);
        }

        // Show the modify tab of the command panel.
        if(Command* modifyCommand = actionManager()->findCommand(ACTION_COMMAND_PANEL_MODIFY))
            modifyCommand->trigger();
    });
}

/******************************************************************************
* Rebuilds the list of actions for the modifier templates.
******************************************************************************/
void AvailableModifiersModel::refreshTemplates()
{
    std::vector<Command*>& templateCommands = _commandsPerCategory[templatesCategory()];

    // Discard the old commands of the modifier templates. The command that manages the templates is not one of them;
    // it belongs to the frontend and outlives this list.
    for(Command* command : templateCommands) {
        if(qobject_cast<ModifierAction*>(command))
            actionManager()->deleteCommand(command);
    }
    templateCommands.clear();

    // Create new commands for the modifier templates.
    int count = ModifierTemplates::get()->templateList().size();
    if(count != 0) {
        for(const QString& templateName : ModifierTemplates::get()->templateList()) {
            // Create the command for the modifier template.
            ModifierAction* action = ModifierAction::createForTemplate(templateName);
            templateCommands.push_back(action);

            // Register it with the ActionManager, which creates its QAction view.
            actionManager()->addCommand(action);
            OVITO_ASSERT(action->parent() == actionManager());

            // Handle the command.
            connect(action, &Command::triggered, this, &AvailableModifiersModel::insertModifier);
        }
    }
    if(_manageTemplatesCommand)
        templateCommands.push_back(_manageTemplatesCommand);

    // Notify views that the model has been reset.
    beginResetModel();
    endResetModel();
}

/******************************************************************************
* Updates the enabled/disabled state of all modifier actions based on the current pipeline.
******************************************************************************/
void AvailableModifiersModel::updateActionState()
{
    // Retrieve the input pipeline state, which a newly inserted modifier would be applied to.
    // This is used to determine which modifiers are applicable.
    PipelineFlowState inputState;

    // Get the selected item in the pipeline editor.
    PipelineListItem* currentItem = _pipelineListModel->selectedItem();
    while(currentItem && currentItem->parent()) {
        currentItem = currentItem->parent();
    }

    // Obtain pipeline output at the selected stage.
    if(PipelineNode* pipelineNode = currentItem ? dynamic_object_cast<PipelineNode>(currentItem->object()) : nullptr) {
        inputState = pipelineNode->getCachedPipelineNodeOutput(currentAnimationTime());
    }
    else if(Pipeline* pipeline = _pipelineListModel->selectedPipeline()) {
        inputState = pipeline->getCachedPipelineOutput(currentAnimationTime());
    }

    // Update the commands.
    for(int categoryIndex = 0; categoryIndex < (int)_commandsPerCategory.size(); categoryIndex++) {
        for(int modifierIndex = 0; modifierIndex < (int)_commandsPerCategory[categoryIndex].size(); modifierIndex++) {
            ModifierAction* action = qobject_cast<ModifierAction*>(_commandsPerCategory[categoryIndex][modifierIndex]);
            if(action && action->updateState(inputState)) {
                QModelIndex idx = index(modifierIndex, 0, index(categoryIndex, 0));
                Q_EMIT dataChanged(idx, idx);
            }
        }
    }
}

/******************************************************************************
* This handler is called whenever a new extension class has been registered at runtime.
******************************************************************************/
void AvailableModifiersModel::extensionClassAdded(OvitoClassPtr cls)
{
    // Skip classes that are not modifiers.
    if(!cls->isDerivedFrom(Modifier::OOClass()))
        return;
    ModifierClassPtr clazz = static_cast<ModifierClassPtr>(cls);

    // Skip modifiers that want to be hidden from the user.
    // Do not add it to the list of available modifiers.
    if(clazz->modifierCategory() == QStringLiteral("-"))
        return;

    // Create the command for the modifier class.
    ModifierAction* action = ModifierAction::createForClass(clazz);

    // Register it with the global ActionManager, which creates its QAction view.
    actionManager()->addCommand(action);

    // Handle the insertion command.
    connect(action, &Command::triggered, this, &AvailableModifiersModel::insertModifier);

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
