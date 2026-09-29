// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/base/app/GuiSettings.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/rendering/RenderThread.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/mainwin/ViewportsPanel.h>
#include "NewGraphicsSystemDialog.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(NewGraphicsSystemService);

/******************************************************************************
 * Modal dialog shown on the first startup with OVITO's new native graphics
 * rendering engine. Lets the user select a preferred GPU adapter.
 ******************************************************************************/
class NewGraphicsSystemDialog : public QDialog
{
public:

    explicit NewGraphicsSystemDialog(QWidget* parent = nullptr) : QDialog(parent)
    {
        setWindowTitle(tr("GPU Adapter Selection"));

        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setSpacing(12);

#if defined(Q_OS_WIN)
        const QString apiName = tr("Direct3D 12");
        const QString osName = tr("Windows");
#elif QT_CONFIG(metal)
        const QString apiName = tr("Metal");
        const QString osName = tr("macOS");
#elif QT_CONFIG(vulkan) && defined(Q_OS_LINUX)
        const QString apiName = tr("Vulkan");
        const QString osName = tr("Linux");
#else
        const QString apiName = tr("OpenGL");
        const QString osName = QOperatingSystemVersion::current().name();
#endif

        QLabel* introLabel = new QLabel(
            tr("Since version 3.16.0, OVITO uses a new rendering engine for the interactive viewports, "
               "powered by %1 on %2. This brings improved performance and support for modern graphics "
               "hardware compared to the previous OpenGL-based renderer. "
               "Please report any issues to the developers at support@ovito.org.").arg(apiName, osName), this);
        introLabel->setWordWrap(true);
        mainLayout->addWidget(introLabel);

        mainLayout->addSpacing(8);

        // Enumerate available adapters (supported for Vulkan, D3D11, D3D12).
        QRhi::Implementation graphicsApi = RenderThread::pickGraphicsApi();
        QList<QRhiDriverInfo> adapters = RenderThread::enumerateAdapters(graphicsApi);

        QHBoxLayout* adapterRowLayout = new QHBoxLayout();
        adapterRowLayout->addWidget(new QLabel(tr("Selected GPU adapter:"), this));
        _gpuAdapterCombo = new QComboBox(this);
        adapterRowLayout->addWidget(_gpuAdapterCombo, 1);
        mainLayout->addLayout(adapterRowLayout);

        QString noteText;
        if(!adapters.isEmpty()) {
            _gpuAdapterCombo->addItem(tr("(Default)"), QByteArray());
            QByteArray currentSelection = RenderThread::selectedAdapterName();
            int selectedIndex = 0;
            for(int i = 0; i < adapters.size(); i++) {
                const QRhiDriverInfo& info = adapters[i];
                QString label = QString::fromUtf8(info.deviceName);
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
                   "most powerful adapter. You can change this selection at any time via "
                   "Edit → Application Settings → Viewports → Configure…");
        }
        else {
            // Adapter enumeration is not available for this graphics API (e.g. Metal on macOS).
            _gpuAdapterCombo->addItem(tr("(Default)"), QByteArray());
            _gpuAdapterCombo->setEnabled(false);

            noteText = tr("On this system, %1 manages the graphics adapter automatically "
                   "and OVITO cannot select a specific one.").arg(apiName);
        }

        QLabel* noteLabel = new QLabel(noteText);
        noteLabel->setWordWrap(true);
        QFont noteFont = noteLabel->font();
        noteFont.setPointSizeF(noteFont.pointSizeF() * 0.85);
        noteLabel->setFont(noteFont);
        mainLayout->addWidget(noteLabel);

        QDialogButtonBox* buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
        buttonBox->addButton(tr("Skip for Now"), QDialogButtonBox::RejectRole);
        QPushButton* confirmButton = buttonBox->addButton(tr("Confirm Selection"), QDialogButtonBox::AcceptRole);
        confirmButton->setDefault(true);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        mainLayout->addWidget(buttonBox);

        setMinimumWidth(480);
    }

    /// Returns the adapter device name selected by the user. Empty means "(Default)".
    QByteArray selectedAdapterName() const
    {
        if(_gpuAdapterCombo)
            return _gpuAdapterCombo->currentData().toByteArray();
        return QByteArray();
    }

private:

    QComboBox* _gpuAdapterCombo = nullptr; ///< Combo box for selecting the GPU adapter.
};

/******************************************************************************
 * Is called by the system during standalone application startup after the
 * main window has been created. Shows the first-run adapter selection dialog
 * if the user has not already confirmed a GPU adapter choice.
 ******************************************************************************/
void NewGraphicsSystemService::applicationStarting()
{
    // Do nothing when not started as a desktop application.
    if(Application::runMode() != Application::AppMode)
        return;

#if QT_CONFIG(metal) && defined(Q_OS_MACOS)
    // Adapter selection is not relevant on macOS with Metal, as the system manages this automatically.
    // No need to inform user about the new graphics system in this case, as it should "just work".
    return;
#endif

    // Check whether the user has already confirmed a GPU adapter selection. The settings facade owns the key, which
    // lives in a [viewport] section of the settings file.
    if(GuiSettings::instance().graphicsAdapterSetupDone())
        return;

    // Get a pointer to the current main window. Note that the running user interface is not necessarily the
    // classic desktop frontend; the Qt Quick frontend, for instance, has no MainWindow to attach this dialog to.
    const MainWindowUI* ui = dynamic_object_cast<MainWindowUI>(this_task::ui().get());
    if(ui == nullptr)
        return;
    MainWindow* mainWindow = ui->mainWindow();

    // Show the first-run GPU adapter selection dialog modally.
    NewGraphicsSystemDialog dialog(mainWindow);
    if(dialog.exec() == QDialog::Accepted) {
        // Save the selected adapter and mark setup as confirmed.
        QSettings writeSettings;
        writeSettings.setValue(QLatin1String(RenderThread::adapterSettingsKey()), dialog.selectedAdapterName());
        GuiSettings::instance().setGraphicsAdapterSetupDone(true);

        // Recreate all viewport windows so RenderThreads pick up the new adapter.
        MainWindow::visitMainWindows([](MainWindow* mw) {
            mw->viewportsPanel()->recreateViewportWindows();
        });
    }
    // If rejected ("Skip for Now"), do nothing -- dialog will appear again next startup.
}

}  // namespace Ovito
