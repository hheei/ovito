// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/base/viewport/ViewportRendererRegistry.h>
#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/viewport/ViewportWindow.h>
#include <ovito/gui/desktop/dialogs/SystemInformationDialog.h>
#include <ovito/gui/desktop/dialogs/MessageDialog.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/mainwin/ViewportsPanel.h>
#include <ovito/gui/desktop/properties/PropertiesPanel.h>
#include <ovito/gui/desktop/app/GuiApplication.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/rendering/RenderThread.h>
#include "ConfigureViewportGraphicsDialog.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
ConfigureViewportGraphicsDialog::ConfigureViewportGraphicsDialog(MainWindowUI& ui, QWidget* parent) :
    QDockWidget(tr("Viewport Graphics Configuration"), parent),
    UserInterfaceComponent<MainWindowUI>(ui)
{
    setFeatures(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    setAllowedAreas(Qt::NoDockWidgetArea);
    setFloating(true);
    setAttribute(Qt::WA_DeleteOnClose); // Make sure this object gets deleted when the dialog window is closed by the user.

    QWidget* widget = new QWidget();
    setWidget(widget);

    QVBoxLayout* mainLayout = new QVBoxLayout(widget);

    // GPU adapter selection.
    QGroupBox* gpuAdapterGroupBox = new QGroupBox(tr("GPU adapter"), widget);
    mainLayout->addWidget(gpuAdapterGroupBox);
    QGridLayout* adapterLayout = new QGridLayout(gpuAdapterGroupBox);
    adapterLayout->setColumnStretch(1, 1);

    _gpuAdapterCombo = new QComboBox(gpuAdapterGroupBox);
    adapterLayout->addWidget(_gpuAdapterCombo, 0, 0, 1, 2);

    // Enumerate available adapters (supported for Vulkan, D3D11, D3D12).
    QRhi::Implementation graphicsApi = RenderThread::pickGraphicsApi();
    QList<QRhiDriverInfo> adapters = RenderThread::enumerateAdapters(graphicsApi);
    QString noteText;
    if(!adapters.isEmpty()) {
        _gpuAdapterCombo->addItem(tr("(Default)"), QByteArray());
        QByteArray currentSelection = RenderThread::selectedAdapterName();
        int selectedIndex = 0;
        for(int i = 0; i < adapters.size(); i++) {
            const QRhiDriverInfo& info = adapters[i];
            QString label = QString::fromUtf8(info.deviceName);
            // Append device type for disambiguation.
            switch(info.deviceType) {
            case QRhiDriverInfo::IntegratedDevice: label += tr(" (Integrated)"); break;
            case QRhiDriverInfo::DiscreteDevice:   label += tr(" (Discrete)"); break;
            case QRhiDriverInfo::CpuDevice:        label += tr(" (Software)"); break;
            default: break;
            }
            _gpuAdapterCombo->addItem(label, info.deviceName);
            if(info.deviceName == currentSelection)
                selectedIndex = i + 1; // +1 because index 0 is "(Default)"
        }
        _gpuAdapterCombo->setCurrentIndex(selectedIndex);
        noteText = tr("Selecting \"(Default)\" lets your operating system choose the graphics adapter "
               "automatically. On systems with multiple GPUs, this may not always select the "
               "most powerful adapter.");
    }
    else {
        // Adapter enumeration not available for this graphics API (e.g. Metal, OpenGL).
        _gpuAdapterCombo->addItem(tr("(Default)"), QByteArray());
        _gpuAdapterCombo->setEnabled(false);
        noteText = tr("On this system, the OS manages the graphics adapter automatically "
               "and OVITO cannot select a specific one.");
    }

    QLabel* noteLabel = new QLabel(noteText);
    noteLabel->setWordWrap(true);
    QFont noteFont = noteLabel->font();
    noteFont.setPointSizeF(noteFont.pointSizeF() * 0.85);
    noteLabel->setFont(noteFont);
    adapterLayout->addWidget(noteLabel, 1, 0, 1, 2);

    QGroupBox* backendSelectionBox = new QGroupBox(tr("Real-time rendering method"), widget);
    mainLayout->addWidget(backendSelectionBox);

    QGridLayout* gridLayout = new QGridLayout(backendSelectionBox);
    gridLayout->setColumnStretch(0, 1);

    _backendSettingsStack = new QStackedWidget(this);
    mainLayout->addWidget(_backendSettingsStack, 1);

    // Create a radio button for each available rendering backend.
    _backendSelectionGroup = new QButtonGroup(this);
    int index = 0;
    for(const auto& [id, label, rendererClass] : ViewportRendererRegistry::instance().availableRenderers()) {
        QRadioButton* option = new QRadioButton(label);
        option->setEnabled(rendererClass);
        option->setProperty("graphics_api", id);
        gridLayout->addWidget(option, index, 0);
        _backendSelectionGroup->addButton(option, index);

        handleExceptions([&]() {
            // Create a settings panel for the rendering backend.
            if(OORef<SceneRenderer> rendererInstance = ViewportRendererRegistry::instance().renderer(id)) {
                PropertiesPanel* propertiesPanel = new PropertiesPanel(ui);
                propertiesPanel->setEditObject(rendererInstance);
                if(propertiesPanel->editor()) {
                    connect(ui.mainWindow(), &MainWindow::closingWindow, propertiesPanel, &PropertiesPanel::close);
                    propertiesPanel->setFrameStyle(QFrame::NoFrame);
                    int stackIndex = _backendSettingsStack->addWidget(propertiesPanel);
                    _backendSettingsMap.emplace(id, stackIndex);
                }
                else {
                    delete propertiesPanel;
                }
            }
        });

        index++;
    }

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close | QDialogButtonBox::Help, Qt::Horizontal, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ConfigureViewportGraphicsDialog::close);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &ConfigureViewportGraphicsDialog::close);
    connect(buttonBox, &QDialogButtonBox::helpRequested, this, [this]() {
        actionManager()->openHelpTopic(QStringLiteral("manual:viewports.configure_graphics_dialog"));
    });
    connect(buttonBox->addButton(tr("System information..."), QDialogButtonBox::ActionRole), &QPushButton::clicked, this, [this]() {
        handleExceptions([&] {
            SystemInformationDialog(this->ui(), this).exec();
        });
    });
    mainLayout->addWidget(buttonBox);

    updateGUI();

    connect(ui.mainWindow()->viewportsPanel(), &ViewportsPanel::interactiveViewportRendererChanged, this, &ConfigureViewportGraphicsDialog::updateGUI);
    connect(_backendSelectionGroup, &QButtonGroup::buttonToggled, this, &ConfigureViewportGraphicsDialog::backendSelectionChanged);
    connect(_gpuAdapterCombo, &QComboBox::currentIndexChanged, this, &ConfigureViewportGraphicsDialog::adapterSelectionChanged);
    connect(ui.mainWindow(), &MainWindow::closingWindow, this, &QWidget::close);
}

/******************************************************************************
* Updates the values displayed in the dialog.
******************************************************************************/
void ConfigureViewportGraphicsDialog::updateGUI()
{
    handleExceptions([&]() {
        QString selectedGraphicsApi = ViewportRendererRegistry::instance().selectedRendererId();
        for(QAbstractButton* option : _backendSelectionGroup->buttons()) {
            if(option->isEnabled() && selectedGraphicsApi.compare(option->property("graphics_api").toString(), Qt::CaseInsensitive) == 0)
                option->setChecked(true);
        }

        // Automatically switch back to standard renderer if the currently selected renderer is not available anymore.
        if(_backendSelectionGroup->checkedId() == -1) {
            _backendSelectionGroup->button(0)->setChecked(true);
            selectedGraphicsApi = "opengl";
        }

        // Show the properties editor for the selected rendering backend.
        if(auto it = _backendSettingsMap.find(selectedGraphicsApi); it != _backendSettingsMap.end()) {
            _backendSettingsStack->setCurrentIndex(it->second);
        }
        else {
            _backendSettingsStack->setCurrentIndex(-1);
        }
    });
}

/******************************************************************************
* Is called when the dialog window is being closed by the user.
******************************************************************************/
void ConfigureViewportGraphicsDialog::closeEvent(QCloseEvent* event)
{
    // Close property editors.
    for(int i = 0; i < _backendSettingsStack->count(); i++) {
        if(PropertiesPanel* propertiesPanel = qobject_cast<PropertiesPanel*>(_backendSettingsStack->widget(i))) {
            propertiesPanel->close();
        }
    }

    // Save renderer settings to application settings store.
    handleExceptions([&]() {
        ViewportRendererRegistry::instance().saveRendererSettings();
    });

    QDockWidget::closeEvent(event);
}

/******************************************************************************
* Is called when the user selects a different GPU adapter.
******************************************************************************/
void ConfigureViewportGraphicsDialog::adapterSelectionChanged()
{
    if(!_gpuAdapterCombo)
        return;
    QByteArray newAdapterName = _gpuAdapterCombo->currentData().toByteArray();
    if(newAdapterName == RenderThread::selectedAdapterName())
        return;

    QSettings settings;
    settings.setValue(QLatin1String(RenderThread::adapterSettingsKey()), newAdapterName);

    // Recreate all viewport windows to restart all RenderThreads and reinitialize the QRhi instances.
    MainWindow::visitMainWindows([&](MainWindow* mainWindow) {
        mainWindow->viewportsPanel()->recreateViewportWindows();
    });
}

/******************************************************************************
* Is called when the user selects a different renderer for the interactive viewports.
******************************************************************************/
void ConfigureViewportGraphicsDialog::backendSelectionChanged(QAbstractButton* option, bool checked)
{
    if(checked) {

        // When switching to ANARI, suggest the NVIDIA adapter for optimal GPU-to-GPU transfer performance with VisRTX.
        if(option->property("graphics_api").toString().compare(QStringLiteral("anari"), Qt::CaseInsensitive) == 0 && _gpuAdapterCombo) {
            QByteArray currentAdapter = _gpuAdapterCombo->currentData().toByteArray();
            int nvidiaIndex = -1;
            for(int i = 0; i < _gpuAdapterCombo->count(); i++) {
                QByteArray adapterName = _gpuAdapterCombo->itemData(i).toByteArray();
                if(adapterName != currentAdapter && QString::fromUtf8(adapterName).contains(QLatin1String("NVIDIA"), Qt::CaseInsensitive)) {
                    nvidiaIndex = i;
                    break;
                }
            }
            if(nvidiaIndex != -1) {
                QString nvidiaName = _gpuAdapterCombo->itemText(nvidiaIndex);
                int result = MessageDialog::question(this,
                    tr("Switch to NVIDIA adapter for best performance?"),
                    tr("The ANARI renderer uses NVIDIA's VisRTX engine, which renders the scene using "
                       "hardware ray tracing directly on the NVIDIA GPU.\n\n"
                       "When the same NVIDIA GPU (%1) is also used for displaying the viewport, rendered "
                       "frames can be transferred directly within the GPU — without ever passing through "
                       "system memory. This significantly reduces per-frame overhead and improves interactive "
                       "viewport performance, especially at high resolutions.\n\n"
                       "With the currently selected display adapter, each rendered frame must first be "
                       "copied from NVIDIA GPU memory to system memory, and then re-uploaded to the display "
                       "adapter. This round-trip adds latency that becomes noticeable during interactive use.\n\n"
                       "Switch the display adapter to %1 now?").arg(nvidiaName));
                if(result == QMessageBox::Yes)
                    _gpuAdapterCombo->setCurrentIndex(nvidiaIndex);
            }
        }

        handleExceptions([&]() {
            // The viewport panels of all main windows follow the ViewportRendererRegistry, which announces the new
            // selection, so assigning the renderer to the viewport windows is not this dialog's business.
            ViewportRendererRegistry::instance().setSelectedRendererId(option->property("graphics_api").toString());
        });
        updateGUI();
    }
}

}   // End of namespace
