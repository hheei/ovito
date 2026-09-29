// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/rendering/SceneRenderer.h>

namespace Ovito {

/**
 * \brief Knows the interactive viewport renderers of the application.
 *
 * The scene renderer that the viewport windows use is chosen by the user (the graphics API of the classic frontend's
 * "Adjust View" dialog) and can be substituted by another implementation, for example an ANARI/VisRTX renderer that a
 * plugin provides. This class holds what both frontends need for that: the renderer implementations available in this
 * build, their display names, the user's selection and one instance per implementation, which carries the settings the
 * user made in the renderer's own property editor.
 *
 * A renderer instance does not render anything itself: it describes the settings of the interactive viewports, and each
 * viewport window creates a rendering implementation from it (see RendererService). The instance is shared by all
 * viewport windows of the application, so changing a setting of the renderer updates every viewport.
 *
 * The workbench of a frontend hands the instance to the viewport windows it creates and follows the signal emitted when
 * the user selects another renderer; if a window reports a fatal error, the frontend reverts to the default renderer,
 * which is what the classic frontend has always done and what makes a broken third-party renderer recoverable.
 */
class OVITO_GUIBASE_EXPORT ViewportRendererRegistry : public QObject
{
    Q_OBJECT

public:

    /// Describes one renderer implementation the user can choose from.
    struct RendererInfo
    {
        /// Identifies the implementation, e.g. in the environment variable and in the settings store.
        QString id;

        /// The name presented to the user.
        QString displayName;

        /// The class of the renderer implementation, or null if it is not available in this build (the entry is then
        /// shown but cannot be selected).
        OvitoClassPtr rendererClass;
    };

    /// Returns the application-wide instance of the registry.
    static ViewportRendererRegistry& instance();

    /// Returns the renderer implementations available in this build, in the order they are presented to the user.
    std::vector<RendererInfo> availableRenderers() const;

    /// Returns the identifier of the renderer the user selected. This is the environment variable OVITO_VIEWPORT_RENDERER
    /// if it is set, and the renderer stored in the application settings otherwise.
    QString selectedRendererId() const;

    /// Selects the renderer to use and stores the selection in the application settings.
    /// \return \c true if the selection changed, \c false if the given renderer was already selected.
    bool setSelectedRendererId(const QString& id);

    /// Switches back to the default renderer for interactive viewports, i.e. forgets the user's selection.
    /// \return \c true if there was a selection to revert, \c false if the default renderer is already in use.
    bool revertToDefaultRenderer();

    /// Returns the instance of the given renderer, or of the selected one if no identifier is given.
    /// The settings of the instance are loaded from the application settings when it is created for the first time.
    /// \return The renderer instance, or null if an explicitly requested implementation does not exist in this build.
    /// The selected renderer falls back to the default renderer when it is not available.
    OORef<SceneRenderer> renderer(const QString& id = {});

    /// Stores the settings of all renderer instances that have been created in the application settings.
    void saveRendererSettings();

Q_SIGNALS:

    /// Emitted when the user selected another renderer. The viewport windows have to be given the new instance
    /// (see renderer()) when this signal arrives.
    void rendererSelectionChanged();

private:

    /// Constructor.
    ViewportRendererRegistry() = default;

    /// The instance of the requested renderer implementation, or null if there is no such implementation.
    OORef<SceneRenderer> loadRenderer(const QString& id);

    /// Returns the instance of the given renderer implementation, creating it when it does not exist yet.
    OORef<SceneRenderer> cachedRenderer(const QString& id);

    /// The instance of every renderer implementation that has been created so far, indexed by identifier.
    std::map<QString, OORef<SceneRenderer>> _renderers;
};

}   // End of namespace
