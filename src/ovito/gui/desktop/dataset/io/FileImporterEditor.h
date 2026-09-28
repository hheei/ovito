// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * \brief Abstract base class for properties editors for FileImporter derived classes.
 */
class OVITO_GUI_EXPORT FileImporterEditor : public PropertiesEditor
{
    OVITO_CLASS(FileImporterEditor)

public:

    /// This is called by the system when the user has selected a new file to import.
    virtual void inspectNewFile(FileImporter* importer, const QUrl& sourceFile) {}
};

}   // End of namespace


