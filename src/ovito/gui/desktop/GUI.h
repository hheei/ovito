// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

//
// Standard precompiled header file included by all source files in this module
//

#ifndef __OVITO_GUI_
#define __OVITO_GUI_

#include <ovito/gui/base/GUIBase.h>

/******************************************************************************
 * Qt framework classes.
 ******************************************************************************/
#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QResource>
#include <QtWidgets>
#include <QtDebug>
#include <QtGui>
#include <QCommandLineParser>

/******************************************************************************
 * Forward declaration of classes.
 ******************************************************************************/
namespace Ovito
{
    class GuiAutoStartObject;
    class MainWindow;
    class MainWindowUI;
    class GuiApplication;
    class WidgetActionManager;
    class DataInspectionApplet;
    class PropertiesPanel;
    class SpinnerWidget;
    class ColorPickerWidget;
    class RolloutContainer;
    class FrameBufferWindow;
    class FrameBufferWidget;
    class TerminalBackend;
    class TerminalWidget;
    class AutocompleteTextEdit;
    class AutocompleteLineEdit;
    class StatusWidget;
    class ElidedTextLabel;
    class HtmlListWidget;
    class PropertiesEditor;
    class AffineTransformationParameterUI;
    class BooleanParameterUI;
    class BooleanActionParameterUI;
    class BooleanGroupBoxParameterUI;
    class BooleanRadioButtonParameterUI;
    class ColorParameterUI;
    class CustomParameterUI;
    class FilenameParameterUI;
    class FloatParameterUI;
    class FontParameterUI;
    class IntegerRadioButtonParameterUI;
    class IntegerParameterUI;
    class ParameterUI;
    class RefTargetListParameterUI;
    class StringParameterUI;
    class SubObjectParameterUI;
    class VariantComboBoxParameterUI;
    class VectorParameterUI;
    class FileExporterSettingsDialog;
    class CoordinateDisplayWidget;
    class CommandPanel;
    class DataInspectorPanel;
    class ModifyCommandPage;
    class RenderCommandPage;
    class OverlayCommandPage;
    class UtilityCommandPage;
    class ViewportMenu;
    class ViewportModeAction;
    class StatusBar;
    class MenuToolButton;
    class PopupUpdateComboBox;
    class ViewportsPanel;
    class WidgetViewportWindow;
}  // namespace Ovito

#include <ovito/gui/desktop/mainwin/MainWindowUI.h>

#endif  // __OVITO_GUI_
