// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {
/**
 * A properties editor for the RemovePropertyModifier class.
 */
class RemovePropertyModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(RemovePropertyModifierEditor)

protected:
    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private:
    class ViewModel : public QAbstractTableModel
    {
    public:
        ViewModel(RemovePropertyModifierEditor* owner) : QAbstractTableModel(owner) {}
        [[nodiscard]] auto* editor() const { return static_cast<RemovePropertyModifierEditor*>(QObject::parent()); }
        [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override { return (int)_propertyNames.size(); }
        [[nodiscard]] int columnCount(const QModelIndex& parent = QModelIndex()) const override { return 1; }
        [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
        [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
        [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
        bool setData(const QModelIndex& index, const QVariant& value, int role) override;
        void refresh();

    private:
        QStringList _propertyNames;
    };

    QTableView* _propertiesBox = nullptr;
};
}  // namespace Ovito
