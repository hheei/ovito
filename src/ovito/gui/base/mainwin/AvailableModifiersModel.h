// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/base/GUIBase.h>
#include <ovito/gui/base/actions/Command.h>

namespace Ovito {

class PipelineListModel;    // Defined in PipelineListModel.h

class OVITO_GUIBASE_EXPORT ModifierAction : public Command
{
    Q_OBJECT

public:

    /// Constructs the command for a built-in modifier class.
    static ModifierAction* createForClass(ModifierClassPtr clazz);

    /// Constructs the command for a modifier template.
    static ModifierAction* createForTemplate(const QString& templateName);

    /// Returns the modifier's category.
    const QString& category() const { return _category; }

    /// Returns the modifier class descriptor if this command inserts a built-in modifier.
    ModifierClassPtr modifierClass() const { return _modifierClass; }

    /// The name of the modifier template if this command inserts a saved modifier template.
    const QString& templateName() const { return _templateName; }

    /// Updates the command's enabled/disabled state depending on the current data pipeline.
    /// Returns true if the state changed.
    bool updateState(const PipelineFlowState& input);

private:

    /// Constructor. Instances are created by the two factory functions above, which know the class or the template.
    ModifierAction(const QString& id, const QString& text, const QString& iconPath, const QString& statusTip);

    /// The Ovito class descriptor of the modifier subclass.
    ModifierClassPtr _modifierClass = nullptr;

    /// The modifier's category.
    QString _category;

    /// The name of the modifier template.
    QString _templateName;
};

/**
 * A Qt tree model that organizes all available modifier types by category.
 * Root items are categories, and child items are modifiers within each category.
 */
class OVITO_GUIBASE_EXPORT AvailableModifiersModel : public QAbstractItemModel, public UserInterfaceComponent<UserInterface>
{
    Q_OBJECT

public:

    /// Constructor.
    AvailableModifiersModel(QObject* parent, UserInterface& ui, PipelineListModel* pipelineListModel);

    /// Returns the model index for the item at the given row and column under the given parent.
    virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;

    /// Returns the parent of the model item with the given index.
    virtual QModelIndex parent(const QModelIndex& index) const override;

    /// Returns the number of rows under the given parent.
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    /// Returns the number of columns for the children of the given parent.
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    /// Returns the data associated with an item.
    virtual QVariant data(const QModelIndex& index, int role) const override;

    /// Returns the flags for an item.
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

    /// Returns the name of the category at the given index.
    const QString& categoryName(int categoryIndex) const { return _categoryNames[categoryIndex]; }

    /// The roles this model offers to the views that present it.
    enum Roles {
        /// The QAction that presents the command of the row in a widgets-based frontend.
        ActionRole = Qt::UserRole,
        /// The Command of the row, which every frontend can present and invoke.
        CommandRole,
    };

    /// Returns the list of modifier commands for the given category.
    const std::vector<Command*>& categoryCommands(int categoryIndex) const { return _commandsPerCategory[categoryIndex]; }

    /// Returns the command for a modifier at a given category and row.
    Command* commandAt(int categoryIndex, int modifierIndex) const;

    /// Returns the command for a modifier from a model index.
    Command* commandFromIndex(const QModelIndex& index) const;

    /// Returns the category index for the modifier templates.
    int templatesCategory() const { return (int)_commandsPerCategory.size() - 1; }

public Q_SLOTS:

    /// Updates the enabled/disabled state of all modifier actions based on the current pipeline.
    void updateActionState();

private Q_SLOTS:

    /// Signal handler that inserts the selected modifier into the current pipeline.
    void insertModifier();

    /// Rebuilds the list of actions for the modifier templates.
    void refreshTemplates();

    /// This handler is called whenever a new extension class has been registered at runtime.
    void extensionClassAdded(OvitoClassPtr clazz);

private:

    /// The list of modifier commands, sorted by category.
    std::vector<std::vector<Command*>> _commandsPerCategory;

    /// The list of modifier categories.
    std::vector<QString> _categoryNames;

    /// Model representing the current data pipeline.
    PipelineListModel* _pipelineListModel;

    /// The command that opens the dialog for managing the saved modifier templates. It is created by the frontend
    /// that owns the dialog and is presented here as an entry of the templates category.
    QPointer<Command> _manageTemplatesCommand;
};

}   // End of namespace
