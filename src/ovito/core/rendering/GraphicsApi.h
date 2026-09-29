// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once

#include <ovito/core/Core.h>
#include <rhi/qrhi.h>

class QVulkanInstance;

namespace Ovito {

/**
 * \brief Static queries about the graphics backends and GPU adapters this machine offers.
 *
 * These queries answer questions that have to be answered *before* a rendering device exists: a RenderThread
 * uses them to pick its backend and its adapter, the graphics settings dialogs of the frontends use them to let
 * the user choose an adapter, and the application's system report lists them. They are deliberately not part of
 * RendererService, which knows only the backend and the device it has been created with, and not part of
 * RenderThread, which is merely one user of them (see audit decision A8.1 in docs/design/UI_PHASE0_AUDIT.md).
 *
 * None of the functions here require a render thread, a GPU resource or a connection to the scene graph, and
 * they may be called from the GUI thread.
 */
class OVITO_CORE_EXPORT GraphicsApi
{
public:

	/// Picks the graphics API to render with on the current platform.
	static QRhi::Implementation preferred();

	/// Enumerates the GPU adapters available for the given graphics API.
	/// Can be called on the GUI thread without creating a QRhi instance.
	static QList<QRhiDriverInfo> enumerateAdapters(QRhi::Implementation graphicsApi);

	/// Returns the name of the GPU adapter the user has selected, or an empty byte array to use the default
	/// adapter. The choice is read from the OVITO_GPU_ADAPTER environment variable, which takes precedence,
	/// or from the application settings.
	static QByteArray selectedAdapterName();

	/// The application settings key under which the user's adapter choice is stored.
	static constexpr const char* adapterSettingsKey() { return "viewport/gpu_adapter"; }

#if QT_CONFIG(vulkan) && defined(Q_OS_LINUX)
	/// Returns true if the Vulkan loader library itself is installed on this system. The loader and the
	/// driver are separate packages, and a diagnostic that cannot tell them apart sends the user to the
	/// wrong one.
	static bool isVulkanLoaderInstalled();

	/// Returns true if at least one Vulkan driver is installed on this system, i.e. if the loader finds an
	/// ICD manifest in one of the directories it searches.
	static bool isVulkanDriverInstalled();

	/// Makes OVITO's bundled software Vulkan driver available, but only on systems that provide no Vulkan
	/// driver of their own. Does nothing on all other platforms.
	static void prepareVulkanEnvironment();

	/// Requests the Vulkan API version OVITO's instances are created with.
	/// Instances that request no particular version make the loader hand out the oldest one, which the
	/// validation layers report as VUID-VkApplicationInfo-apiVersion.
	static void configureVulkanApiVersion(QVulkanInstance& instance);
#endif
};

}	// End of namespace
