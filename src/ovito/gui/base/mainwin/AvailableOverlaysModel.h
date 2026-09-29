// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/base/GUIBase.h>
#include <ovito/gui/base/actions/Command.h>
#include <ovito/core/viewport/overlays/ViewportOverlay.h>

namespace Ovito {

class OverlayListModel; // Defined in OverlayListModel.h

class OVITO_GUIBASE_EXPORT OverlayAction : public Command
{
    Q_OBJECT

public:

    /// Constructor. Instances are created by the two factory functions below, which know the class or the template.
    OverlayAction(const QString& id, const QString& text, const QString& iconPath, const QString& statusTip);

public:

    /// Constructs an action for a built-in layer class.
    static OverlayAction* createForClass(const ViewportOverlay::OOMetaClass* clazz);

    /// Constructs an action for a viewport layer template.
    static OverlayAction* createForTemplate(const QString& templateName);

    /// Returns the viewport layer's category.
    const QString& category() const { return _category; }

    /// Returns the overlay class descriptor if this action represents a built-in overlay type.
    OvitoClassPtr layerClass() const { return _layerClass; }

   /// The name of the viewport layer template if this action represents a saved template.
    const QString& templateName() const { return _templateName; }

    /// The absolute path of the modifier script if this action represents a Python-based modifier function.
    const QString& scriptPath() const { return _scriptPath; }

private:

    /// The Ovito class descriptor of the viewport layer subclass.
    OvitoClassPtr _layerClass = nullptr;

    /// The viewport layer's category.
    QString _category;

    /// The path to the overlay script on disk.
    QString _scriptPath;

    /// The name of the viewport layer template.
    QString _templateName;
};

/**
 * A Qt tree model that organizes all available viewport layer types by category.
 * Root items are categories, and child items are viewport layers within each category.
 */
class OVITO_GUIBASE_EXPORT AvailableOverlaysModel : public QAbstractItemModel, public UserInterfaceComponent<UserInterface>
{
    Q_OBJECT

public:

    /// Constructor.
    AvailableOverlaysModel(QObject* parent, UserInterface& ui, OverlayListModel* overlayListModel);

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

    /// Returns the list of viewport layer commands for the given category.
    const std::vector<Command*>& categoryCommands(int categoryIndex) const { return _commandsPerCategory[categoryIndex]; }

    /// Returns the command for an overlay at a given category and row.
    Command* commandAt(int categoryIndex, int overlayIndex) const;

    /// Returns the command for an overlay from a model index.
    Command* commandFromIndex(const QModelIndex& index) const;

    /// Returns the category index for the viewport layer templates.
    int templatesCategory() const { return (int)_commandsPerCategory.size() - 1; }

private Q_SLOTS:

    /// Rebuilds the list of actions for the viewport layer templates.
    void refreshTemplates();

    /// Signal handler that inserts the selected viewport layer into the active viewport.
    void insertViewportLayer();

    /// This handler is called whenever a new extension class has been registered at runtime.
    void extensionClassAdded(OvitoClassPtr clazz);

private:

    /// The list of viewport layer commands, sorted by category.
    std::vector<std::vector<Command*>> _commandsPerCategory;

    /// The list of viewport layer categories.
    std::vector<QString> _categoryNames;

    /// The model representing the viewport layers of the active viewport.
    OverlayListModel* _overlayListModel;

    /// The list of directories searched for user-defined viewport layer scripts.
    QVector<QDir> _layerScriptDirectories;

    /// The command that opens the dialog for managing the saved viewport layer templates. It is created by the
    /// frontend that owns the dialog and is presented here as an entry of the templates category.
    QPointer<Command> _manageTemplatesCommand;
};

}   // End of namespace
