////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

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
