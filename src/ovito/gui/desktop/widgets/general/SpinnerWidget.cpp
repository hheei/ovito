// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/base/viewport/ViewportInputMode.h>
#include "SpinnerWidget.h"

#include <QAccessible>
#include <QAccessibleWidget>

namespace Ovito {

namespace {

/**
 * Exposes SpinnerWidget's increment/decrement affordance and current value to assistive
 * technologies and UI automation tools. Without this, the widget is a custom-painted QWidget
 * with no accessible representation at all, so stepping its value (otherwise only reachable
 * by dragging or clicking with the mouse) is invisible to accessibility clients, even though
 * the text box it is normally paired with already exposes its own accessible name.
 */
class SpinnerWidgetAccessible : public QAccessibleWidget, public QAccessibleValueInterface
{
public:
    explicit SpinnerWidgetAccessible(QWidget* widget) : QAccessibleWidget(widget, QAccessible::SpinBox) {}

    QString text(QAccessible::Text t) const override
    {
        if(t == QAccessible::Name) {
            if(SpinnerWidget* spinner = qobject_cast<SpinnerWidget*>(object())) {
                if(!spinner->accessibleName().isEmpty())
                    return spinner->accessibleName();
                if(spinner->textBox())
                    return spinner->textBox()->accessibleName();
            }
        }
        return QAccessibleWidget::text(t);
    }

    void* interface_cast(QAccessible::InterfaceType t) override
    {
        if(t == QAccessible::ValueInterface)
            return static_cast<QAccessibleValueInterface*>(this);
        return QAccessibleWidget::interface_cast(t);
    }

    QStringList actionNames() const override
    {
        return QStringList{ increaseAction(), decreaseAction() } + QAccessibleWidget::actionNames();
    }

    void doAction(const QString& actionName) override
    {
        if(SpinnerWidget* spinner = qobject_cast<SpinnerWidget*>(object())) {
            if(actionName == increaseAction()) {
                spinner->stepUp();
                return;
            }
            if(actionName == decreaseAction()) {
                spinner->stepDown();
                return;
            }
        }
        QAccessibleWidget::doAction(actionName);
    }

    QVariant currentValue() const override
    {
        if(SpinnerWidget* spinner = qobject_cast<SpinnerWidget*>(object()))
            return spinner->floatValue();
        return QVariant();
    }

    void setCurrentValue(const QVariant& value) override
    {
        if(SpinnerWidget* spinner = qobject_cast<SpinnerWidget*>(object()))
            spinner->setFloatValue(value.value<FloatType>(), true);
    }

    QVariant minimumValue() const override
    {
        if(SpinnerWidget* spinner = qobject_cast<SpinnerWidget*>(object())) {
            if(spinner->minValue() != FLOATTYPE_MIN)
                return spinner->minValue();
        }
        return QVariant();
    }

    QVariant maximumValue() const override
    {
        if(SpinnerWidget* spinner = qobject_cast<SpinnerWidget*>(object())) {
            if(spinner->maxValue() != FLOATTYPE_MAX)
                return spinner->maxValue();
        }
        return QVariant();
    }

    QVariant minimumStepSize() const override
    {
        if(SpinnerWidget* spinner = qobject_cast<SpinnerWidget*>(object()))
            return spinner->unit() ? spinner->unit()->stepSize(spinner->floatValue(), true) : FloatType(1);
        return QVariant();
    }
};

QAccessibleInterface* spinnerWidgetAccessibleFactory(const QString&, QObject* object)
{
    if(SpinnerWidget* spinner = qobject_cast<SpinnerWidget*>(object))
        return new SpinnerWidgetAccessible(spinner);
    return nullptr;
}

} // End of anonymous namespace

/******************************************************************************
* Constructs the spinner.
******************************************************************************/
SpinnerWidget::SpinnerWidget(QWidget* parent, QLineEdit* textBox) : QWidget(parent)
{
    static const bool accessibilityFactoryRegistered = []() {
        QAccessible::installFactory(spinnerWidgetAccessibleFactory);
        return true;
    }();
    Q_UNUSED(accessibilityFactoryRegistered);

    setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum, QSizePolicy::SpinBox));
    setFocusPolicy(Qt::ClickFocus);
    setTextBox(textBox);
}

/******************************************************************************
* Paint event handler.
******************************************************************************/
void SpinnerWidget::paintEvent(QPaintEvent* event)
{
    QStylePainter p(this);
    QStyleOptionSpinBox sboption;

    sboption.initFrom(this);
    sboption.state |= _upperBtnPressed ? QStyle::State_Sunken : QStyle::State_Raised;
    sboption.rect.setHeight(sboption.rect.height() / 2);
    p.drawPrimitive(QStyle::PE_PanelButtonTool, sboption);
    if(sboption.rect.width() > sboption.rect.height() * 3 / 2) {
        int d = (sboption.rect.width() - (sboption.rect.height() * 3 / 2)) / 2;
        sboption.rect.adjust(d, 0, -d, 0);
    }
    p.drawPrimitive(QStyle::PE_IndicatorSpinUp, sboption);

    sboption.initFrom(this);
    sboption.state |= _lowerBtnPressed ? QStyle::State_Sunken : QStyle::State_Raised;
    sboption.rect.setTop(sboption.rect.top() + sboption.rect.height() / 2);
    p.drawPrimitive(QStyle::PE_PanelButtonTool, sboption);
    if(sboption.rect.width() > sboption.rect.height() * 3 / 2) {
        int d = (sboption.rect.width() - (sboption.rect.height() * 3 / 2)) / 2;
        sboption.rect.adjust(d, 0, -d, 0);
    }
    p.drawPrimitive(QStyle::PE_IndicatorSpinDown, sboption);
}

/******************************************************************************
* Returns the natural size of the spinner widget.
******************************************************************************/
QSize SpinnerWidget::sizeHint() const
{
    if(textBox() == nullptr) return QSize(16, 30);
    else return QSize(16, textBox()->sizeHint().height());
}

/******************************************************************************
* Connects this spinner with the given textbox control.
******************************************************************************/
void SpinnerWidget::setTextBox(QLineEdit* box)
{
    if(box == textBox()) return;
    if(_textBox.isNull() == false) {
        disconnect(_textBox.data(), &QLineEdit::editingFinished, this, &SpinnerWidget::onTextChanged);
    }
    _textBox = box;
    if(_textBox.isNull() == false) {
        connect(box, &QLineEdit::editingFinished, this, &SpinnerWidget::onTextChanged);
        box->setEnabled(isEnabled());
        updateTextBox();
    }
}

/******************************************************************************
* Will be called when the user has entered a new text into the text box.
* The text will be parsed and taken as the new value of the spinner.
******************************************************************************/
void SpinnerWidget::onTextChanged()
{
    OVITO_CHECK_POINTER(textBox());

    try {
        if(textBox()->text() == _originalText) return;
        if(textBox()->text().isEmpty() && hasStandardValue()) {
            setFloatValue(standardValue(), true);
        }
        else if(unit()) {
            FloatType newValue = unit()->parseString(textBox()->text());
            setFloatValue(unit()->userToNative(newValue), true);
        }
        else {
            bool ok;
            FloatType newValue = textBox()->text().toDouble(&ok);
            if(!ok)
                throw Exception(tr("Invalid floating-point value: %1").arg(textBox()->text()));
            setFloatValue(newValue, true);
        }
    }
    catch(const Exception&) {
        // Ignore invalid value and restore old text content.
        updateTextBox();
    }
}

/******************************************************************************
* Updates the text of the connected text box after the spinner's value has changed.
******************************************************************************/
void SpinnerWidget::updateTextBox()
{
    if(textBox()) {
        if(floatValue() == standardValue() && !textBox()->placeholderText().isEmpty())
            _originalText.clear();
        else if(unit())
            _originalText = unit()->formatValue(unit()->nativeToUser(floatValue()));
        else
            _originalText = QString::number(floatValue());
        textBox()->setText(_originalText);
    }
}

/******************************************************************************
* Sets the current value of the spinner.
******************************************************************************/
void SpinnerWidget::setFloatValue(FloatType newVal, bool emitChangeSignal)
{
    if(newVal == _value)
        return;
    // Clamp value if it was entered by the user.
    if(emitChangeSignal) {
        newVal = std::max(minValue(), newVal);
        newVal = std::min(maxValue(), newVal);
    }
    if(_value != newVal) {
        _value = newVal;
        if(emitChangeSignal) {
#ifdef OVITO_DEBUG
            QPointer<SpinnerWidget> self(this);
#endif
            spinnerValueChanged();
#ifdef OVITO_DEBUG
            OVITO_ASSERT(!self.isNull());
#endif
        }
    }
    updateTextBox();
}

/******************************************************************************
* Sets the current integer value of the spinner.
******************************************************************************/
void SpinnerWidget::setIntValue(int newValInt, bool emitChangeSignal)
{
    FloatType newVal = (FloatType)newValInt;

    if(newVal == _value)
        return;
    // Clamp value if it was entered by the user.
    if(emitChangeSignal) {
        newVal = std::max((FloatType)std::ceil(minValue()), newVal);
        newVal = std::min((FloatType)std::floor(maxValue()), newVal);
    }
    if(_value != newVal) {
        _value = newVal;
        if(emitChangeSignal) {
#ifdef OVITO_DEBUG
            QPointer<SpinnerWidget> self(this);
#endif
            spinnerValueChanged();
#ifdef OVITO_DEBUG
            OVITO_ASSERT(!self.isNull());
#endif
        }
    }
    updateTextBox();
}

/******************************************************************************
* Sets the minimum allowed value of the spinner.
* If the current value of the spinner is less than the new minimum value,
* it will be set to the new minimum value.
******************************************************************************/
void SpinnerWidget::setMinValue(FloatType minValue)
{
    _minValue = minValue;
}

/******************************************************************************
* Sets the maximum allowed value of the spinner.
* If the current value of the spinner is greater than the new maximum value,
* it will be set to the new maximum value.
******************************************************************************/
void SpinnerWidget::setMaxValue(FloatType maxValue)
{
    _maxValue = maxValue;
}

/******************************************************************************
* Specifies the standard value that, if the spinner is set to this special value, should be highlighted in the input field.
******************************************************************************/
void SpinnerWidget::setStandardValue(FloatType value)
{
    _standardValue = value;
    updateTextBox();
}

/******************************************************************************
* Sets the units of this spinner's value.
******************************************************************************/
void SpinnerWidget::setUnit(ParameterUnit* unit)
{
    if(unit == _unit)
        return;

    if(_unit)
        disconnect(_unit, &ParameterUnit::formatChanged, this, &SpinnerWidget::updateTextBox);

    _unit = unit;

    if(_unit)
        connect(_unit, &ParameterUnit::formatChanged, this, &SpinnerWidget::updateTextBox);

    updateTextBox();
}

/******************************************************************************
* Handles the change events for the spinner.
******************************************************************************/
void SpinnerWidget::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if(event->type() == QEvent::EnabledChange) {
        if(textBox())
            textBox()->setEnabled(isEnabled());
    }
}

/******************************************************************************
* Handles the mouse down event.
******************************************************************************/
void SpinnerWidget::mousePressEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton && !_upperBtnPressed && !_lowerBtnPressed) {
        // Backup current value.
        _oldValue = floatValue();

        OVITO_ASSERT(_lowerBtnPressed == false && _upperBtnPressed == false);

        if(ViewportInputMode::getMousePosition(event).y() <= height()/2)
            _upperBtnPressed = true;
        else
            _lowerBtnPressed = true;

        _currentStepSize = unit() ? unit()->stepSize(floatValue(), _upperBtnPressed) : 1;

        event->accept();
        grabMouse();
        repaint();
    }
    else if(event->button() == Qt::RightButton) {

        if(_upperBtnPressed || _lowerBtnPressed) {
            // restore old value
            setFloatValue(_oldValue, true);
        }

        if(_upperBtnPressed && _lowerBtnPressed) {
#ifdef OVITO_DEBUG
            QPointer<SpinnerWidget> self(this);
#endif
            spinnerDragAbort();
#ifdef OVITO_DEBUG
            OVITO_ASSERT(!self.isNull());
#endif
        }

        _upperBtnPressed = false;
        _lowerBtnPressed = false;

        event->accept();
        releaseMouse();
        update();
    }
}

/******************************************************************************
* Handles the mouse up event.
******************************************************************************/
void SpinnerWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if(_upperBtnPressed || _lowerBtnPressed) {
        if(_upperBtnPressed && _lowerBtnPressed) {
#ifdef OVITO_DEBUG
            QPointer<SpinnerWidget> self(this);
#endif
            spinnerDragStop();
#ifdef OVITO_DEBUG
            OVITO_ASSERT(!self.isNull());
#endif
        }
        else {
            if(_upperBtnPressed)
                stepUp();
            else
                stepDown();
        }

        _upperBtnPressed = false;
        _lowerBtnPressed = false;
        if(textBox()) textBox()->setFocus(Qt::OtherFocusReason);

        // Repaint spinner.
        update();
        event->accept();
    }
    releaseMouse();
}

/******************************************************************************
* Increments the spinner value by one step, as if the upper spinner button had been clicked.
******************************************************************************/
void SpinnerWidget::stepUp()
{
    FloatType newValue;
    if(unit())
        newValue = unit()->roundValue(floatValue() + unit()->stepSize(floatValue(), true));
    else
        newValue = floatValue() + FloatType(1);
    setFloatValue(newValue, true);
}

/******************************************************************************
* Decrements the spinner value by one step, as if the lower spinner button had been clicked.
******************************************************************************/
void SpinnerWidget::stepDown()
{
    FloatType newValue;
    if(unit())
        newValue = unit()->roundValue(floatValue() - unit()->stepSize(floatValue(), false));
    else
        newValue = floatValue() - FloatType(1);
    setFloatValue(newValue, true);
}

/******************************************************************************
* Handles the mouse move event.
******************************************************************************/
void SpinnerWidget::mouseMoveEvent(QMouseEvent* event)
{
    if(_upperBtnPressed || _lowerBtnPressed) {
        if(_upperBtnPressed && !_lowerBtnPressed) {
            if(ViewportInputMode::getMousePosition(event).y() > height()/2 || ViewportInputMode::getMousePosition(event).y() < 0) {
                _lowerBtnPressed = true;
                _lastMouseY = _startMouseY = mapToGlobal(event->pos()).y();
                update();
                spinnerDragStart();
            }
        }
        else if(!_upperBtnPressed && _lowerBtnPressed) {
            if(ViewportInputMode::getMousePosition(event).y() <= height()/2 || ViewportInputMode::getMousePosition(event).y() > height()) {
                _upperBtnPressed = true;
                _lastMouseY = _startMouseY = mapToGlobal(event->pos()).y();
                update();
                spinnerDragStart();
            }
        }
        else {
            const QPoint cursorPos = QCursor::pos();
            const int screenY = cursorPos.y();
            if(screenY != _lastMouseY) {
                // Get current screen top and bottom positions
                // QT convention has greater Y values pointing down
                const int screenMaxY = screen()->geometry().bottom();
                const int screenMinY = screen()->geometry().top();
                OVITO_ASSERT(screenMinY < screenMaxY);
                // Distance from the edge at which warpping occurs
                constexpr int borderWidth = 5;

                if(screenY <= screenMinY + borderWidth && _lastMouseY == screenMaxY - 1) return;
                if(screenY >= screenMaxY - borderWidth && _lastMouseY == screenMinY) return;

                FloatType newVal = _oldValue + _currentStepSize * (FloatType)(_startMouseY - screenY) * FloatType(0.1);
                if(unit())
                    newVal = unit()->roundValue(newVal);

                // Accessibility access is required on macOS to move the mouse
                bool accessibilty = false;
                if(MainWindow* mainWindow = qobject_cast<MainWindow*>(window())) {
                    accessibilty = mainWindow->checkAccessibilityAccess(this);
                }

                if(accessibilty && screenY < _lastMouseY && screenY <= screenMinY + borderWidth) {
                    _lastMouseY = screenMaxY - 1;
                    _startMouseY += _lastMouseY - screenY;
                    QCursor::setPos(cursorPos.x(), _lastMouseY);
                }
                else if(accessibilty && screenY > _lastMouseY && screenY >= screenMaxY - borderWidth) {
                    _lastMouseY = screenMinY;
                    _startMouseY += _lastMouseY - screenY;
                    QCursor::setPos(cursorPos.x(), _lastMouseY);
                }
                else {
                    _lastMouseY = screenY;
                }

                if(newVal != floatValue()) {
                    setFloatValue(newVal, true);
                    if(textBox())
                        textBox()->update();
                }
            }
        }
        event->accept();
    }
}

/******************************************************************************
* Is called when the widgets looses the input focus.
******************************************************************************/
void SpinnerWidget::focusOutEvent(QFocusEvent* event)
{
    if(_upperBtnPressed && _lowerBtnPressed) {
#ifdef OVITO_DEBUG
        QPointer<SpinnerWidget> self(this);
#endif
        spinnerDragAbort();
#ifdef OVITO_DEBUG
        OVITO_ASSERT(!self.isNull());
#endif
    }
    _upperBtnPressed = false;
    _lowerBtnPressed = false;
    releaseMouse();

    QWidget::focusOutEvent(event);
}

/******************************************************************************
* Called after the spinner's value has been changed by the user.
******************************************************************************/
void SpinnerWidget::spinnerValueChanged()
{
    if(_userInterface) {
        // Create an undoable transaction to record the change or
        // use the existing transaction if the user is dragging the spinner.
        std::optional<UndoableTransaction> transaction;
        if(_undoTransaction.operation()) {
            auto val = _value;
            _undoTransaction.revert(); // Note: This may temporarily revert the spinner's value to an old value.
            _value = val;
        }
        else {
            transaction.emplace(*_userInterface, _undoOperationName);
        }

        // Perform the parameter change (including exception handling).
        bool success = _userInterface->performActions(transaction ? *transaction : _undoTransaction, [&]() {
            Q_EMIT valueChanged();
        });

        // Commit the transaction.
        if(success && transaction)
            transaction->commit();
    }
    else {
        Q_EMIT valueChanged();
    }
}

/******************************************************************************
* Called when the user has started a drag operation.
******************************************************************************/
void SpinnerWidget::spinnerDragStart()
{
    if(_userInterface)
        _undoTransaction.begin(*_userInterface, _undoOperationName);
}

/******************************************************************************
* Called when the user has finished the drag operation.
******************************************************************************/
void SpinnerWidget::spinnerDragStop()
{
    if(_undoTransaction.operation())
        _undoTransaction.commit();
}

/******************************************************************************
* Called when the user has aborted the drag operation.
******************************************************************************/
void SpinnerWidget::spinnerDragAbort()
{
    if(_undoTransaction.operation())
        _undoTransaction.cancel();
}

}   // End of namespace
