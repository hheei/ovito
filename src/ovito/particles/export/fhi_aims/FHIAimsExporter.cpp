// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/app/Application.h>
#include "FHIAimsExporter.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(FHIAimsExporter);

/******************************************************************************
* Creates a worker performing the actual data export.
*****************************************************************************/
OORef<FileExportJob> FHIAimsExporter::createExportJob(const QString& filePath, int numberOfFrames)
{
    class Job : public FileExportJob
    {
    public:

        /// Writes the exportable data of a single trajectory frame to the output file.
        virtual ScopedFuture<void> exportFrameData(boost::anys::unique_any frameData, int frameNumber, const QString& filePath,
                                               TaskProgress& progress) override
        {
            // The exportable frame data.
            OVITO_ASSERT(frameData.has_value());
            const auto state = boost::anys::any_cast<PipelineFlowState>(frameData);

            // Perform the following in a worker thread.
            co_await ExecutorAwaiter(ThreadPoolExecutor());

            // Get particle positions and types.
            const Particles* particles = state.expectObject<Particles>();
            BufferReadAccess<Point3> posProperty = particles->expectProperty(Particles::PositionProperty);
            const Property* particleTypeProperty = particles->getProperty(Particles::TypeProperty);
            BufferReadAccess<int32_t> particleTypeArray(particleTypeProperty);

            textStream() << "# FHI-aims file written by " << Application::applicationName() << " " << Application::applicationVersionString() << "\n";

            // Output simulation cell.
            Point3 origin = Point3::Origin();
            const SimulationCell* simulationCell = state.getObject<SimulationCell>();
            if(simulationCell) {
                origin = simulationCell->cellOrigin();
                if(simulationCell->pbcX() || simulationCell->pbcY() || simulationCell->pbcZ()) {
                    const AffineTransformation& cell = simulationCell->cellMatrix();
                    for(size_t i = 0; i < 3; i++)
                        textStream() << "lattice_vector " << cell(0, i) << ' ' << cell(1, i) << ' ' << cell(2, i) << '\n';
                }
            }

            // Output atoms.
            for(size_t i = 0; i < posProperty.size(); i++) {
                const Point3& p = posProperty[i];
                const ElementType* type = particleTypeArray ? particleTypeProperty->elementType(particleTypeArray[i]) : nullptr;

                textStream() << "atom " << (p.x() - origin.x()) << ' ' << (p.y() - origin.y()) << ' ' << (p.z() - origin.z());
                if(type && !type->name().isEmpty()) {
                    QString s = type->name();
                    textStream() << ' ' << s.replace(QChar(' '), QChar('_')) << '\n';
                }
                else if(particleTypeArray) {
                    textStream() << ' ' << particleTypeArray[i] << '\n';
                }
                else {
                    textStream() << " 1\n";
                }

                // Check for user cancellation.
                this_task::throwIfCanceled();
            }
        }
    };

    return OORef<Job>::create(this, filePath, true);
}

}   // End of namespace
