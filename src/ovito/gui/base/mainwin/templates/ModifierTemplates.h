// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include "ObjectTemplates.h"

namespace Ovito {

/**
 * \brief Manages the application-wide list of modifier templates.
 */
class OVITO_GUIBASE_EXPORT ModifierTemplates : public ObjectTemplates
{
    Q_OBJECT

private:

    /// \brief Constructor.
    ModifierTemplates(QObject* parent = nullptr);

public:

    /// \brief Returns the singleton instance of this class.
    static ModifierTemplates* get();

    /// \brief Creates a new template on the basis of the given modifier(s).
    /// \param templateName The name of the new template. If a template with the same name exists, it is overwritten.
    /// \param modifiers The list of one or more modifiers from which the template should be created.
    /// \return The index of the created template.
    int createTemplate(const QString& templateName, const QVector<OORef<Modifier>>& modifiers) {
        QVector<OORef<RefTarget>> objects;
        for(auto& mod : modifiers)
            objects.push_back(mod);
        return ObjectTemplates::createTemplate(templateName, objects);
    }

    /// \brief Instantiates the objects that are stored under the given template name.
    QVector<OORef<Modifier>> instantiateTemplate(const QString& templateName) {
        QVector<OORef<Modifier>> modifiers;
        for(auto& obj : ObjectTemplates::instantiateTemplate(templateName)) {
            if(OORef<Modifier> modifier = dynamic_object_cast<Modifier>(std::move(obj)))
                modifiers.push_back(std::move(modifier));
        }
        return modifiers;
    }
};

}   // End of namespace
