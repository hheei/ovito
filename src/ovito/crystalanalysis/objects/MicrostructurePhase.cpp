// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include "MicrostructurePhase.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(MicrostructurePhase);
DEFINE_PROPERTY_FIELD(MicrostructurePhase, shortName);
DEFINE_PROPERTY_FIELD(MicrostructurePhase, dimensionality);
DEFINE_PROPERTY_FIELD(MicrostructurePhase, crystalSymmetryClass);
DEFINE_VECTOR_REFERENCE_FIELD(MicrostructurePhase, burgersVectorFamilies);
DEFINE_SNAPSHOT_PROPERTY_FIELD(MicrostructurePhase, shortName);
DEFINE_SNAPSHOT_PROPERTY_FIELD(MicrostructurePhase, dimensionality);
DEFINE_SNAPSHOT_PROPERTY_FIELD(MicrostructurePhase, crystalSymmetryClass);
SET_PROPERTY_FIELD_LABEL(MicrostructurePhase, shortName, "Short name");
SET_PROPERTY_FIELD_LABEL(MicrostructurePhase, dimensionality, "Dimensionality");
SET_PROPERTY_FIELD_LABEL(MicrostructurePhase, crystalSymmetryClass, "Symmetry class");
SET_PROPERTY_FIELD_LABEL(MicrostructurePhase, burgersVectorFamilies, "Burgers vector families");

/******************************************************************************
* Returns the display color to be used for a given Burgers vector.
******************************************************************************/
Color MicrostructurePhase::getBurgersVectorColor(const QString& latticeName, const Cluster::VecType& b)
{
    if(latticeName == ParticleType::getPredefinedStructureTypeName(ParticleType::PredefinedStructureType::BCC)) {
        return getBurgersVectorColor(ParticleType::PredefinedStructureType::BCC, b);
    }
    else if(latticeName == ParticleType::getPredefinedStructureTypeName(ParticleType::PredefinedStructureType::FCC)) {
        return getBurgersVectorColor(ParticleType::PredefinedStructureType::FCC, b);
    }
    return getBurgersVectorColor(ParticleType::PredefinedStructureType::OTHER, b);
}

/******************************************************************************
* Returns the display color to be used for a given Burgers vector.
******************************************************************************/
Color MicrostructurePhase::getBurgersVectorColor(ParticleType::PredefinedStructureType structureType, const Cluster::VecType& b)
{
    if(structureType == ParticleType::PredefinedStructureType::BCC) {
        static constexpr Color predefinedLineColors[] = {
                Color(0.4f,1.0f,0.4f),
                Color(1.0f,0.2f,0.2f),
                Color(0.4f,0.4f,1.0f),
                Color(0.9f,0.5f,0.0f),
                Color(1.0f,1.0f,0.0f),
                Color(1.0f,0.4f,1.0f),
                Color(0.7f,0.0f,1.0f)
        };
        static constexpr Cluster::VecType burgersVectors[] = {
                { FloatType(0.5), FloatType(0.5), FloatType(0.5) },
                { FloatType(-0.5), FloatType(0.5), FloatType(0.5) },
                { FloatType(0.5), FloatType(-0.5), FloatType(0.5) },
                { FloatType(0.5), FloatType(0.5), FloatType(-0.5) },
                { FloatType(1.0), FloatType(0.0), FloatType(0.0) },
                { FloatType(0.0), FloatType(1.0), FloatType(0.0) },
                { FloatType(0.0), FloatType(0.0), FloatType(1.0) }
        };
        OVITO_STATIC_ASSERT(std::size(burgersVectors) == std::size(predefinedLineColors));
        for(size_t i = 0; i < std::size(burgersVectors); i++) {
            if(b.equals(burgersVectors[i], GraphicsFloatType(1e-6)) || b.equals(-burgersVectors[i], GraphicsFloatType(1e-6)))
                return predefinedLineColors[i];
        }
    }
    else if(structureType == ParticleType::PredefinedStructureType::FCC) {
        static constexpr Color predefinedLineColors[] = {
                Color(230.0/255.0, 25.0/255.0, 75.0/255.0),
                Color(245.0/255.0, 130.0/255.0, 48.0/255.0),
                Color(255.0/255.0, 225.0/255.0, 25.0/255.0),
                Color(210.0/255.0, 245.0/255.0, 60.0/255.0),
                Color(60.0/255.0, 180.0/255.0, 75.0/255.0),
                Color(70.0/255.0, 240.0/255.0, 240.0/255.0),
                Color(0.0/255.0, 130.0/255.0, 200.0/255.0),
                Color(145.0/255.0, 30.0/255.0, 180.0/255.0),
                Color(240.0/255.0, 50.0/255.0, 230.0/255.0),
                Color(0.0/255.0, 128.0/255.0, 128.0/255.0),
                Color(170.0/255.0, 110.0/255.0, 40.0/255.0),
                Color(128.0/255.0, 128.0/255.0, 0.0/255.0),

                Color(0.5f,0.5f,0.5f),
                Color(0.5f,0.5f,0.5f),
                Color(0.5f,0.5f,0.5f),
                Color(0.5f,0.5f,0.5f),
                Color(0.5f,0.5f,0.5f),
                Color(0.5f,0.5f,0.5f),
        };
        static constexpr Cluster::VecType burgersVectors[] = {
                { GraphicsFloatType(1.0/6.0), GraphicsFloatType(-2.0/6.0), GraphicsFloatType(-1.0/6.0) },
                { GraphicsFloatType(1.0/6.0), GraphicsFloatType(-2.0/6.0), GraphicsFloatType(1.0/6.0) },
                { GraphicsFloatType(1.0/6.0), GraphicsFloatType(-1.0/6.0), GraphicsFloatType(2.0/6.0) },
                { GraphicsFloatType(1.0/6.0), GraphicsFloatType(-1.0/6.0), GraphicsFloatType(-2.0/6.0) },
                { GraphicsFloatType(1.0/6.0), GraphicsFloatType(1.0/6.0), GraphicsFloatType(2.0/6.0) },
                { GraphicsFloatType(1.0/6.0), GraphicsFloatType(1.0/6.0), GraphicsFloatType(-2.0/6.0) },
                { GraphicsFloatType(1.0/6.0), GraphicsFloatType(2.0/6.0), GraphicsFloatType(1.0/6.0) },
                { GraphicsFloatType(1.0/6.0), GraphicsFloatType(2.0/6.0), GraphicsFloatType(-1.0/6.0) },
                { GraphicsFloatType(2.0/6.0), GraphicsFloatType(-1.0/6.0), GraphicsFloatType(-1.0/6.0) },
                { GraphicsFloatType(2.0/6.0), GraphicsFloatType(-1.0/6.0), GraphicsFloatType(1.0/6.0) },
                { GraphicsFloatType(2.0/6.0), GraphicsFloatType(1.0/6.0), GraphicsFloatType(-1.0/6.0) },
                { GraphicsFloatType(2.0/6.0), GraphicsFloatType(1.0/6.0), GraphicsFloatType(1.0/6.0) },

                { GraphicsFloatType(0), GraphicsFloatType(1.0/6.0), GraphicsFloatType(1.0/6.0) },
                { GraphicsFloatType(0), GraphicsFloatType(1.0/6.0), GraphicsFloatType(-1.0/6.0) },
                { GraphicsFloatType(1.0/6.0), GraphicsFloatType(0), GraphicsFloatType(1.0/6.0) },
                { GraphicsFloatType(1.0/6.0), GraphicsFloatType(0), GraphicsFloatType(-1.0/6.0) },
                { GraphicsFloatType(1.0/6.0), GraphicsFloatType(1.0/6.0), GraphicsFloatType(0) },
                { GraphicsFloatType(1.0/6.0), GraphicsFloatType(-1.0/6.0), GraphicsFloatType(0) },
        };
        OVITO_STATIC_ASSERT(std::size(burgersVectors) == std::size(predefinedLineColors));
        for(size_t i = 0; i < std::size(burgersVectors); i++) {
            if(b.equals(burgersVectors[i], GraphicsFloatType(1e-6)) || b.equals(-burgersVectors[i], GraphicsFloatType(1e-6)))
                return predefinedLineColors[i];
        }
    }
    return Color(0.9f, 0.9f, 0.9f);
}

}   // End of namespace
