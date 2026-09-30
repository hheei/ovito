// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <ovito/core/automation/AutomationGateway.h>
#include <ovito/core/automation/AutomationSession.h>
#include <ovito/core/automation/AutomationTask.h>
#include <ovito/core/automation/AutomationTransaction.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/utilities/Exception.h>
#include <ovito/core/utilities/concurrent/Task.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>

#include <limits>

namespace Ovito {

namespace {

/// The wire name of a viewport's view type. A client switches on this, not on the number of the enum.
QString viewTypeName(Viewport::ViewType type)
{
    switch(type) {
        case Viewport::VIEW_TOP: return QStringLiteral("top");
        case Viewport::VIEW_BOTTOM: return QStringLiteral("bottom");
        case Viewport::VIEW_FRONT: return QStringLiteral("front");
        case Viewport::VIEW_BACK: return QStringLiteral("back");
        case Viewport::VIEW_LEFT: return QStringLiteral("left");
        case Viewport::VIEW_RIGHT: return QStringLiteral("right");
        case Viewport::VIEW_ORTHO: return QStringLiteral("ortho");
        case Viewport::VIEW_PERSPECTIVE: return QStringLiteral("perspective");
        case Viewport::VIEW_SCENENODE: return QStringLiteral("scene-node");
        case Viewport::VIEW_NONE: break;
    }
    return QStringLiteral("none");
}

/**
 * \brief The scene a client looks at: the one the active viewport shows, and otherwise the first viewport's.
 *
 * A data set stores viewports, and a viewport stores the scene it displays. There is no single scene accessor, because a
 * user can have several scenes open at once in different viewports; the active viewport is the one the user works in and
 * therefore what an automation client means by "the scene".
 */
Scene* activeScene(DataSet* dataSet)
{
    const ViewportConfiguration* config = dataSet ? dataSet->viewportConfig() : nullptr;
    if(!config)
        return nullptr;
    if(Viewport* active = config->activeViewport()) {
        if(Scene* scene = active->scene())
            return scene;
    }
    for(Viewport* viewport : config->viewports()) {
        if(Scene* scene = viewport->scene())
            return scene;
    }
    return nullptr;
}

}   // End of anonymous namespace

/******************************************************************************
* The capabilities a client has after connecting.
******************************************************************************/
QVector<AutomationContract::Capability> AutomationPermissionSet::readOnlyDefaults()
{
    return {
        AutomationContract::Capability::SessionRead,
        AutomationContract::Capability::SceneRead,
        AutomationContract::Capability::SelectionRead,
        AutomationContract::Capability::FileRead
    };
}

AutomationPermissionSet::AutomationPermissionSet(bool grantReadOnlyDefaults)
{
    if(grantReadOnlyDefaults)
        this->grant(readOnlyDefaults());
}

void AutomationPermissionSet::grant(const QVector<AutomationContract::Capability>& capabilities)
{
    for(AutomationContract::Capability capability : capabilities)
        _capabilities.insert(static_cast<int>(capability));
}

QStringList AutomationPermissionSet::names() const
{
    QStringList names;
    for(int value : _capabilities)
        names.push_back(AutomationContract::capabilityName(static_cast<AutomationContract::Capability>(value)));
    names.sort();
    return names;
}

/******************************************************************************
* Constructs a gateway for a client the session attributes to the user.
******************************************************************************/
AutomationGateway::AutomationGateway(AutomationSession& session, QObject* parent)
    : AutomationGateway(session, AutomationContract::ActivityOrigin::User, {}, parent)
{
}

/******************************************************************************
* Constructs a gateway for one client of a session.
******************************************************************************/
AutomationGateway::AutomationGateway(AutomationSession& session, AutomationContract::ActivityOrigin origin,
                                     QString clientName, QObject* parent)
    : QObject(parent), _session(session), _origin(origin)
{
    // The session allocates the client's ID and records that it connected: a client is part of the session's history,
    // including the one that is only reading.
    _clientId = _session.registerClient(origin, std::move(clientName));
    registerBuiltinOperations();
}

AutomationGateway::~AutomationGateway() = default;

/******************************************************************************
* Grants a capability, recording the grant as an activity.
******************************************************************************/
void AutomationGateway::grantCapability(AutomationContract::Capability capability)
{
    if(_permissions.contains(capability))
        return;
    _permissions.grant(capability);
    logActivity(QStringLiteral("Capability '%1' granted to client '%2'.")
                    .arg(AutomationContract::capabilityName(capability), _clientId),
                {}, {}, {}, { { QStringLiteral("capability"), AutomationContract::capabilityName(capability) },
                              { QStringLiteral("grantedCapabilities"), _permissions.names() } });
}

void AutomationGateway::grantCapabilities(const QVector<AutomationContract::Capability>& capabilities)
{
    for(AutomationContract::Capability capability : capabilities)
        grantCapability(capability);
}

void AutomationGateway::grantReadOnlyDefaults()
{
    // The read capabilities are what a client has from the start, so granting them again is not a change and is not
    // recorded; the call exists for a client that was created with nothing at all (see AutomationPermissionSet).
    const QVector<AutomationContract::Capability> capabilities = AutomationPermissionSet::readOnlyDefaults();
    bool changed = false;
    for(AutomationContract::Capability capability : capabilities)
        changed = changed || !_permissions.contains(capability);
    _permissions.grant(capabilities);
    if(changed) {
        logActivity(QStringLiteral("The read capabilities were granted to client '%1'.").arg(_clientId), {}, {}, {},
                    { { QStringLiteral("grantedCapabilities"), _permissions.names() } });
    }
}

void AutomationGateway::revokeCapability(AutomationContract::Capability capability)
{
    if(!_permissions.contains(capability))
        return;
    _permissions.revoke(capability);
    logActivity(QStringLiteral("Capability '%1' was revoked from client '%2'.")
                    .arg(AutomationContract::capabilityName(capability), _clientId),
                {}, {}, {}, { { QStringLiteral("capability"), AutomationContract::capabilityName(capability) },
                              { QStringLiteral("grantedCapabilities"), _permissions.names() } });
}

void AutomationGateway::clearCapabilities()
{
    if(_permissions.names().isEmpty())
        return;
    _permissions.clear();
    logActivity(QStringLiteral("Every capability of client '%1' was revoked.").arg(_clientId));
}

/******************************************************************************
* Records one activity of this client.
******************************************************************************/
void AutomationGateway::logActivity(QString summary, QString operationId, QString taskId, QString transactionId,
                                    QVariantMap details)
{
    _session.auditTrail().record(AutomationContract::EventKind::Activity, std::move(summary), _origin, _clientId,
                                 std::move(taskId), std::move(transactionId), std::move(operationId),
                                 std::move(details));
}

/******************************************************************************
* Adds an operation to the catalog.
******************************************************************************/
void AutomationGateway::registerOperation(AutomationOperationDescriptor descriptor, OperationHandler handler)
{
    const QString id = descriptor.id();
    OVITO_ASSERT_MSG(!id.isEmpty(), "AutomationGateway::registerOperation()", "An operation needs an ID.");
    OVITO_ASSERT_MSG(!_operations.contains(id), "AutomationGateway::registerOperation()", "An operation ID must be unique.");
    if(_operations.contains(id)) {
        // Release builds must not silently shadow an operation either: the catalog is what clients discover, and a
        // duplicate would make one of the two unreachable.
        qWarning("The automation operation '%s' is already registered and was ignored.", qPrintable(id));
        return;
    }
    _operations.insert(id, Entry{ std::move(descriptor), std::move(handler) });
}

/******************************************************************************
* The whole catalog, sorted by operation ID.
******************************************************************************/
QVector<AutomationOperationDescriptor> AutomationGateway::operations() const
{
    QVector<AutomationOperationDescriptor> catalog;
    catalog.reserve(_operations.size());
    for(const Entry& entry : _operations)
        catalog.push_back(entry.descriptor);
    std::sort(catalog.begin(), catalog.end(), [](const auto& a, const auto& b) { return a.id() < b.id(); });
    return catalog;
}

/******************************************************************************
* Looks an operation up by ID.
******************************************************************************/
std::optional<AutomationOperationDescriptor> AutomationGateway::findOperation(const QString& operationId) const
{
    const auto it = _operations.constFind(operationId);
    if(it == _operations.constEnd())
        return std::nullopt;
    return it->descriptor;
}

/******************************************************************************
* The catalog as JSON.
******************************************************************************/
QVariantList AutomationGateway::catalogToJson() const
{
    QVariantList json;
    for(const AutomationOperationDescriptor& descriptor : operations())
        json.push_back(descriptor.toJson());
    return json;
}

/******************************************************************************
* Runs one request through the checks and returns its structured result.
******************************************************************************/
AutomationResult AutomationGateway::dispatch(const AutomationRequest& request)
{
    // Every answer names the contract version and the revision it was computed from, including the failures: a client
    // that gets an error still learns whether its view of the session is current.
    AutomationResult result = AutomationResult::success();
    result.setRevision(_session.revision());

    if(request.operationId().isEmpty()) {
        result.setError(AutomationContract::ErrorCode::InvalidRequest, QStringLiteral("The request does not name an operation."));
        return result;
    }

    const auto it = _operations.constFind(request.operationId());
    if(it == _operations.constEnd()) {
        QStringList known;
        for(const AutomationOperationDescriptor& descriptor : operations())
            known.push_back(descriptor.id());
        result.setError(AutomationContract::ErrorCode::UnknownOperation,
                        QStringLiteral("There is no automation operation called '%1'.").arg(request.operationId()),
                        { { QStringLiteral("operationId"), request.operationId() },
                          { QStringLiteral("knownOperations"), known } });
        return result;
    }

    // An operation this build declares but does not implement is answered before anything client-specific is looked
    // at: what the build does not have cannot be granted by a capability or fixed by a better argument, and a client
    // that is told NotSupported stops asking instead of requesting a permission that would not help.
    if(!it->handler) {
        result.setError(AutomationContract::ErrorCode::NotSupported,
                        QStringLiteral("The automation operation '%1' is declared but not implemented yet.").arg(request.operationId()));
        return result;
    }

    // The arguments, before anything else can fail: a client that calls an existing operation wrongly should hear
    // which parameter is wrong, and not that it lacks a capability.
    const QStringList argumentErrors = it->descriptor.validateArguments(request.arguments());
    if(!argumentErrors.isEmpty()) {
        result.setError(AutomationContract::ErrorCode::InvalidArgument,
                        QStringLiteral("The arguments of '%1' do not match its schema.").arg(request.operationId()),
                        { { QStringLiteral("errors"), argumentErrors } });
        return result;
    }

    QStringList missingCapabilities;
    for(AutomationContract::Capability capability : it->descriptor.requiredCapabilities()) {
        if(!_permissions.contains(capability))
            missingCapabilities.push_back(AutomationContract::capabilityName(capability));
    }
    if(!missingCapabilities.isEmpty()) {
        result.setError(AutomationContract::ErrorCode::MissingCapability,
                        QStringLiteral("This client is not allowed to run '%1'.").arg(request.operationId()),
                        { { QStringLiteral("missing"), missingCapabilities },
                          { QStringLiteral("granted"), _permissions.names() } });
        return result;
    }

    if(!_session.acceptsRevision(request.baseRevision())) {
        result.setError(AutomationContract::ErrorCode::StaleRevision,
                        QStringLiteral("The request's revision %1 is stale; the session is at %2. Re-query the session.")
                            .arg(*request.baseRevision())
                            .arg(_session.revision()),
                        { { QStringLiteral("baseRevision"), QVariant::fromValue<qulonglong>(*request.baseRevision()) },
                          { QStringLiteral("currentRevision"), QVariant::fromValue<qulonglong>(_session.revision()) } });
        return result;
    }

    // The request is acceptable. A command becomes a task - something a client can watch, stop, and read the answer of
    // afterwards - and runs inside a transaction boundary; a query is answered in one step and has neither.
    const bool isCommand = !it->descriptor.isQuery();
    AutomationTaskRecord* task = nullptr;
    AutomationTransaction* transaction = nullptr;
    std::unique_ptr<AutomationTransaction> ownTransaction;

    if(isCommand) {
        // The task records what the request was and what the client was allowed to do; the capabilities are a snapshot,
        // because a later revocation must not rewrite what a finished task may do.
        task = _session.tasks().begin(_session.auditTrail(), _clientId, _origin, request.operationId(), request.requestId(),
                                     request.baseRevision().value_or(_session.revision()),
                                     it->descriptor.requiredCapabilities(), _permissions.names());
        if(AutomationTransaction* open = _session.openTransaction()) {
            // A caller opened a boundary: this command joins it, so several commands become one undo step and the
            // caller decides when - and whether - the whole thing is committed.
            transaction = open;
        }
        else {
            ownTransaction = std::make_unique<AutomationTransaction>();
            ownTransaction->begin(_session.transactions(), _session.auditTrail(), _session.userInterface(), _clientId,
                                  _origin, it->descriptor.undoLabel());
            transaction = ownTransaction.get();
        }
        transaction->joinCommand(request.operationId());
        _session.tasks().attachTransaction(*task, transaction->record()->id());
        result.setTaskId(task->id());
        result.setTransactionId(transaction->record()->id());
    }

    logActivity(QStringLiteral("%1 '%2' was dispatched.")
                    .arg(isCommand ? QStringLiteral("Command") : QStringLiteral("Query"), request.operationId()),
                request.operationId(), task ? task->id() : QString(), transaction ? transaction->record()->id() : QString());

    // From here the operation itself runs. The scope is what this_automation_task::progress() and the cancellation
    // check of the handler read; it is installed even for a query, where it holds no task, so that a handler may call
    // the helpers without asking whether it is a command.
    const int mark = transaction ? transaction->mark() : -1;
    {
        AutomationTaskScope scope(task);
        if(task)
            _session.tasks().start(*task);
        try {
            result.data() = QVariantMap{};
            it->handler(request, result);
        }
        catch(const OperationCanceled&) {
            result.setError(AutomationContract::ErrorCode::Cancelled, QStringLiteral("The operation was cancelled."));
        }
        catch(const Exception& ex) {
            result.setError(AutomationContract::ErrorCode::InternalError,
                            ex.messages().value(0, QStringLiteral("The operation failed.")),
                            { { QStringLiteral("messages"), ex.messages() } });
        }
    }

    if(!task) {
        // A query changes nothing, so the session revision stays where it was.
        return result;
    }

    if(result.isError()) {
        // A failed command leaves no half-applied change behind. An own boundary is rolled back completely; inside a
        // boundary someone else opened, only this command's share is taken back, because the commands before it are
        // theirs to keep.
        if(ownTransaction)
            ownTransaction->abort();
        else
            transaction->revertTo(mark);
        _session.tasks().finish(*task, result);
        return result;
    }

    // A command changes the session; the revision advances so that every other client - and this one - can tell that
    // the snapshot it may have taken before is outdated.
    _session.bumpRevision(request.operationId());
    result.setRevision(_session.revision());
    if(ownTransaction)
        ownTransaction->commit();
    _session.tasks().finish(*task, result);
    return result;
}

/******************************************************************************
* Registers the read-only queries and the task operations this phase provides.
******************************************************************************/
void AutomationGateway::registerBuiltinOperations()
{
    // -----------------------------------------------------------------------
    // session.describe: the first call a client makes.
    // -----------------------------------------------------------------------
    registerOperation(
        AutomationOperationDescriptor(QStringLiteral("session.describe"),
                                     AutomationContract::OperationKind::Query,
                                     QStringLiteral("Describes the session: its revision, its file, its viewports, its scene and its selection."))
            .setUndoLabel(QStringLiteral("Describe the session"))
            .addRequiredCapability(AutomationContract::Capability::SessionRead),
        [this](const AutomationRequest& request, AutomationResult& result) -> void {
            QVariantMap data;
            DataSet* dataSet = _session.dataSet();
            // A client that has just connected asks this first: whether there is anything to look at, which contract it
            // is speaking, and which revision it should base its next request on.
            data.insert(QStringLiteral("hasDataSet"), dataSet != nullptr);
            data.insert(QStringLiteral("contractVersion"), AutomationContract::version());
            data.insert(QStringLiteral("revision"), QVariant::fromValue<qulonglong>(_session.revision()));
            data.insert(QStringLiteral("sessionFilePath"), dataSet ? dataSet->filePath() : QString());
            data.insert(QStringLiteral("objectCount"), QVariant::fromValue<qlonglong>(_session.objects().objectCount()));
            data.insert(QStringLiteral("clientId"), _clientId);
            data.insert(QStringLiteral("origin"), AutomationContract::originName(_origin));
            data.insert(QStringLiteral("capabilities"), _permissions.names());

            QVariantList viewports;
            if(const ViewportConfiguration* config = dataSet ? dataSet->viewportConfig() : nullptr) {
                const Viewport* active = config->activeViewport();
                for(Viewport* viewport : config->viewports()) {
                    QVariantMap entry;
                    entry.insert(QStringLiteral("id"), _session.objects().idFor(viewport, AutomationObjectId::Kind::Viewport));
                    entry.insert(QStringLiteral("viewType"), viewTypeName(viewport->viewType()));
                    entry.insert(QStringLiteral("active"), viewport == active);
                    viewports.push_back(entry);
                }
            }
            data.insert(QStringLiteral("viewports"), viewports);

            Scene* scene = activeScene(dataSet);
            data.insert(QStringLiteral("sceneNodeCount"), scene ? scene->children().size() : 0);
            QVariantList selection;
            if(scene && scene->selection()) {
                for(SceneNode* node : scene->selection()->nodes())
                    selection.push_back(_session.objects().idFor(node, AutomationObjectId::Kind::SceneNode));
            }
            data.insert(QStringLiteral("selectedObjects"), selection);
            // The current frame belongs to the scene, not to the data set: a data set can hold several scenes, and the
            // one a client means by "the session" is the scene its active viewport shows.
            data.insert(QStringLiteral("currentFrame"), scene && scene->animationSettings() ? scene->animationSettings()->currentFrame() : 0);
            data.insert(QStringLiteral("taskCount"), _session.tasks().size());
            data.insert(QStringLiteral("openTransactionId"),
                        _session.openTransaction() ? _session.openTransaction()->record()->id() : QString());
            result.data() = std::move(data);
        });

    // -----------------------------------------------------------------------
    // scene.list_nodes: the scene structure, addressed by stable IDs.
    // -----------------------------------------------------------------------
    registerOperation(
        AutomationOperationDescriptor(QStringLiteral("scene.list_nodes"),
                                     AutomationContract::OperationKind::Query,
                                     QStringLiteral("Lists the scene nodes of the active scene with their stable IDs and their pipelines."))
            .setUndoLabel(QStringLiteral("List the scene nodes"))
            .addRequiredCapability(AutomationContract::Capability::SceneRead),
        [this](const AutomationRequest& request, AutomationResult& result) -> void {
            QVariantList nodes;
            if(Scene* scene = activeScene(_session.dataSet())) {
                for(SceneNode* node : scene->children()) {
                    QVariantMap entry;
                    entry.insert(QStringLiteral("id"), _session.objects().idFor(node, AutomationObjectId::Kind::SceneNode));
                    entry.insert(QStringLiteral("title"), node->objectTitle());
                    if(Pipeline* pipeline = node->pipeline())
                        entry.insert(QStringLiteral("pipelineId"), _session.objects().idFor(pipeline, AutomationObjectId::Kind::Pipeline));
                    nodes.push_back(entry);
                }
            }
            result.data() = QVariantMap{ { QStringLiteral("nodes"), nodes } };
        });

    // -----------------------------------------------------------------------
    // pipeline.describe: one pipeline, its chain of modification nodes, by ID.
    // -----------------------------------------------------------------------
    registerOperation(
        AutomationOperationDescriptor(QStringLiteral("pipeline.describe"),
                                     AutomationContract::OperationKind::Query,
                                     QStringLiteral("Describes one pipeline: its modification nodes, in evaluation order, with their IDs."))
            .setUndoLabel(QStringLiteral("Describe a pipeline"))
            .addRequiredCapability(AutomationContract::Capability::SceneRead)
            .addParameter(AutomationParameter(QStringLiteral("pipelineId"), AutomationParameter::ObjectId)
                              .setDescription(QStringLiteral("The ID of the pipeline, as reported by scene.list_nodes."))),
        [this](const AutomationRequest& request, AutomationResult& result) -> void {
            const QString pipelineId = request.arguments().value(QStringLiteral("pipelineId")).toString();
            const std::optional<AutomationObjectId> id = AutomationObjectId::parse(pipelineId);
            const OORef<OvitoObject> resolved = _session.objects().resolve(pipelineId);
            if(!resolved) {
                // Distinguish the three reasons a client can see, because they call for different reactions: a
                // malformed ID, a wrong kind (the client asked a viewport for its modifiers), and an ID whose object is
                // gone - the last one means "re-query", the others mean "fix your request".
                const AutomationContract::ErrorCode code =
                    _session.objects().wasAssigned(pipelineId) ? AutomationContract::ErrorCode::InvalidatedObject : AutomationContract::ErrorCode::UnknownObject;
                result.setError(code, QStringLiteral("The session has no pipeline '%1'.").arg(pipelineId),
                                { { QStringLiteral("pipelineId"), pipelineId } });
                return;
            }
            const OORef<Pipeline> pipeline = dynamic_object_cast<Pipeline>(resolved);
            if(!pipeline) {
                result.setError(AutomationContract::ErrorCode::InvalidArgument,
                                QStringLiteral("'%1' is not a pipeline.").arg(pipelineId),
                                { { QStringLiteral("pipelineId"), pipelineId } });
                return;
            }

            QVariantList items;
            for(PipelineNode* node = pipeline->head(); node; node = dynamic_object_cast<ModificationNode>(node) ? static_cast<ModificationNode*>(node)->input() : nullptr) {
                // Every item of the chain carries a stable ID, which is how a client addresses the modifier it wants to
                // change later; the source node at the end of the chain has none of its own yet, so it is reported by
                // its title and type.
                QVariantMap item;
                if(ModificationNode* modificationNode = dynamic_object_cast<ModificationNode>(node)) {
                    item.insert(QStringLiteral("id"), _session.objects().idFor(modificationNode, AutomationObjectId::Kind::Modifier));
                    if(Modifier* modifier = modificationNode->modifier())
                        item.insert(QStringLiteral("type"), modifier->getOOMetaClass().name());
                }
                item.insert(QStringLiteral("title"), node->objectTitle());
                item.insert(QStringLiteral("className"), node->getOOMetaClass().name());
                items.push_back(item);
            }
            result.data() = QVariantMap{
                { QStringLiteral("id"), pipelineId },
                { QStringLiteral("items"), items }
            };
        });

    // -----------------------------------------------------------------------
    // task.list: what this session has been asked to do.
    // -----------------------------------------------------------------------
    registerOperation(
        AutomationOperationDescriptor(QStringLiteral("task.list"),
                                     AutomationContract::OperationKind::Query,
                                     QStringLiteral("Lists the tasks of the session, newest first, with their state, progress and origin."))
            .setUndoLabel(QStringLiteral("List the automation tasks"))
            .addRequiredCapability(AutomationContract::Capability::SessionRead)
            .addParameter(AutomationParameter(QStringLiteral("limit"), AutomationParameter::Integer, false)
                              .setDescription(QStringLiteral("The maximum number of tasks to report."))
                              .setRange(1, 1000)),
        [this](const AutomationRequest& request, AutomationResult& result) -> void {
            const int limit = request.arguments().value(QStringLiteral("limit"), 0).toInt();
            QVariantList tasks;
            for(AutomationTaskRecord* task : _session.tasks().tasks()) {
                if(limit > 0 && tasks.size() >= limit)
                    break;
                tasks.push_back(task->toJson(false));
            }
            result.data() = QVariantMap{ { QStringLiteral("tasks"), tasks },
                                        { QStringLiteral("count"), _session.tasks().size() },
                                        { QStringLiteral("lastSequence"), QVariant::fromValue<qulonglong>(_session.events().lastSequence()) } };
        });

    // -----------------------------------------------------------------------
    // task.describe: one task, including the answer it holds.
    // -----------------------------------------------------------------------
    registerOperation(
        AutomationOperationDescriptor(QStringLiteral("task.describe"),
                                     AutomationContract::OperationKind::Query,
                                     QStringLiteral("Describes one task: its lifecycle, who asked for it, and the result it holds."))
            .setUndoLabel(QStringLiteral("Describe a task"))
            .addRequiredCapability(AutomationContract::Capability::SessionRead)
            .addParameter(AutomationParameter(QStringLiteral("taskId"), AutomationParameter::String)
                              .setDescription(QStringLiteral("The ID of the task, as reported by task.list or by a command's result."))),
        [this](const AutomationRequest& request, AutomationResult& result) -> void {
            const QString taskId = request.arguments().value(QStringLiteral("taskId")).toString();
            AutomationTaskRecord* task = _session.tasks().find(taskId);
            if(!task) {
                // A task ID the session issued and has since dropped from its bounded history is a released identity,
                // not a typo; the client is told to look again rather than to fix its request.
                const bool dropped = _session.tasks().wasDropped(taskId);
                result.setError(dropped ? AutomationContract::ErrorCode::InvalidatedObject : AutomationContract::ErrorCode::UnknownObject,
                                dropped ? QStringLiteral("Task '%1' is no longer retained by the session.").arg(taskId)
                                        : QStringLiteral("The session has no task '%1'.").arg(taskId),
                                { { QStringLiteral("taskId"), taskId } });
                return;
            }
            result.data() = task->toJson(true);
        });

    // -----------------------------------------------------------------------
    // task.cancel: stop a task this client started.
    // -----------------------------------------------------------------------
    registerOperation(
        AutomationOperationDescriptor(QStringLiteral("task.cancel"),
                                     AutomationContract::OperationKind::Command,
                                     QStringLiteral("Asks a task of this client to stop; a cooperative operation answers Cancelled."))
            .setUndoLabel(QStringLiteral("Cancel an automation task"))
            .addRequiredCapability(AutomationContract::Capability::TaskControl)
            .addParameter(AutomationParameter(QStringLiteral("taskId"), AutomationParameter::String)
                              .setDescription(QStringLiteral("The ID of the task to cancel."))),
        [this](const AutomationRequest& request, AutomationResult& result) -> void {
            const QString taskId = request.arguments().value(QStringLiteral("taskId")).toString();
            AutomationTaskRecord* task = _session.tasks().find(taskId);
            if(!task) {
                const bool dropped = _session.tasks().wasDropped(taskId);
                result.setError(dropped ? AutomationContract::ErrorCode::InvalidatedObject : AutomationContract::ErrorCode::UnknownObject,
                                dropped ? QStringLiteral("Task '%1' is no longer retained by the session.").arg(taskId)
                                        : QStringLiteral("The session has no task '%1'.").arg(taskId),
                                { { QStringLiteral("taskId"), taskId } });
                return;
            }
            // A client may stop what it started. Anything else would let one client interrupt another one's work, which
            // is a question of authorization that TaskControl alone does not answer.
            if(task->clientId() != _clientId) {
                result.setError(AutomationContract::ErrorCode::InvalidArgument,
                                QStringLiteral("Task '%1' belongs to client '%2', not to this client.")
                                    .arg(taskId, task->clientId()),
                                { { QStringLiteral("taskId"), taskId }, { QStringLiteral("clientId"), task->clientId() } });
                return;
            }
            if(task->isFinished()) {
                // Cancelling something that has already answered is not an error: the client wanted it to stop and it
                // has. It is worth a warning, because the answer the client already has is what counts.
                result.addWarning(QStringLiteral("Task '%1' had already finished; its state is '%2'.")
                                      .arg(taskId, AutomationContract::taskStateName(task->state())));
                result.data() = task->toJson(false);
                return;
            }
            _session.tasks().requestCancel(taskId);
            result.data() = task->toJson(false);
        });

    // -----------------------------------------------------------------------
    // transaction.list: which changes belong together as one undo step.
    // -----------------------------------------------------------------------
    registerOperation(
        AutomationOperationDescriptor(QStringLiteral("transaction.list"),
                                     AutomationContract::OperationKind::Query,
                                     QStringLiteral("Lists the transaction boundaries of the session, newest first."))
            .setUndoLabel(QStringLiteral("List the transactions"))
            .addRequiredCapability(AutomationContract::Capability::SessionRead)
            .addParameter(AutomationParameter(QStringLiteral("limit"), AutomationParameter::Integer, false)
                              .setDescription(QStringLiteral("The maximum number of transactions to report."))
                              .setRange(1, 1000)),
        [this](const AutomationRequest& request, AutomationResult& result) -> void {
            const int limit = request.arguments().value(QStringLiteral("limit"), 0).toInt();
            QVariantList transactions;
            for(AutomationTransactionRecord* transaction : _session.transactions().transactions()) {
                if(limit > 0 && transactions.size() >= limit)
                    break;
                transactions.push_back(transaction->toJson());
            }
            result.data() = QVariantMap{ { QStringLiteral("transactions"), transactions },
                                        { QStringLiteral("count"), _session.transactions().transactions().size() } };
        });

    // -----------------------------------------------------------------------
    // event.list: what happened in this session lately.
    // -----------------------------------------------------------------------
    registerOperation(
        AutomationOperationDescriptor(QStringLiteral("event.list"),
                                     AutomationContract::OperationKind::Query,
                                     QStringLiteral("Lists the recent events of the session: changes, task lifecycles and activities."))
            .setUndoLabel(QStringLiteral("List the recent events"))
            .addRequiredCapability(AutomationContract::Capability::SessionRead)
            .addParameter(AutomationParameter(QStringLiteral("since"), AutomationParameter::Integer, false)
                              .setDescription(QStringLiteral("Only events after this sequence number are returned."))
                              .setRange(0, std::numeric_limits<double>::max()))
            .addParameter(AutomationParameter(QStringLiteral("limit"), AutomationParameter::Integer, false)
                              .setDescription(QStringLiteral("The maximum number of events to return, from the oldest one that matches."))
                              .setRange(1, 1000))
            .addParameter(AutomationParameter(QStringLiteral("kinds"), AutomationParameter::StringList, false)
                              .setDescription(QStringLiteral("Only events of these kinds are returned."))
                              .setAllowedValues({ AutomationContract::eventKindName(AutomationContract::EventKind::SessionChanged),
                                                  AutomationContract::eventKindName(AutomationContract::EventKind::TaskStarted),
                                                  AutomationContract::eventKindName(AutomationContract::EventKind::TaskProgress),
                                                  AutomationContract::eventKindName(AutomationContract::EventKind::TaskFinished),
                                                  AutomationContract::eventKindName(AutomationContract::EventKind::Activity) })),
        [this](const AutomationRequest& request, AutomationResult& result) -> void {
            const quint64 since = request.arguments().value(QStringLiteral("since"), 0).toULongLong();
            const int limit = request.arguments().value(QStringLiteral("limit"), 0).toInt();
            QStringList kinds;
            for(const QVariant& kind : request.arguments().value(QStringLiteral("kinds")).toList())
                kinds.push_back(kind.toString());

            QVariantList events;
            for(const AutomationEvent& event : _session.events().events(since, limit)) {
                if(!kinds.isEmpty() && !kinds.contains(AutomationContract::eventKindName(event.kind())))
                    continue;
                events.push_back(event.toJson());
            }
            result.data() = QVariantMap{ { QStringLiteral("events"), events },
                                        { QStringLiteral("lastSequence"), QVariant::fromValue<qulonglong>(_session.events().lastSequence()) },
                                        { QStringLiteral("firstRetainedSequence"), QVariant::fromValue<qulonglong>(_session.events().firstSequence()) } };
        });
}

}   // End of namespace
