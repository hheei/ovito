// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/base/mainwin/UtilityObject.h>

namespace Ovito {

/**
 * A Qt action that activates a utility in the command panel.
 */
class OVITO_GUI_EXPORT UtilityAction : public QAction
{
    Q_OBJECT

public:

    /// Constructs an action for a utility object class.
    static UtilityAction* createForClass(const UtilityObject::OOMetaClass* clazz);

    /// Returns the utility's category.
    const QString& category() const { return _category; }

    /// Returns the utility class descriptor.
    const UtilityObject::OOMetaClass* utilityClass() const { return _utilityClass; }

private:

    /// The Ovito class descriptor of the utility object class.
    const UtilityObject::OOMetaClass* _utilityClass = nullptr;

    /// The utility's category.
    QString _category;
};

/**
 * A Qt list model that list all available utility applets.
 */
class OVITO_GUI_EXPORT UtilityListModel : public QAbstractListModel, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Constructor.
    UtilityListModel(QObject* parent, MainWindowUI& ui);

    /// Returns the number of rows in the model.
    virtual int rowCount(const QModelIndex& parent) const override;

    /// Returns the data associated with a list item.
    virtual QVariant data(const QModelIndex& index, int role) const override;

    /// Returns the flags for an item.
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

    /// Returns the action that belongs to the given model index.
    UtilityAction* actionFromIndex(int index) const { return (index >= 0 && index < _modelActions.size()) ? _modelActions[index] : nullptr; }

    /// Returns the action that belongs to the given model index.
    UtilityAction* actionFromIndex(const QModelIndex& index) const { return actionFromIndex(index.row()); }

    /// Determines the corresponding model index for a utility.
    int indexFromUtilityObject(const UtilityObject* utility) const;

    /// Returns the list index where the "Get more utilities..." item is located.
    int getMoreExtensionsItemIndex() const { return _getMoreExtensionsItemIndex; }

Q_SIGNALS:

    /// Is emitted when the user has selected a utility to be shown in the command panel.
    void utilitySelected(UtilityObject* utility);

private Q_SLOTS:

    /// Updates the color brushes of the model.
    void updateColorPalette(const QPalette& palette);

    /// Signal handler that activates the selected utility.
    void activateUtility();

    /// This handler is called whenever a new extension class has been registered at runtime.
    void extensionClassAdded(OvitoClassPtr clazz);

    /// This handler is called whenever a dataset has been loaded from a state file.
    void datasetReplaced(DataSet* dataset);

private:

    /// Rebuilds the internal list of model items.
    void updateModelLists();

    /// The list of utility actions, sorted alphabetically.
    std::vector<UtilityAction*> _actions;

    /// The list of utility actions, sorted by category.
    std::vector<std::vector<UtilityAction*>> _actionsPerCategory;

    /// The list of categories.
    std::vector<QString> _categoryNames;

    /// The utility actions as shown by the model.
    std::vector<UtilityAction*> _modelActions;

    /// The display strings as shown by the model.
    std::vector<QString> _modelStrings;

    /// Font used for category header items.
    QFont _categoryFont;

    /// Colors used for category header items.
    QBrush _categoryBackgroundBrush;
    QBrush _categoryForegroundBrush;

    /// The font used for "Get more utilities..." item.
    QFont _getMoreExtensionsFont;

    /// Color used for the "Get more utilities..." item.
    QBrush _getMoreExtensionsForegroundBrush;

    /// The list index where the "Get more utilities..." item is located.
    int _getMoreExtensionsItemIndex = -1;

    /// List of instantiated utility objects.
    std::vector<OORef<UtilityObject>> _utilityObjects;
};

}   // End of namespace
