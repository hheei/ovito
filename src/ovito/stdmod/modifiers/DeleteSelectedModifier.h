// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/stdmod/StdMod.h>
#include <ovito/core/dataset/pipeline/DelegatingModifier.h>

namespace Ovito {

/**
 * \brief Base class for DeleteSelectedModifier delegates that operate on different kinds of data.
 */
class OVITO_STDMOD_EXPORT DeleteSelectedModifierDelegate : public ModifierDelegate
{
    OVITO_CLASS(DeleteSelectedModifierDelegate)
};

/**
 * \brief This modifier deletes the currently selected elements.
 */
class OVITO_STDMOD_EXPORT DeleteSelectedModifier : public MultiDelegatingModifier
{
    /// Give this modifier class its own metaclass.
    class DeleteSelectedModifierClass : public MultiDelegatingModifier::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using MultiDelegatingModifier::OOMetaClass::OOMetaClass;

        /// Return the metaclass of delegates for this modifier type.
        virtual const ModifierDelegate::OOMetaClass& delegateMetaclass() const override { return DeleteSelectedModifierDelegate::OOClass(); }
    };

    OVITO_CLASS_META(DeleteSelectedModifier, DeleteSelectedModifierClass)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags) {
        MultiDelegatingModifier::initializeObject(flags);

        if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
            // Generate the list of delegate objects - with the particles delegate being the first one in the list.
            createModifierDelegates(DeleteSelectedModifierDelegate::OOClass(), {
                QStringLiteral("ParticlesDeleteSelectedModifierDelegate"),
                QStringLiteral("BondsDeleteSelectedModifierDelegate"),
                QStringLiteral("LinesDeleteSelectedModifierDelegate"),
                QStringLiteral("VectorsDeleteSelectedModifierDelegate"),
                QStringLiteral("SurfaceMeshRegionsDeleteSelectedModifierDelegate")
            });
        }
    }

    /// Indicates whether the interactive viewports should be updated after a parameter of the modifier has
    /// been changed and before the entire pipeline is recomputed.
    virtual bool shouldRefreshViewportsAfterChange() override { return true; }

    /// Returns a short piece of information (typically a string or color) to be displayed next to the modifier's title in the pipeline editor list.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override;
};

}   // End of namespace
