// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/automation/AutomationSession.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/app/UserInterface.h>

namespace Ovito {

/******************************************************************************
* Constructs an empty session.
******************************************************************************/
AutomationSession::AutomationSession(QObject* parent) : QObject(parent)
{
}

AutomationSession::~AutomationSession() = default;

/******************************************************************************
* Replaces the data set.
******************************************************************************/
void AutomationSession::setDataSet(OORef<DataSet> dataSet)
{
    if(_dataSet == dataSet) {
        // The data set that is already current is not a change: the identities stay valid and the revision stays put.
        // A caller that means "something about the data set changed" reports that through the container connections.
        return;
    }
    _dataSet = std::move(dataSet);
    // A different data set: every ID handed out so far describes an object that this session no longer contains. The
    // registry forgets them without reusing their numbers, and the revision advances so a client recognizes that the
    // snapshot it based its request on is obsolete.
    _objects.invalidateAll();
    bumpRevision(QStringLiteral("the data set changed"));
}

/******************************************************************************
* Advances the revision because something a client can observe changed.
******************************************************************************/
quint64 AutomationSession::bumpRevision(QString cause)
{
    ++_revision;
    // The revision is the coarse answer to "what changed"; the event log is the precise one. Every advance is one
    // event, with the cause when the caller knows it, so a client that subscribes later can tell a data-set change from
    // a command's effect without diffing the session.
    _events.append(AutomationEvent(AutomationContract::EventKind::SessionChanged, 0)
                       .setRevision(_revision)
                       .setSummary(cause));
    return _revision;
}

/******************************************************************************
* Advances the revision because a container signal fired.
******************************************************************************/
void AutomationSession::sessionChanged(QString cause)
{
    bumpRevision(std::move(cause));
}

/******************************************************************************
* Registers a client of this session.
******************************************************************************/
QString AutomationSession::registerClient(AutomationContract::ActivityOrigin origin, QString name)
{
    const QString clientId = QStringLiteral("client:c%1").arg(_nextClientNumber++);
    logActivity(name.isEmpty() ? QStringLiteral("Client '%1' connected.").arg(clientId)
                               : QStringLiteral("Client '%1' (%2) connected.").arg(clientId, name),
                 origin, clientId);
    return clientId;
}

/******************************************************************************
* Records a semantic activity of the session.
******************************************************************************/
quint64 AutomationSession::logActivity(QString summary, AutomationContract::ActivityOrigin origin, QString clientId,
                                       QVariantMap details)
{
    return auditTrail().record(AutomationContract::EventKind::Activity, std::move(summary), origin, std::move(clientId),
                               {}, {}, {}, std::move(details));
}

/******************************************************************************
* Opens a transaction boundary.
******************************************************************************/
AutomationTransaction* AutomationSession::beginTransaction(QString label, AutomationContract::ActivityOrigin origin,
                                                          QString clientId)
{
    if(openTransaction())
        return nullptr;
    auto transaction = std::make_unique<AutomationTransaction>();
    transaction->begin(_transactions, auditTrail(), _userInterface, std::move(clientId), origin, std::move(label));
    _openTransaction = std::move(transaction);
    return _openTransaction.get();
}

/******************************************************************************
* Closes the open transaction boundary.
******************************************************************************/
void AutomationSession::commitTransaction()
{
    if(_openTransaction && _openTransaction->isOpen())
        _openTransaction->commit();
}

void AutomationSession::abortTransaction()
{
    if(_openTransaction && _openTransaction->isOpen())
        _openTransaction->abort();
}

/******************************************************************************
* Attaches the session to a data set container.
******************************************************************************/
void AutomationSession::attachToContainer(DataSetContainer& container)
{
    setDataSet(OORef<DataSet>(container.currentSet()));

    // The data set is the one case that invalidates identities.
    connect(&container, &DataSetContainer::dataSetChanged, this, [this](DataSet* dataSet) { setDataSet(OORef<DataSet>(dataSet)); });

    // Everything else only advances the revision. A lambda that takes no arguments is connected to these signals even
    // though they carry the changed object: the revision says "something you looked at changed", and a client that
    // wants to know what it was re-queries.
    connect(&container, &DataSetContainer::viewportLayoutChanged, this, [this]() { sessionChanged(QStringLiteral("the viewport layout changed")); });
    connect(&container, &DataSetContainer::activeViewportChanged, this, [this]() { sessionChanged(QStringLiteral("the active viewport changed")); });
    connect(&container, &DataSetContainer::maximizedViewportChanged, this, [this]() { sessionChanged(QStringLiteral("a viewport was maximized")); });
    connect(&container, &DataSetContainer::selectionChangeComplete, this, [this]() { sessionChanged(QStringLiteral("the selection changed")); });
    connect(&container, &DataSetContainer::currentFrameChanged, this, [this]() { sessionChanged(QStringLiteral("the current frame changed")); });
    connect(&container, &DataSetContainer::animationIntervalChanged, this, [this]() { sessionChanged(QStringLiteral("the animation interval changed")); });
}

}   // End of namespace
