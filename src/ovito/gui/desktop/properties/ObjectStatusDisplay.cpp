// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include "ObjectStatusDisplay.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ObjectStatusDisplay);
DEFINE_REFERENCE_FIELD(ObjectStatusDisplay, activeObject);

/******************************************************************************
* Constructor.
******************************************************************************/
void ObjectStatusDisplay::initializeObject(PropertiesEditor* parentEditor)
{
    ParameterUI::initializeObject(parentEditor);
    _widget = new StatusWidget();
}

/******************************************************************************
* Destructor.
******************************************************************************/
ObjectStatusDisplay::~ObjectStatusDisplay()
{
    // Release GUI widget.
    delete statusWidget();
}

/******************************************************************************
* Returns the UI widget managed by this ParameterUI.
******************************************************************************/
StatusWidget* ObjectStatusDisplay::statusWidget() const
{
    return _widget;
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void ObjectStatusDisplay::resetUI()
{
    ParameterUI::resetUI();

    // Determine the active object. Consider all nested editors.
    ActiveObject* activeObject = dynamic_object_cast<ActiveObject>(editObject());
    if(!activeObject) {
        PropertiesEditor* editor = this->editor()->parentEditor();
        while(editor) {
            activeObject = dynamic_object_cast<ActiveObject>(editor->editObject());
            if(activeObject)
                break;
            editor = editor->parentEditor();
        }
    }
    _activeObject.set(this, PROPERTY_FIELD(activeObject), activeObject);
    _updateTimer.stop();
    _isUpToDate = true;

    if(statusWidget()) {
        if(activeObject) {
            statusWidget()->setStatus(activeObject->status());
        }
        else {
            statusWidget()->clearStatus();
        }
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void ObjectStatusDisplay::setEnabled(bool enabled)
{
    if(enabled == isEnabled())
        return;
    ParameterUI::setEnabled(enabled);
    if(statusWidget())
        statusWidget()->setEnabled(editObject() && isEnabled());
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool ObjectStatusDisplay::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == activeObject() && event.type() == ReferenceEvent::ObjectStatusChanged) {
        if(!_updateTimer.isActive()) {
            if(statusWidget())
                statusWidget()->setStatus(activeObject()->status());
            _isUpToDate = true;
            _updateTimer.start(100, Qt::CoarseTimer, this);
        }
        else {
            _isUpToDate = false;
        }
    }
    return ParameterUI::referenceEvent(source, event);
}

/******************************************************************************
* Handles timer events for this object.
******************************************************************************/
void ObjectStatusDisplay::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == _updateTimer.timerId()) {
        OVITO_ASSERT(_updateTimer.isActive());
        if(_isUpToDate)
            _updateTimer.stop();
        else if(statusWidget())
            statusWidget()->setStatus(activeObject() ? activeObject()->status() : PipelineStatus());
        _isUpToDate = true;
    }
    ParameterUI::timerEvent(event);
}

}   // End of namespace
