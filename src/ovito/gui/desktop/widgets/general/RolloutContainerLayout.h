// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/******************************************************************************
* A Qt layout implementation used by the RolloutContainer widget.
******************************************************************************/
class OVITO_GUI_EXPORT RolloutContainerLayout : public QLayout
{
    Q_OBJECT

public:

    RolloutContainerLayout(QWidget* parent) : QLayout(parent) {}
    virtual ~RolloutContainerLayout();

    void addItem(QLayoutItem* item) override;
    int count() const override { return list.size(); }

    void insertWidget(int index, QWidget* widgets);

    void setGeometry(const QRect& r) override;

    QSize sizeHint() const override;
    QSize minimumSize() const override;

    QLayoutItem* itemAt(int index) const override {
        if(index < 0 || index >= list.size()) return nullptr;
        return list[index];
    }
    QLayoutItem* takeAt(int index) override {
        if(index < 0 || index >= list.size()) return nullptr;
        return list.takeAt(index);
    }

private:

    QList<QLayoutItem*> list;
};

}   // End of namespace


