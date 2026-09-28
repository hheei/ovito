// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/viewport/overlays/ViewportOverlay.h>
#include "ObjectTemplates.h"

namespace Ovito {

/**
 * \brief Manages the application-wide list of viewport layer templates.
 */
class OVITO_GUIBASE_EXPORT OverlayTemplates : public ObjectTemplates
{
    Q_OBJECT

private:

    /// \brief Constructor.
    OverlayTemplates(QObject* parent = nullptr);

public:

    /// \brief Returns the singleton instance of this class.
    static OverlayTemplates* get();

    /// \brief Creates a new template on the basis of the given overlays(s).
    /// \param templateName The name of the new template. If a template with the same name exists, it is overwritten.
    /// \param modifiers The list of one or more overlays from which the template should be created.
    /// \return The index of the created template.
    int createTemplate(const QString& templateName, const QVector<OORef<ViewportOverlay>>& overlays) {
        QVector<OORef<RefTarget>> objects;
        for(auto& ov : overlays)
            objects.push_back(ov);
        return ObjectTemplates::createTemplate(templateName, objects);
    }

    /// \brief Instantiates the objects that are stored under the given template name.
    QVector<OORef<ViewportOverlay>> instantiateTemplate(const QString& templateName) {
        QVector<OORef<ViewportOverlay>> overlays;
        for(auto& obj : ObjectTemplates::instantiateTemplate(templateName)) {
            if(OORef<ViewportOverlay> ov = dynamic_object_cast<ViewportOverlay>(std::move(obj)))
                overlays.push_back(std::move(ov));
        }
        return overlays;
    }
};

}   // End of namespace
