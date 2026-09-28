// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/export/FileColumnParticleExporter.h>
#include <ovito/stdobj/io/PropertyOutputWriter.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * \brief User interface component for the FileColumnParticleExporter class.
 */
class FileColumnParticleExporterEditor : public PropertiesEditor
{
    OVITO_CLASS(FileColumnParticleExporterEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private Q_SLOTS:

    /// Updates the displayed list of particle properties that are available for export.
    void updateParticlePropertiesList();

    /// Is called when the user checked/unchecked an item in the particle property list.
    void onParticlePropertyItemChanged();

private:

    /// Populates the column mapping list box with an entry.
    void insertPropertyItem(const PropertyReference& propRef, const QString& displayName, const OutputColumnMapping& columnMapping);

    /// This writes the settings made in the UI back to the exporter.
    void saveChanges(FileColumnParticleExporter* particleExporter);

    QListWidget* _columnMappingWidget;
};

}   // End of namespace
