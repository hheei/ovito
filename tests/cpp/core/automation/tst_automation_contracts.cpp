// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

// Contract tests of the machine-facing automation layer (Phase 2.6, deliverables 1-4).
//
// These tests must run without a Python installation, without a GUI and without a window system: they cover the
// vocabulary, the wire types, the stable object IDs, the revision preconditions, the gateway's dispatch rules, the task
// lifecycle, the transaction boundaries and the permission model. They are the executable half of the contract that
// docs/design/AUTOMATION_CONTRACTS.md describes (still to be written, see the audit's open item O13).

#include <QTest>

#include <ovito/core/automation/AutomationEvent.h>
#include <ovito/core/automation/AutomationGateway.h>
#include <ovito/core/automation/AutomationObjectId.h>
#include <ovito/core/automation/AutomationObjectRegistry.h>
#include <ovito/core/automation/AutomationSession.h>
#include <ovito/core/automation/AutomationTask.h>
#include <ovito/core/automation/AutomationTransaction.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/undo/UndoStack.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/SceneNode.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/utilities/concurrent/Task.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>

using namespace Ovito;

namespace {

/// A minimal concrete Application that serves purely as a test environment, the same fixture
/// tst_concurrent_pool uses: it provides the global Application::instance() - which the core consults to decide
/// whether the current thread is the main thread, for instance when a scene node invalidates its world
/// transformation - without pulling in plugin loading, command-line parsing or a real Qt application object. The
/// only pure-virtual member of Application is createQtApplicationImpl(); the test never calls it, because QTest
/// already owns the QCoreApplication, so the override just returns nullptr.
class TestApplication : public Application
{
public:
    using Application::Application;

    /// A user interface owns an undo stack - WorkbenchUI creates its own the same way - and the setter for it is
    /// protected, because a frontend never sets one from the outside. A test is not a frontend, so it gets a named way
    /// in rather than a friend declaration.
    void createUndoStack() { setUndoStack(new UndoStack(*this, this)); }

protected:
    QCoreApplication* createQtApplicationImpl(bool /*supportGui*/, int& /*argc*/, char** /*argv*/) override { return nullptr; }
};

}   // namespace

class AutomationContractTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    /// Creates the application this suite runs under - one per process, as Application allows only one.
    void initTestCase()
    {
        // The Application is an OvitoObject, so it must be created through OORef::create(); plain construction would
        // trip an assertion when the object is later destroyed.
        _application = OORef<TestApplication>::create();
        // Prime the thread-identity cache from the main thread, so isMainThread() classifies it correctly.
        QVERIFY(this_task::isMainThread());
        // A user interface owns an undo stack - WorkbenchUI creates its own the same way. The transaction tests need
        // one to show that a command's changes become one undo step; every other test ignores it, and init() empties it.
        _application->createUndoStack();
    }

    /// Retires the application of initTestCase() once every test has run.
    void cleanupTestCase()
    {
        // Put the TaskManager into the shutting-down state (drains the work queue and joins the pool) before the
        // Application is destroyed - its destructor asserts that this happened.
        _application->taskManager().requestShutdown();
        _application = {};
    }

    /**
     * \brief Establishes the ambient OVITO task every test of this suite runs in.
     *
     * Creating an OVITO object asks the ambient task whether the creation happens interactively
     * (`OORef::create()`), so a test that builds a data set, a scene or a pipeline needs one; without it the call
     * asserts in a debug build and reads a null task in a release build. The task is deliberately not interactive: a
     * contract test is a script, not a user at the user interface.
     */
    void init()
    {
        // The ambient task comes first: creating any OVITO object asks it whether the creation is interactive.
        _task = std::make_shared<Task>();
        _taskScope = std::make_unique<Task::Scope>(_task.get());

        // The task carries the application this suite runs under, because core work that belongs to a user interface
        // resolves `this_task::ui()` - inserting a scene node asks the active interface for the current animation
        // time. There is no window behind it; the application only owns the data set container these tests need.
        _task->setUserInterface(_application);

        // An undo stack is per test function, not per suite: what one test recorded must not show up in the next one.
        _application->undoStack()->clear();
    }

    /// Ends the ambient task of init() - a task must be finished when it goes away.
    void cleanup()
    {
        _taskScope.reset();
        _task->setFinished();
        _task.reset();
    }

    // -----------------------------------------------------------------------
    // The vocabulary of the contract
    // -----------------------------------------------------------------------

    void vocabulary_round_trips()
    {
        QCOMPARE(AutomationContract::version(), QStringLiteral("0.2"));
        QCOMPARE(AutomationContract::kindName(AutomationContract::OperationKind::Query), QStringLiteral("query"));
        QCOMPARE(AutomationContract::kindName(AutomationContract::OperationKind::Command), QStringLiteral("command"));

        // Every capability name has to come back as the same capability.
        for(int i = 0; i <= static_cast<int>(AutomationContract::Capability::TaskControl); ++i) {
            const auto capability = static_cast<AutomationContract::Capability>(i);
            const QString name = AutomationContract::capabilityName(capability);
            QVERIFY(!name.isEmpty());
            const std::optional<AutomationContract::Capability> parsed = AutomationContract::capabilityFromName(name);
            QVERIFY(parsed.has_value());
            QCOMPARE(*parsed, capability);
        }
        QVERIFY(!AutomationContract::capabilityFromName(QStringLiteral("scene.write")).has_value());

        // Error names are unique, so a client can switch on them.
        QSet<QString> errorNames;
        for(int i = 0; i <= static_cast<int>(AutomationContract::ErrorCode::InternalError); ++i) {
            const QString name = AutomationContract::errorCodeName(static_cast<AutomationContract::ErrorCode>(i));
            QVERIFY(!name.isEmpty());
            QVERIFY2(!errorNames.contains(name), qPrintable(name));
            errorNames.insert(name);
        }

        // The task, origin, event and transaction vocabularies round-trip as well: a client switches on these names the
        // same way, and the senders and readers of the wire format must not drift apart.
        for(int i = 0; i <= static_cast<int>(AutomationContract::TaskState::Cancelled); ++i) {
            const auto state = static_cast<AutomationContract::TaskState>(i);
            QVERIFY(!AutomationContract::taskStateName(state).isEmpty());
            QCOMPARE(AutomationContract::isTerminalTaskState(state),
                     i >= static_cast<int>(AutomationContract::TaskState::Completed));
        }
        for(int i = 0; i <= static_cast<int>(AutomationContract::ActivityOrigin::Python); ++i) {
            const auto origin = static_cast<AutomationContract::ActivityOrigin>(i);
            const QString name = AutomationContract::originName(origin);
            QVERIFY(!name.isEmpty());
            const std::optional<AutomationContract::ActivityOrigin> parsed = AutomationContract::originFromName(name);
            QVERIFY(parsed.has_value());
            QCOMPARE(*parsed, origin);
        }
        for(int i = 0; i <= static_cast<int>(AutomationContract::EventKind::Activity); ++i) {
            const auto kind = static_cast<AutomationContract::EventKind>(i);
            const QString name = AutomationContract::eventKindName(kind);
            QVERIFY(!name.isEmpty());
            const std::optional<AutomationContract::EventKind> parsed = AutomationContract::eventKindFromName(name);
            QVERIFY(parsed.has_value());
            QCOMPARE(*parsed, kind);
        }
        QVERIFY(!AutomationContract::originFromName(QStringLiteral("robot")).has_value());
        QVERIFY(!AutomationContract::eventKindFromName(QStringLiteral("task.exploded")).has_value());
        QCOMPARE(AutomationContract::transactionStateName(AutomationContract::TransactionState::Open), QStringLiteral("open"));
        QCOMPARE(AutomationContract::transactionStateName(AutomationContract::TransactionState::Committed), QStringLiteral("committed"));
        QCOMPARE(AutomationContract::transactionStateName(AutomationContract::TransactionState::Aborted), QStringLiteral("aborted"));
    }

    void vocabulary_classifies_read_capabilities()
    {
        using Capability = AutomationContract::Capability;
        QVERIFY(AutomationContract::isReadCapability(Capability::SessionRead));
        QVERIFY(AutomationContract::isReadCapability(Capability::SceneRead));
        QVERIFY(AutomationContract::isReadCapability(Capability::SelectionRead));
        QVERIFY(AutomationContract::isReadCapability(Capability::FileRead));
        QVERIFY(!AutomationContract::isReadCapability(Capability::PipelineWrite));
        QVERIFY(!AutomationContract::isReadCapability(Capability::SelectionWrite));
        QVERIFY(!AutomationContract::isReadCapability(Capability::AnimationWrite));
        QVERIFY(!AutomationContract::isReadCapability(Capability::PythonExecute));
        QVERIFY(!AutomationContract::isReadCapability(Capability::FileWrite));
        QVERIFY(!AutomationContract::isReadCapability(Capability::NetworkAccess));
        QVERIFY(!AutomationContract::isReadCapability(Capability::ProcessExecute));
        QVERIFY(!AutomationContract::isReadCapability(Capability::TaskControl));
    }

    // -----------------------------------------------------------------------
    // Parameter schemas
    // -----------------------------------------------------------------------

    void parameter_validates_types()
    {
        const AutomationParameter integer(QStringLiteral("index"), AutomationParameter::Integer);
        QVERIFY(integer.validate(QVariant(3)).isEmpty());
        QVERIFY(!integer.validate(QVariant(3.5)).isEmpty());          // a double is not an integer
        QVERIFY(!integer.validate(QVariant(QStringLiteral("3"))).isEmpty());
        QVERIFY(!integer.validate(QVariant(true)).isEmpty());
        // The message names the parameter that is wrong, so a client can fix the request it sent.
        QVERIFY(integer.validate(QVariant(3.5)).contains(QStringLiteral("index")));

        const AutomationParameter number(QStringLiteral("distance"), AutomationParameter::Number);
        QVERIFY(number.validate(QVariant(1.5)).isEmpty());
        QVERIFY(number.validate(QVariant(2)).isEmpty());
        QVERIFY(!number.validate(QVariant(QStringLiteral("2"))).isEmpty());

        const AutomationParameter boolean(QStringLiteral("enabled"), AutomationParameter::Boolean);
        QVERIFY(boolean.validate(QVariant(true)).isEmpty());
        QVERIFY(!boolean.validate(QVariant(1)).isEmpty());

        const AutomationParameter text(QStringLiteral("name"), AutomationParameter::String);
        QVERIFY(text.validate(QVariant(QStringLiteral("x"))).isEmpty());
        QVERIFY(!text.validate(QVariant(1)).isEmpty());

        const AutomationParameter list(QStringLiteral("files"), AutomationParameter::StringList);
        QVERIFY(list.validate(QVariant(QStringList{QStringLiteral("a")})).isEmpty());
        QVERIFY(!list.validate(QVariant(QStringLiteral("a"))).isEmpty());

        const AutomationParameter objectId(QStringLiteral("pipelineId"), AutomationParameter::ObjectId);
        QVERIFY(objectId.validate(QVariant(QStringLiteral("pipeline:p1"))).isEmpty());
        QVERIFY(!objectId.validate(QVariant(QStringLiteral("p1"))).isEmpty());
        QVERIFY(!objectId.validate(QVariant(QStringLiteral("pipeline:p0"))).isEmpty());
    }

    void parameter_validates_constraints()
    {
        const AutomationParameter bounded = AutomationParameter(QStringLiteral("radius"), AutomationParameter::Number)
                                                .setRange(0.5, 2.5)
                                                .setUnit(QStringLiteral("nm"));
        QVERIFY(bounded.validate(QVariant(1.0)).isEmpty());
        QVERIFY(bounded.validate(QVariant(0.5)).isEmpty());
        QVERIFY(bounded.validate(QVariant(2.5)).isEmpty());
        QVERIFY(bounded.validate(QVariant(0.4)).contains(QStringLiteral("below the minimum 0.5")));
        QVERIFY(bounded.validate(QVariant(2.6)).contains(QStringLiteral("above the maximum 2.5")));

        const AutomationParameter enumerated = AutomationParameter(QStringLiteral("mode"), AutomationParameter::String)
                                                   .setAllowedValues({QStringLiteral("full"), QStringLiteral("reduced")});
        QVERIFY(enumerated.validate(QVariant(QStringLiteral("full"))).isEmpty());
        QVERIFY(enumerated.validate(QVariant(QStringLiteral("half"))).contains(QStringLiteral("is not one of")));

        // The schema carries what a client needs to build a form.
        const QVariantMap json = bounded.toJson();
        QCOMPARE(json.value(QStringLiteral("name")).toString(), QStringLiteral("radius"));
        QCOMPARE(json.value(QStringLiteral("type")).toString(), QStringLiteral("number"));
        QCOMPARE(json.value(QStringLiteral("required")).toBool(), true);
        QCOMPARE(json.value(QStringLiteral("unit")).toString(), QStringLiteral("nm"));
        QCOMPARE(json.value(QStringLiteral("minimum")).toDouble(), 0.5);
        QCOMPARE(json.value(QStringLiteral("maximum")).toDouble(), 2.5);
    }

    void descriptor_validates_arguments()
    {
        const AutomationOperationDescriptor descriptor = AutomationOperationDescriptor(
                                                             QStringLiteral("example.op"),
                                                             AutomationContract::OperationKind::Command,
                                                             QStringLiteral("An example."))
                                                             .addParameter(AutomationParameter(QStringLiteral("required"), AutomationParameter::Integer))
                                                             .addParameter(AutomationParameter(QStringLiteral("optional"), AutomationParameter::String, false))
                                                             .addRequiredCapability(AutomationContract::Capability::PipelineWrite);

        QVERIFY(descriptor.validateArguments({{QStringLiteral("required"), 1}}).isEmpty());
        QVERIFY(descriptor.validateArguments({{QStringLiteral("required"), 1}, {QStringLiteral("optional"), QStringLiteral("x")}}).isEmpty());

        // A missing required parameter, a misspelled one and a wrong type are three different messages.
        const QStringList missing = descriptor.validateArguments({});
        QCOMPARE(missing.size(), 1);
        QVERIFY(missing.front().contains(QStringLiteral("required")));
        QVERIFY(missing.front().contains(QStringLiteral("missing")));

        const QStringList unknown = descriptor.validateArguments({{QStringLiteral("required"), 1}, {QStringLiteral("requried"), 2}});
        QCOMPARE(unknown.size(), 1);
        QVERIFY(unknown.front().contains(QStringLiteral("no parameter of that name")));

        const QStringList wrongType = descriptor.validateArguments({{QStringLiteral("required"), QStringLiteral("1")}});
        QCOMPARE(wrongType.size(), 1);
        QVERIFY(wrongType.front().contains(QStringLiteral("expected an integer")));

        // The descriptor describes itself for the catalog.
        const QVariantMap json = descriptor.toJson();
        QCOMPARE(json.value(QStringLiteral("id")).toString(), QStringLiteral("example.op"));
        QCOMPARE(json.value(QStringLiteral("kind")).toString(), QStringLiteral("command"));
        QCOMPARE(json.value(QStringLiteral("parameters")).toList().size(), 2);
        QCOMPARE(json.value(QStringLiteral("requiredCapabilities")).toStringList(), QStringList{QStringLiteral("pipeline.write")});
        QVERIFY(!descriptor.isQuery());
    }

    // -----------------------------------------------------------------------
    // Requests and results
    // -----------------------------------------------------------------------

    void request_round_trips()
    {
        const AutomationRequest request = AutomationRequest(QStringLiteral("pipeline.describe"))
                                              .setArguments({{QStringLiteral("pipelineId"), QStringLiteral("pipeline:p3")}})
                                              .setBaseRevision(7)
                                              .setRequestId(QStringLiteral("req-1"));
        const QVariantMap json = request.toJson();
        QCOMPARE(json.value(QStringLiteral("operationId")).toString(), QStringLiteral("pipeline.describe"));
        QCOMPARE(json.value(QStringLiteral("baseRevision")).toULongLong(), quint64(7));
        QCOMPARE(json.value(QStringLiteral("requestId")).toString(), QStringLiteral("req-1"));

        const AutomationRequest restored = AutomationRequest::fromJson(json);
        QCOMPARE(restored.operationId(), request.operationId());
        QCOMPARE(restored.arguments(), request.arguments());
        QVERIFY(restored.baseRevision().has_value());
        QCOMPARE(*restored.baseRevision(), quint64(7));
        QCOMPARE(restored.requestId(), QStringLiteral("req-1"));

        // A request without a base revision is the "I do not care what I overwrite" case, which a query may use.
        QVERIFY(!AutomationRequest(QStringLiteral("session.describe")).baseRevision().has_value());
    }

    void result_round_trips()
    {
        AutomationResult result = AutomationResult::success({{QStringLiteral("nodes"), QVariantList{}}});
        result.setRevision(12);
        result.addWarning(QStringLiteral("the scene is empty"));
        result.setTaskId(QStringLiteral("task-7"));
        result.setTransactionId(QStringLiteral("tx-3"));
        result.addArtifact(AutomationArtifact(QStringLiteral("art-1"), QStringLiteral("image/png"), QStringLiteral("viewport.capture"))
                               .setDimensions(640, 480)
                               .setFrame(3));

        const QVariantMap json = result.toJson();
        QCOMPARE(json.value(QStringLiteral("ok")).toBool(), true);
        QCOMPARE(json.value(QStringLiteral("contractVersion")).toString(), AutomationContract::version());
        QCOMPARE(json.value(QStringLiteral("revision")).toULongLong(), quint64(12));
        QCOMPARE(json.value(QStringLiteral("warnings")).toStringList(), QStringList{QStringLiteral("the scene is empty")});
        QCOMPARE(json.value(QStringLiteral("taskId")).toString(), QStringLiteral("task-7"));
        const QVariantMap artifact = json.value(QStringLiteral("artifacts")).toList().front().toMap();
        QCOMPARE(artifact.value(QStringLiteral("mediaType")).toString(), QStringLiteral("image/png"));
        QCOMPARE(artifact.value(QStringLiteral("width")).toInt(), 640);
        QCOMPARE(artifact.value(QStringLiteral("height")).toInt(), 480);
        QCOMPARE(artifact.value(QStringLiteral("frame")).toInt(), 3);
        QVERIFY(!artifact.contains(QStringLiteral("filePath")));   // an in-memory artifact has no file

        const AutomationResult restored = AutomationResult::fromJson(json);
        QVERIFY(restored.isSuccess());
        QCOMPARE(restored.revision(), quint64(12));
        QCOMPARE(restored.warnings(), result.warnings());
        QCOMPARE(restored.taskId(), QStringLiteral("task-7"));
        QCOMPARE(restored.transactionId(), QStringLiteral("tx-3"));
        QCOMPARE(restored.artifacts().size(), 1);
        QCOMPARE(restored.artifacts().front().width(), 640);

        // A failure carries the code and the details a client acts on.
        const AutomationResult failure = AutomationResult::failure(AutomationContract::ErrorCode::StaleRevision,
                                                                   QStringLiteral("stale"),
                                                                   {{QStringLiteral("currentRevision"), QVariant::fromValue<qulonglong>(9)}});
        QVERIFY(failure.isError());
        const QVariantMap failureJson = failure.toJson();
        QCOMPARE(failureJson.value(QStringLiteral("ok")).toBool(), false);
        const QVariantMap error = failureJson.value(QStringLiteral("error")).toMap();
        QCOMPARE(error.value(QStringLiteral("code")).toString(), QStringLiteral("stale_revision"));
        QCOMPARE(error.value(QStringLiteral("message")).toString(), QStringLiteral("stale"));
        QCOMPARE(error.value(QStringLiteral("details")).toMap().value(QStringLiteral("currentRevision")).toULongLong(), quint64(9));

        const AutomationResult restoredFailure = AutomationResult::fromJson(failureJson);
        QVERIFY(restoredFailure.isError());
        QCOMPARE(restoredFailure.errorCode(), AutomationContract::ErrorCode::StaleRevision);
        QCOMPARE(restoredFailure.errorDetails().value(QStringLiteral("currentRevision")).toULongLong(), quint64(9));
    }

    // -----------------------------------------------------------------------
    // Stable object IDs
    // -----------------------------------------------------------------------

    void object_id_grammar()
    {
        const QString pipelineId = AutomationObjectId::forPipeline(42).toString();
        QCOMPARE(pipelineId, QStringLiteral("pipeline:p42"));
        QCOMPARE(AutomationObjectId::forModifier(108).toString(), QStringLiteral("modifier:m108"));
        QCOMPARE(AutomationObjectId::forViewport(2).toString(), QStringLiteral("viewport:v2"));
        QCOMPARE(AutomationObjectId::forSceneNode(7).toString(), QStringLiteral("scenenode:s7"));
        QCOMPARE(AutomationObjectId::forProperty(AutomationObjectId::Kind::Modifier, 108, QStringLiteral("distance")).toString(),
                 QStringLiteral("property:m108/distance"));

        const std::optional<AutomationObjectId> parsed = AutomationObjectId::parse(pipelineId);
        QVERIFY(parsed.has_value());
        QCOMPARE(parsed->kind(), AutomationObjectId::Kind::Pipeline);
        QCOMPARE(parsed->number(), quint64(42));
        QVERIFY(!parsed->isProperty());

        const std::optional<AutomationObjectId> property = AutomationObjectId::parse(QStringLiteral("property:m108/distance"));
        QVERIFY(property.has_value());
        QVERIFY(property->isProperty());
        QCOMPARE(property->ownerKind(), AutomationObjectId::Kind::Modifier);
        QCOMPARE(property->number(), quint64(108));
        QCOMPARE(property->fieldName(), QStringLiteral("distance"));

        // Malformed IDs are rejected rather than guessed at.
        for(const QString& text : { QString(), QStringLiteral("p42"), QStringLiteral("pipeline:"), QStringLiteral("pipeline:p"),
                                    QStringLiteral("pipeline:p0"), QStringLiteral("pipeline:p01"), QStringLiteral("pipeline:x1"),
                                    QStringLiteral("modifier:p1"), QStringLiteral("property:m108"), QStringLiteral("property:m108/"),
                                    QStringLiteral("property:/distance"), QStringLiteral("property:m108/a/b"),
                                    QStringLiteral("property:pipeline:p1/distance"), QStringLiteral("pipeline:p1/distance") }) {
            QVERIFY2(!AutomationObjectId::parse(text).has_value(), qPrintable(text));
        }

        // A default-constructed ID is invalid and prints as nothing.
        QVERIFY(!AutomationObjectId().isValid());
        QCOMPARE(AutomationObjectId().toString(), QString());
    }

    void registry_keeps_ids_stable()
    {
        AutomationObjectRegistry registry;
        OORef<Pipeline> pipeline = OORef<Pipeline>::create();
        OORef<ModificationNode> node = OORef<ModificationNode>::create();

        const QString pipelineId = registry.idFor(pipeline.get(), AutomationObjectId::Kind::Pipeline);
        QCOMPARE(pipelineId, QStringLiteral("pipeline:p1"));
        // Asking again is the presentation-refresh case: the same object keeps its ID.
        QCOMPARE(registry.idFor(pipeline.get(), AutomationObjectId::Kind::Pipeline), pipelineId);
        QCOMPARE(registry.existingId(pipeline.get()).value(), pipelineId);

        const QString nodeId = registry.idFor(node.get(), AutomationObjectId::Kind::Modifier);
        QCOMPARE(nodeId, QStringLiteral("modifier:m1"));
        QVERIFY(registry.idFor(node.get(), AutomationObjectId::Kind::Modifier) == nodeId);
        QCOMPARE(registry.objectCount(), 2);

        // Resolution goes both ways, and a property ID names its owner.
        QCOMPARE(registry.resolve(pipelineId).get(), pipeline.get());
        QCOMPARE(registry.resolve(nodeId).get(), node.get());
        const QString propertyId = registry.propertyIdFor(node.get(), AutomationObjectId::Kind::Modifier, QStringLiteral("distance"));
        QCOMPARE(propertyId, QStringLiteral("property:m1/distance"));
        QCOMPARE(registry.resolveOwner(*AutomationObjectId::parse(propertyId)).get(), node.get());

        // Unknown and malformed IDs do not resolve, and are not reported as known.
        QVERIFY(!registry.resolve(QStringLiteral("pipeline:p99")).get());
        QVERIFY(!registry.resolve(QStringLiteral("distance")).get());
        QVERIFY(!registry.wasAssigned(QStringLiteral("pipeline:p99")));
        QVERIFY(registry.wasAssigned(propertyId));

        // A deleted object invalidates its ID, and the number is not reused: the next pipeline is p2, and the old ID
        // is still recognized as one this registry handed out.
        registry.invalidate(node.get());
        QVERIFY(!registry.resolve(nodeId).get());
        QVERIFY(registry.wasAssigned(nodeId));
        QCOMPARE(registry.objectCount(), 1);
    }

    void registry_invalidates_all_without_reusing_numbers()
    {
        AutomationObjectRegistry registry;
        OORef<Pipeline> first = OORef<Pipeline>::create();
        const QString firstId = registry.idFor(first.get(), AutomationObjectId::Kind::Pipeline);
        QCOMPARE(firstId, QStringLiteral("pipeline:p1"));

        // This is the data set replacement case: the objects of the old session are gone, and an ID from before the
        // replacement must never be attached to a new object.
        registry.invalidateAll();
        QVERIFY(!registry.resolve(firstId).get());
        QVERIFY(registry.wasAssigned(firstId));
        QCOMPARE(registry.objectCount(), 0);
        // The old object itself is still alive here - the registry simply no longer knows it.
        QVERIFY(!registry.existingId(first.get()).has_value());

        OORef<Pipeline> second = OORef<Pipeline>::create();
        const QString secondId = registry.idFor(second.get(), AutomationObjectId::Kind::Pipeline);
        QCOMPARE(secondId, QStringLiteral("pipeline:p2"));
        QVERIFY(secondId != firstId);
    }

    void registry_survives_a_deleted_object_becoming_a_new_one()
    {
        AutomationObjectRegistry registry;
        QString lastId;
        // The reference outlives an iteration, so the registry sees the previous object die and - when the allocator
        // hands the same address out again - has to notice that the address now belongs to a different object.
        OORef<Pipeline> pipeline;
        for(int i = 0; i < 3; ++i) {
            pipeline = OORef<Pipeline>::create();
            const QString id = registry.idFor(pipeline.get(), AutomationObjectId::Kind::Pipeline);
            // Every iteration gets a fresh number, whether the previous object is still alive or not; the allocator may
            // hand out the same address again, which the registry has to notice.
            QVERIFY(id != lastId);
            lastId = id;
        }
        QCOMPARE(registry.objectCount(), 1);   // only the last one is still alive
    }

    // -----------------------------------------------------------------------
    // The session and its revision
    // -----------------------------------------------------------------------

    void session_revision_tracks_the_data_set()
    {
        AutomationSession session;
        QCOMPARE(session.revision(), quint64(1));
        QVERIFY(!session.dataSet());
        QVERIFY(session.acceptsRevision(std::nullopt));
        QVERIFY(session.acceptsRevision(1));
        QVERIFY(!session.acceptsRevision(2));

        session.bumpRevision();
        QCOMPARE(session.revision(), quint64(2));

        // Replacing the data set advances the revision and forgets the object identities of the old session.
        OORef<Pipeline> pipeline = OORef<Pipeline>::create();
        const QString id = session.objects().idFor(pipeline.get(), AutomationObjectId::Kind::Pipeline);
        session.setDataSet(OORef<DataSet>::create());
        QVERIFY(session.dataSet());
        QVERIFY(session.revision() > 2);
        QVERIFY(!session.objects().resolve(id).get());

        // Setting the same data set again is not a change.
        DataSet* dataSet = session.dataSet();
        const quint64 revision = session.revision();
        session.setDataSet(dataSet);
        QCOMPARE(session.revision(), revision);
    }

    // -----------------------------------------------------------------------
    // The gateway
    // -----------------------------------------------------------------------

    void gateway_catalog_is_read_only()
    {
        AutomationSession session;
        AutomationGateway gateway(session);

        // The catalog is not empty, is sorted, and every built-in operation is a query that only requires read
        // capabilities - which is the promise a read-only client depends on. The one exception is task.cancel: stopping
        // a task is a change, so it is a command that requires task.control.
        const QVector<AutomationOperationDescriptor> operations = gateway.operations();
        QVERIFY(operations.size() >= 6);
        QStringList ids;
        for(const AutomationOperationDescriptor& descriptor : operations) {
            ids.push_back(descriptor.id());
            QVERIFY(!descriptor.requiredCapabilities().isEmpty());
            for(AutomationContract::Capability capability : descriptor.requiredCapabilities())
                QCOMPARE(AutomationContract::isReadCapability(capability), descriptor.isQuery());
            if(descriptor.id() == QStringLiteral("task.cancel")) {
                QCOMPARE(descriptor.kind(), AutomationContract::OperationKind::Command);
                QVERIFY(descriptor.requiredCapabilities().contains(AutomationContract::Capability::TaskControl));
                continue;
            }
            QCOMPARE(descriptor.kind(), AutomationContract::OperationKind::Query);
        }
        QStringList sortedIds = ids;
        sortedIds.sort();
        QCOMPARE(ids, sortedIds);
        QVERIFY(ids.contains(QStringLiteral("session.describe")));
        QVERIFY(ids.contains(QStringLiteral("scene.list_nodes")));
        QVERIFY(ids.contains(QStringLiteral("pipeline.describe")));
        QVERIFY(ids.contains(QStringLiteral("task.list")));
        QVERIFY(ids.contains(QStringLiteral("task.describe")));
        QVERIFY(ids.contains(QStringLiteral("transaction.list")));
        QVERIFY(ids.contains(QStringLiteral("event.list")));

        // A default gateway grants the read-only defaults and nothing else.
        QVERIFY(gateway.permissions().contains(AutomationContract::Capability::SessionRead));
        QVERIFY(gateway.permissions().contains(AutomationContract::Capability::SceneRead));
        QVERIFY(!gateway.permissions().contains(AutomationContract::Capability::PipelineWrite));
        QVERIFY(!gateway.permissions().contains(AutomationContract::Capability::PythonExecute));
        QVERIFY(!gateway.permissions().contains(AutomationContract::Capability::FileWrite));
        QCOMPARE(gateway.permissions().names(), QStringList({QStringLiteral("file.read"), QStringLiteral("scene.read"),
                                                             QStringLiteral("selection.read"), QStringLiteral("session.read")}));
    }

    void gateway_rejects_bad_requests()
    {
        AutomationSession session;
        AutomationGateway gateway(session);

        // No operation ID at all.
        AutomationResult result = gateway.dispatch(AutomationRequest());
        QVERIFY(result.isError());
        QCOMPARE(result.errorCode(), AutomationContract::ErrorCode::InvalidRequest);
        QCOMPARE(result.revision(), session.revision());

        // An operation that does not exist: the result names what does.
        result = gateway.dispatch(AutomationRequest(QStringLiteral("scene.delete_everything")));
        QVERIFY(result.isError());
        QCOMPARE(result.errorCode(), AutomationContract::ErrorCode::UnknownOperation);
        QCOMPARE(result.errorDetails().value(QStringLiteral("operationId")).toString(), QStringLiteral("scene.delete_everything"));
        QVERIFY(result.errorDetails().value(QStringLiteral("knownOperations")).toStringList().contains(QStringLiteral("session.describe")));

        // Arguments that do not match the schema, before anything else happens.
        result = gateway.dispatch(AutomationRequest(QStringLiteral("pipeline.describe")));
        QVERIFY(result.isError());
        QCOMPARE(result.errorCode(), AutomationContract::ErrorCode::InvalidArgument);
        QCOMPARE(result.errorDetails().value(QStringLiteral("errors")).toStringList().size(), 1);
        result = gateway.dispatch(AutomationRequest(QStringLiteral("pipeline.describe")).setArguments({{QStringLiteral("pipelineId"), QStringLiteral("nonsense")}}));
        QVERIFY(result.isError());
        QCOMPARE(result.errorCode(), AutomationContract::ErrorCode::InvalidArgument);

        // An operation that is declared for a later phase answers NotSupported instead of doing something.
        gateway.registerOperation(AutomationOperationDescriptor(QStringLiteral("pipeline.insert_modifier"),
                                                                AutomationContract::OperationKind::Command,
                                                                QStringLiteral("Insert a modifier. Phase 4."))
                                      .addParameter(AutomationParameter(QStringLiteral("modifierId"), AutomationParameter::String))
                                      .addRequiredCapability(AutomationContract::Capability::PipelineWrite));
        result = gateway.dispatch(AutomationRequest(QStringLiteral("pipeline.insert_modifier")).setArguments({{QStringLiteral("modifierId"), QStringLiteral("cna")}}));
        QVERIFY(result.isError());
        QCOMPARE(result.errorCode(), AutomationContract::ErrorCode::NotSupported);
        QVERIFY(gateway.findOperation(QStringLiteral("pipeline.insert_modifier")).has_value());
    }

    void gateway_checks_capabilities_and_revisions()
    {
        AutomationSession session;
        AutomationGateway gateway(session);

        // The same call, once without and once with the capability.
        gateway.registerOperation(
            AutomationOperationDescriptor(QStringLiteral("test.recolor"), AutomationContract::OperationKind::Command, QStringLiteral("Test command."))
                .addRequiredCapability(AutomationContract::Capability::PipelineWrite),
            [](const AutomationRequest&, AutomationResult& result) { result.data().insert(QStringLiteral("done"), true); });

        const quint64 revisionBefore = session.revision();
        AutomationResult result = gateway.dispatch(AutomationRequest(QStringLiteral("test.recolor")));
        QVERIFY(result.isError());
        QCOMPARE(result.errorCode(), AutomationContract::ErrorCode::MissingCapability);
        QCOMPARE(result.errorDetails().value(QStringLiteral("missing")).toStringList(), QStringList{QStringLiteral("pipeline.write")});
        QVERIFY(result.errorDetails().value(QStringLiteral("granted")).toStringList().contains(QStringLiteral("scene.read")));
        // A rejected request must not move the session on.
        QCOMPARE(session.revision(), revisionBefore);

        gateway.grantCapability(AutomationContract::Capability::PipelineWrite);
        result = gateway.dispatch(AutomationRequest(QStringLiteral("test.recolor")));
        QVERIFY(result.isSuccess());
        QCOMPARE(result.data().value(QStringLiteral("done")).toBool(), true);
        // A command that succeeded advances the revision and reports the new one.
        QCOMPARE(session.revision(), revisionBefore + 1);
        QCOMPARE(result.revision(), session.revision());

        // A query does not advance it.
        result = gateway.dispatch(AutomationRequest(QStringLiteral("session.describe")));
        QVERIFY(result.isSuccess());
        QCOMPARE(session.revision(), revisionBefore + 1);

        // A base revision that is no longer current is rejected with both revisions in the details.
        result = gateway.dispatch(AutomationRequest(QStringLiteral("session.describe")).setBaseRevision(revisionBefore));
        QVERIFY(result.isError());
        QCOMPARE(result.errorCode(), AutomationContract::ErrorCode::StaleRevision);
        QCOMPARE(result.errorDetails().value(QStringLiteral("baseRevision")).toULongLong(), revisionBefore);
        QCOMPARE(result.errorDetails().value(QStringLiteral("currentRevision")).toULongLong(), session.revision());
        QVERIFY(result.errorMessage().contains(QStringLiteral("Re-query")));

        // The current revision is accepted, and a query without one is always accepted.
        QVERIFY(gateway.dispatch(AutomationRequest(QStringLiteral("session.describe")).setBaseRevision(session.revision())).isSuccess());
        QVERIFY(gateway.dispatch(AutomationRequest(QStringLiteral("session.describe"))).isSuccess());

        // Revoking is possible again, and a cleared set grants nothing; both are recorded as activity of the session.
        gateway.revokeCapability(AutomationContract::Capability::PipelineWrite);
        QVERIFY(gateway.dispatch(AutomationRequest(QStringLiteral("test.recolor"))).isError());
        gateway.clearCapabilities();
        QVERIFY(gateway.dispatch(AutomationRequest(QStringLiteral("session.describe"))).isError());
        gateway.grantReadOnlyDefaults();
        QVERIFY(gateway.dispatch(AutomationRequest(QStringLiteral("session.describe"))).isSuccess());
    }

    void gateway_answers_read_only_operations()
    {
        AutomationSession session;
        AutomationGateway gateway(session);

        // An empty session answers with an empty but well-formed snapshot, not with an error: a client that connects
        // before anything is loaded still gets to see the revision and the contract version.
        AutomationResult result = gateway.dispatch(AutomationRequest(QStringLiteral("session.describe")));
        QVERIFY(result.isSuccess());
        QCOMPARE(result.data().value(QStringLiteral("hasDataSet")).toBool(), false);
        QCOMPARE(result.data().value(QStringLiteral("contractVersion")).toString(), AutomationContract::version());
        QCOMPARE(result.data().value(QStringLiteral("revision")).toULongLong(), session.revision());
        QCOMPARE(result.data().value(QStringLiteral("viewports")).toList().size(), 0);
        QCOMPARE(result.data().value(QStringLiteral("selectedObjects")).toList().size(), 0);

        result = gateway.dispatch(AutomationRequest(QStringLiteral("scene.list_nodes")));
        QVERIFY(result.isSuccess());
        QCOMPARE(result.data().value(QStringLiteral("nodes")).toList().size(), 0);

        // An object ID that was never handed out is an unknown object, and one whose identity was released is an
        // invalidated object; the two are different codes because clients act on them differently.
        result = gateway.dispatch(AutomationRequest(QStringLiteral("pipeline.describe")).setArguments({{QStringLiteral("pipelineId"), QStringLiteral("pipeline:p1")}}));
        QVERIFY(result.isError());
        QCOMPARE(result.errorCode(), AutomationContract::ErrorCode::UnknownObject);

        OORef<Pipeline> pipeline = OORef<Pipeline>::create();
        const QString id = session.objects().idFor(pipeline.get(), AutomationObjectId::Kind::Pipeline);
        result = gateway.dispatch(AutomationRequest(QStringLiteral("pipeline.describe")).setArguments({{QStringLiteral("pipelineId"), id}}));
        QVERIFY(result.isSuccess());
        QCOMPARE(result.data().value(QStringLiteral("id")).toString(), id);
        // A pipeline that was created empty has no modification node yet: the operation describes that as an empty
        // chain rather than failing, which is what a client that has just connected the two calls needs.
        QCOMPARE(result.data().value(QStringLiteral("items")).toList().size(), 0);

        session.setDataSet(OORef<DataSet>::create());
        result = gateway.dispatch(AutomationRequest(QStringLiteral("pipeline.describe")).setArguments({{QStringLiteral("pipelineId"), id}}));
        QVERIFY(result.isError());
        QCOMPARE(result.errorCode(), AutomationContract::ErrorCode::InvalidatedObject);

        // An ID that names a different kind of object than the operation expects.
        OORef<ModificationNode> node = OORef<ModificationNode>::create();
        const QString nodeId = session.objects().idFor(node.get(), AutomationObjectId::Kind::Modifier);
        result = gateway.dispatch(AutomationRequest(QStringLiteral("pipeline.describe")).setArguments({{QStringLiteral("pipelineId"), nodeId}}));
        QVERIFY(result.isError());
        QCOMPARE(result.errorCode(), AutomationContract::ErrorCode::InvalidArgument);
        QVERIFY(result.errorMessage().contains(QStringLiteral("not a pipeline")));
    }

    void gateway_describes_a_real_data_set()
    {
        // A data set created from scratch builds the default viewport configuration, which is what a client sees when
        // it attaches to a fresh session. The IDs it reports stay stable across a second call.
        AutomationSession session;
        AutomationGateway gateway(session);
        session.setDataSet(OORef<DataSet>::create());

        AutomationResult result = gateway.dispatch(AutomationRequest(QStringLiteral("session.describe")));
        QVERIFY(result.isSuccess());
        QCOMPARE(result.data().value(QStringLiteral("hasDataSet")).toBool(), true);
        const QVariantList viewports = result.data().value(QStringLiteral("viewports")).toList();
        QVERIFY(viewports.size() >= 1);

        QStringList firstIds;
        for(const QVariant& viewport : viewports)
            firstIds.push_back(viewport.toMap().value(QStringLiteral("id")).toString());
        QVERIFY(firstIds.front().startsWith(QStringLiteral("viewport:v")));
        // The view type is a contract name, not a translated label.
        const QString viewType = viewports.front().toMap().value(QStringLiteral("viewType")).toString();
        QVERIFY(QStringList({QStringLiteral("top"), QStringLiteral("bottom"), QStringLiteral("front"), QStringLiteral("back"),
                             QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("orthographic"),
                             QStringLiteral("perspective"), QStringLiteral("scene-node"), QStringLiteral("none")}).contains(viewType));

        result = gateway.dispatch(AutomationRequest(QStringLiteral("session.describe")));
        QVERIFY(result.isSuccess());
        QStringList secondIds;
        for(const QVariant& viewport : result.data().value(QStringLiteral("viewports")).toList())
            secondIds.push_back(viewport.toMap().value(QStringLiteral("id")).toString());
        QCOMPARE(firstIds, secondIds);
        QCOMPARE(session.objects().resolve(firstIds.front()).get(), session.dataSet()->viewportConfig()->viewports().front().get());

        // The scene of a fresh data set is empty, and listing it is still a successful query.
        result = gateway.dispatch(AutomationRequest(QStringLiteral("scene.list_nodes")));
        QVERIFY(result.isSuccess());
        QCOMPARE(result.data().value(QStringLiteral("nodes")).toList().size(), 0);

        // Put a node with a pipeline into the scene and describe it: this is the shape a client walks - node ID, then
        // the pipeline behind it, then the modifiers of that pipeline.
        Scene* scene = session.dataSet()->viewportConfig()->viewports().front()->scene();
        QVERIFY(scene);
        OORef<SceneNode> node = OORef<SceneNode>::create();
        OORef<Pipeline> pipeline = OORef<Pipeline>::create();
        node->setPipeline(pipeline);
        scene->addChildNode(node);

        result = gateway.dispatch(AutomationRequest(QStringLiteral("scene.list_nodes")));
        QVERIFY(result.isSuccess());
        const QVariantList nodes = result.data().value(QStringLiteral("nodes")).toList();
        QCOMPARE(nodes.size(), 1);
        const QVariantMap nodeInfo = nodes.front().toMap();
        QVERIFY(nodeInfo.value(QStringLiteral("id")).toString().startsWith(QStringLiteral("scenenode:s")));
        const QString pipelineId = nodeInfo.value(QStringLiteral("pipelineId")).toString();
        QVERIFY(pipelineId.startsWith(QStringLiteral("pipeline:p")));
        QCOMPARE(session.objects().resolve(pipelineId).get(), pipeline.get());

        result = gateway.dispatch(AutomationRequest(QStringLiteral("pipeline.describe")).setArguments({{QStringLiteral("pipelineId"), pipelineId}}));
        QVERIFY(result.isSuccess());
        QCOMPARE(result.data().value(QStringLiteral("id")).toString(), pipelineId);

        // The empty pipeline of a fresh node describes as a chain without items rather than failing.
        QVERIFY(result.data().contains(QStringLiteral("items")));
    }

    // -----------------------------------------------------------------------
    // The task lifecycle
    // -----------------------------------------------------------------------

    void task_lifecycle_reports_progress_and_cancellation()
    {
        AutomationSession session;
        AutomationGateway gateway(session);

        gateway.registerOperation(
            AutomationOperationDescriptor(QStringLiteral("test.long_running"), AutomationContract::OperationKind::Command,
                                          QStringLiteral("A test command that reports progress and stops cooperatively."))
                .setUndoLabel(QStringLiteral("Run the long test command"))
                .addRequiredCapability(AutomationContract::Capability::PipelineWrite),
            [&session](const AutomationRequest&, AutomationResult& result) -> void {
                AutomationTaskRecord* task = this_automation_task::get();
                QVERIFY(task);
                QVERIFY(!this_automation_task::isCancellationRequested());
                // Progress is reported by fraction and text; the task is the only channel an operation needs.
                this_automation_task::progress(0.25);
                this_automation_task::progress(0.5, QStringLiteral("halfway"));
                this_automation_task::recordActivity(QStringLiteral("The test operation reached its halfway point."));
                // A client asks for the task to stop. Here the operation asks on its own behalf, because a dispatch
                // cannot be interrupted from outside while it runs on the main thread.
                QVERIFY(session.tasks().requestCancel(task->id()));
                QVERIFY(this_automation_task::isCancellationRequested());
                this_automation_task::throwIfCancellationRequested();
                result.data().insert(QStringLiteral("reached"), true);   // the cancellation came first
            });
        gateway.grantCapability(AutomationContract::Capability::PipelineWrite);
        const quint64 revisionBefore = session.revision();

        const AutomationResult result = gateway.dispatch(AutomationRequest(QStringLiteral("test.long_running")));
        QVERIFY(result.isError());
        QCOMPARE(result.errorCode(), AutomationContract::ErrorCode::Cancelled);
        QVERIFY(result.taskId().startsWith(QStringLiteral("task:t")));
        // A command that did not succeed is not a change: the revision stays where it was.
        QCOMPARE(session.revision(), revisionBefore);

        AutomationTaskRecord* task = session.tasks().find(result.taskId());
        QVERIFY(task);
        QCOMPARE(task->state(), AutomationContract::TaskState::Cancelled);
        QVERIFY(task->isFinished());
        QVERIFY(task->finishedAt().isValid());
        QCOMPARE(task->progress().value_or(-1.0), 0.5);
        QCOMPARE(task->progressText(), QStringLiteral("halfway"));
        QVERIFY(task->isCancellationRequested());
        QCOMPARE(task->operationId(), QStringLiteral("test.long_running"));
        QCOMPARE(task->clientId(), gateway.clientId());
        QCOMPARE(task->requiredCapabilities(), QVector<AutomationContract::Capability>{ AutomationContract::Capability::PipelineWrite });
        QVERIFY(task->grantedCapabilities().contains(QStringLiteral("pipeline.write")));
        QCOMPARE(task->result().errorCode(), AutomationContract::ErrorCode::Cancelled);
        // The boundary of the failed command was rolled back rather than committed.
        QCOMPARE(session.transactions().find(task->transactionId())->state(), AutomationContract::TransactionState::Aborted);

        // A client reads the task it started through the catalog, and the describe form carries the answer.
        AutomationResult listed = gateway.dispatch(AutomationRequest(QStringLiteral("task.list")));
        QVERIFY(listed.isSuccess());
        QCOMPARE(listed.data().value(QStringLiteral("tasks")).toList().size(), 1);
        QCOMPARE(listed.data().value(QStringLiteral("count")).toInt(), 1);

        AutomationResult described = gateway.dispatch(AutomationRequest(QStringLiteral("task.describe"))
                                                          .setArguments({ { QStringLiteral("taskId"), result.taskId() } }));
        QVERIFY(described.isSuccess());
        QCOMPARE(described.data().value(QStringLiteral("state")).toString(), QStringLiteral("cancelled"));
        QCOMPARE(described.data().value(QStringLiteral("operationId")).toString(), QStringLiteral("test.long_running"));
        QCOMPARE(described.data().value(QStringLiteral("clientId")).toString(), gateway.clientId());
        QCOMPARE(described.data().value(QStringLiteral("origin")).toString(), QStringLiteral("user"));
        QVERIFY(described.data().contains(QStringLiteral("result")));
        // The result of a finished task is the wire form the client would have received: a failure carries its code
        // under `error`.
        QCOMPARE(described.data()
                     .value(QStringLiteral("result"))
                     .toMap()
                     .value(QStringLiteral("error"))
                     .toMap()
                     .value(QStringLiteral("code"))
                     .toString(),
                 QStringLiteral("cancelled"));

        // A task ID that was never handed out is an unknown object, and the answer names it.
        const AutomationResult unknown = gateway.dispatch(AutomationRequest(QStringLiteral("task.describe"))
                                                              .setArguments({ { QStringLiteral("taskId"), QStringLiteral("task:t900") } }));
        QVERIFY(unknown.isError());
        QCOMPARE(unknown.errorCode(), AutomationContract::ErrorCode::UnknownObject);
        QCOMPARE(unknown.errorDetails().value(QStringLiteral("taskId")).toString(), QStringLiteral("task:t900"));

        // The event log tells the same story, in the order the lifecycle happened.
        QVector<AutomationContract::EventKind> kinds;
        for(const AutomationEvent& event : session.events().events())
            kinds.push_back(event.kind());
        const int started = kinds.indexOf(AutomationContract::EventKind::TaskStarted);
        const int progressed = kinds.indexOf(AutomationContract::EventKind::TaskProgress);
        const int finished = kinds.indexOf(AutomationContract::EventKind::TaskFinished);
        QVERIFY(started >= 0);
        QVERIFY(progressed > started);
        QVERIFY(finished > progressed);
        QVERIFY(kinds.contains(AutomationContract::EventKind::Activity));
        // Each progress event names the task it belongs to and how far the operation said it was; the first report
        // carried no text, the second one did.
        QStringList progressTexts;
        double firstProgress = -1.0;
        for(const AutomationEvent& event : session.events().events()) {
            if(event.kind() != AutomationContract::EventKind::TaskProgress)
                continue;
            QCOMPARE(event.taskId(), result.taskId());
            progressTexts.push_back(event.summary());
            if(firstProgress < 0.0)
                firstProgress = event.details().value(QStringLiteral("progress")).toDouble();
        }
        QCOMPARE(firstProgress, 0.25);
        QCOMPARE(progressTexts, QStringList({ QString(), QStringLiteral("halfway") }));
    }

    void task_cancel_requires_control_and_ownership()
    {
        AutomationSession session;
        AutomationGateway gateway(session);
        AutomationGateway other(session, AutomationContract::ActivityOrigin::Ai, QStringLiteral("a test agent"));
        QVERIFY(gateway.clientId() != other.clientId());
        QCOMPARE(other.origin(), AutomationContract::ActivityOrigin::Ai);

        // A task of the other client. It is created directly in the registry, because cancelling a running task needs
        // a second actor and a dispatch cannot run while another one runs on the main thread.
        AutomationTaskRecord* foreign = session.tasks().begin(session.auditTrail(), other.clientId(),
                                                              AutomationContract::ActivityOrigin::Ai,
                                                              QStringLiteral("test.foreign"), {}, session.revision(), {}, {});
        session.tasks().start(*foreign);

        // Without task.control a client cannot cancel anything, not even its own task.
        QVariantMap arguments{ { QStringLiteral("taskId"), foreign->id() } };
        AutomationResult result = gateway.dispatch(AutomationRequest(QStringLiteral("task.cancel")).setArguments(arguments));
        QVERIFY(result.isError());
        QCOMPARE(result.errorCode(), AutomationContract::ErrorCode::MissingCapability);
        QVERIFY(result.errorDetails().value(QStringLiteral("missing")).toStringList().contains(QStringLiteral("task.control")));
        QVERIFY(!foreign->isCancellationRequested());

        // With the capability it still cannot cancel a task of another client.
        gateway.grantCapability(AutomationContract::Capability::TaskControl);
        result = gateway.dispatch(AutomationRequest(QStringLiteral("task.cancel")).setArguments(arguments));
        QVERIFY(result.isError());
        QCOMPARE(result.errorCode(), AutomationContract::ErrorCode::InvalidArgument);
        QVERIFY(result.errorMessage().contains(other.clientId()));
        QVERIFY(!foreign->isCancellationRequested());

        // Its own task it can, and the answer says where the task stands now.
        AutomationTaskRecord* own = session.tasks().begin(session.auditTrail(), gateway.clientId(),
                                                          AutomationContract::ActivityOrigin::User,
                                                          QStringLiteral("test.own"), {}, session.revision(), {}, {});
        session.tasks().start(*own);
        result = gateway.dispatch(AutomationRequest(QStringLiteral("task.cancel"))
                                      .setArguments({ { QStringLiteral("taskId"), own->id() } }));
        QVERIFY(result.isSuccess());
        QVERIFY(own->isCancellationRequested());
        QCOMPARE(result.data().value(QStringLiteral("state")).toString(), QStringLiteral("running"));
        // Cancelling is itself a command, so it runs as a task and advances the revision.
        QVERIFY(result.taskId().startsWith(QStringLiteral("task:t")));
        QVERIFY(!result.transactionId().isEmpty());

        // A task that has already answered cannot be cancelled again: the client wanted it to stop and it has. That is
        // a success with a warning, not an error, and the task keeps the state its answer gave it.
        const QString finishedId = own->id();
        session.tasks().finish(*own, AutomationResult::success());
        QCOMPARE(own->state(), AutomationContract::TaskState::Completed);
        result = gateway.dispatch(AutomationRequest(QStringLiteral("task.cancel"))
                                      .setArguments({ { QStringLiteral("taskId"), finishedId } }));
        QVERIFY(result.isSuccess());
        QCOMPARE(result.warnings().size(), 1);
        QCOMPARE(result.data().value(QStringLiteral("state")).toString(), QStringLiteral("completed"));

        // The history is bounded: the oldest finished tasks are dropped while their IDs stay allocated, so an ID the
        // session issued and has since forgotten reads as a released identity rather than as a typo.
        session.tasks().setHistoryLimit(1);
        AutomationTaskRecord* newer = session.tasks().begin(session.auditTrail(), gateway.clientId(),
                                                           AutomationContract::ActivityOrigin::User,
                                                           QStringLiteral("test.newer"), {}, session.revision(), {}, {});
        session.tasks().start(*newer);
        session.tasks().finish(*newer, AutomationResult::success());
        QVERIFY(session.tasks().find(finishedId) == nullptr);
        result = gateway.dispatch(AutomationRequest(QStringLiteral("task.describe"))
                                      .setArguments({ { QStringLiteral("taskId"), finishedId } }));
        QVERIFY(result.isError());
        QCOMPARE(result.errorCode(), AutomationContract::ErrorCode::InvalidatedObject);
        session.tasks().setHistoryLimit(AutomationTaskRegistry::defaultHistoryLimit);

        // Cancellation reaches the OVITO task behind an automation task, which is how a long step written against the
        // task system stops: the bound task is cancelled with it.
        AutomationTaskRecord* bound = session.tasks().begin(session.auditTrail(), gateway.clientId(),
                                                            AutomationContract::ActivityOrigin::User,
                                                            QStringLiteral("test.bound"), {}, session.revision(), {}, {});
        session.tasks().start(*bound);
        std::shared_ptr<Task> backingTask = std::make_shared<Task>();
        session.tasks().attachTask(*bound, backingTask);
        result = gateway.dispatch(AutomationRequest(QStringLiteral("task.cancel"))
                                      .setArguments({ { QStringLiteral("taskId"), bound->id() } }));
        QVERIFY(result.isSuccess());
        QVERIFY(backingTask->isCanceled());
        session.tasks().finish(*bound, AutomationResult::failure(AutomationContract::ErrorCode::Cancelled,
                                                                 QStringLiteral("The bound task was cancelled.")));
        QCOMPARE(bound->state(), AutomationContract::TaskState::Cancelled);
        backingTask->setFinished();
    }

    void event_log_is_ordered_and_bounded()
    {
        // The log on its own: sequence numbers keep counting while the retained history stays bounded, so a client can
        // ask for "what is new since" without the session retaining everything that ever happened.
        AutomationEventLog log;
        QCOMPARE(log.lastSequence(), quint64(0));
        QCOMPARE(log.firstSequence(), quint64(0));
        QVERIFY(log.events().isEmpty());

        log.setCapacity(3);
        for(int i = 0; i < 5; ++i) {
            log.append(AutomationEvent(AutomationContract::EventKind::Activity, 0).setSummary(QStringLiteral("step %1").arg(i)));
        }
        QCOMPARE(log.size(), 3);
        QCOMPARE(log.lastSequence(), quint64(5));
        QCOMPARE(log.firstSequence(), quint64(3));
        QCOMPARE(log.events().front().summary(), QStringLiteral("step 2"));
        QCOMPARE(log.events(3).size(), 2);
        QCOMPARE(log.events(3, 1).front().summary(), QStringLiteral("step 3"));
        QCOMPARE(log.events(0, 2).size(), 2);
        QVERIFY(log.events().front().timestamp().isValid());

        // The log assigns the position and the time; what a recorder passes for them is overwritten rather than kept.
        QCOMPARE(log.append(AutomationEvent(AutomationContract::EventKind::Activity, 42).setSummary(QStringLiteral("the newest step"))), quint64(6));
        QCOMPARE(log.events().back().sequence(), quint64(6));

        // The wire form of an event carries what a client needs to act on it, and reads back unchanged.
        const QVariantMap json = log.events().back().toJson();
        QCOMPARE(json.value(QStringLiteral("kind")).toString(), QStringLiteral("activity"));
        QCOMPARE(json.value(QStringLiteral("sequence")).toULongLong(), quint64(6));
        QCOMPARE(json.value(QStringLiteral("origin")).toString(), QStringLiteral("user"));
        QVERIFY(!json.value(QStringLiteral("timestamp")).toString().isEmpty());
        const AutomationEvent restored = AutomationEvent::fromJson(json);
        QCOMPARE(restored.kind(), AutomationContract::EventKind::Activity);
        QCOMPARE(restored.sequence(), quint64(6));
        QCOMPARE(restored.summary(), QStringLiteral("the newest step"));

        // The session's log, read through the catalog. Its history is bounded too, so the oldest events fall out while
        // their sequence numbers stay allocated.
        AutomationSession session;
        session.events().setCapacity(4);
        AutomationGateway gateway(session);
        gateway.grantCapability(AutomationContract::Capability::PipelineWrite);
        for(int i = 0; i < 4; ++i)
            QVERIFY(gateway.dispatch(AutomationRequest(QStringLiteral("scene.list_nodes"))).isSuccess());
        QVERIFY(session.events().size() <= 4);
        QVERIFY(session.events().firstSequence() > 1);

        AutomationResult result = gateway.dispatch(AutomationRequest(QStringLiteral("event.list"))
                                                       .setArguments({ { QStringLiteral("kinds"), QStringList{ QStringLiteral("activity") } } }));
        QVERIFY(result.isSuccess());
        QVERIFY(!result.data().value(QStringLiteral("events")).toList().isEmpty());
        for(const QVariant& event : result.data().value(QStringLiteral("events")).toList())
            QCOMPARE(event.toMap().value(QStringLiteral("kind")).toString(), QStringLiteral("activity"));
        QCOMPARE(result.data().value(QStringLiteral("lastSequence")).toULongLong(), session.events().lastSequence());
        QCOMPARE(result.data().value(QStringLiteral("firstRetainedSequence")).toULongLong(), session.events().firstSequence());

        // Only the kinds the vocabulary names are accepted, and the schema rejects them before anything runs.
        result = gateway.dispatch(AutomationRequest(QStringLiteral("event.list"))
                                      .setArguments({ { QStringLiteral("kinds"), QStringList{ QStringLiteral("task.exploded") } } }));
        QVERIFY(result.isError());
        QCOMPARE(result.errorCode(), AutomationContract::ErrorCode::InvalidArgument);
    }

    // -----------------------------------------------------------------------
    // Transaction boundaries
    // -----------------------------------------------------------------------

    void transaction_commits_one_undo_step_per_command()
    {
        AutomationSession session;
        session.setUserInterface(_application);
        AutomationGateway gateway(session);
        gateway.grantCapability(AutomationContract::Capability::PipelineWrite);
        registerTransactionTestCommand(gateway, session);

        OORef<DataSet> dataSet = OORef<DataSet>::create();
        session.setDataSet(dataSet);
        AnimationSettings* animation = animationSettingsOf(session);
        QVERIFY(animation);
        QCOMPARE(animation->lastFrame(), 0);
        QCOMPARE(_application->undoStack()->count(), 0);

        const AutomationResult result = gateway.dispatch(AutomationRequest(QStringLiteral("test.set_last_frame"))
                                                             .setArguments({ { QStringLiteral("lastFrame"), 42 } }));
        QVERIFY(result.isSuccess());
        QCOMPARE(animation->lastFrame(), 42);
        QVERIFY(result.transactionId().startsWith(QStringLiteral("transaction:x")));

        // One command is one undo step, named after the descriptor's undo label rather than after the operation ID.
        QCOMPARE(_application->undoStack()->count(), 1);
        QCOMPARE(_application->undoStack()->undoText(), QStringLiteral("Set the last animation frame"));

        const AutomationTransactionRecord* transaction = session.transactions().find(result.transactionId());
        QVERIFY(transaction);
        QCOMPARE(transaction->state(), AutomationContract::TransactionState::Committed);
        QVERIFY(transaction->isUndoable());
        QVERIFY(transaction->recordedOperations() >= 1);
        QCOMPARE(transaction->commands(), QStringList{ QStringLiteral("test.set_last_frame") });
        QCOMPARE(transaction->closeRevision(), result.revision());
        QCOMPARE(transaction->openRevision(), result.revision() - 1);

        // The undo step is real: what the command changed is what undo takes back.
        _application->undoStack()->undo();
        QCOMPARE(animation->lastFrame(), 0);
    }

    void transaction_rolls_back_a_failed_command()
    {
        AutomationSession session;
        session.setUserInterface(_application);
        AutomationGateway gateway(session);
        gateway.grantCapability(AutomationContract::Capability::PipelineWrite);
        registerTransactionTestCommand(gateway, session);

        session.setDataSet(OORef<DataSet>::create());
        AnimationSettings* animation = animationSettingsOf(session);
        QVERIFY(animation);
        const quint64 revisionBefore = session.revision();

        const AutomationResult result = gateway.dispatch(AutomationRequest(QStringLiteral("test.set_last_frame"))
                                                             .setArguments({ { QStringLiteral("lastFrame"), 42 },
                                                                             { QStringLiteral("failAfterChanging"), true } }));
        QVERIFY(result.isError());
        QCOMPARE(result.errorCode(), AutomationContract::ErrorCode::InvalidArgument);

        // The command changed the property and then reported a failure: the change is gone, no undo step claims it,
        // and the session revision did not move - a failure must leave neither a half-applied change nor a trace of one.
        QCOMPARE(animation->lastFrame(), 0);
        QCOMPARE(_application->undoStack()->count(), 0);
        QCOMPARE(session.revision(), revisionBefore);

        const AutomationTransactionRecord* transaction = session.transactions().find(result.transactionId());
        QVERIFY(transaction);
        QCOMPARE(transaction->state(), AutomationContract::TransactionState::Aborted);
        QVERIFY2(transaction->recordedOperations() >= 1, "the boundary must have recorded the change it took back");

        // The task keeps the failure, so a client that comes back later is told what happened.
        const AutomationTaskRecord* task = session.tasks().find(result.taskId());
        QVERIFY(task);
        QCOMPARE(task->state(), AutomationContract::TaskState::Failed);
        QCOMPARE(task->result().errorCode(), AutomationContract::ErrorCode::InvalidArgument);
    }

    void transaction_group_is_one_undo_step()
    {
        AutomationSession session;
        session.setUserInterface(_application);
        AutomationGateway gateway(session);
        gateway.grantCapability(AutomationContract::Capability::PipelineWrite);
        registerTransactionTestCommand(gateway, session);

        session.setDataSet(OORef<DataSet>::create());
        AnimationSettings* animation = animationSettingsOf(session);
        QVERIFY(animation);

        // A caller - a frontend, a later phase's plan executor - opens one boundary for several commands.
        AutomationTransaction* group = session.beginTransaction(QStringLiteral("Two test changes"),
                                                              AutomationContract::ActivityOrigin::Cli, QStringLiteral("client:c9"));
        QVERIFY(group);
        QCOMPARE(group->record()->state(), AutomationContract::TransactionState::Open);
        QCOMPARE(session.openTransaction(), group);
        QCOMPARE(group->record()->origin(), AutomationContract::ActivityOrigin::Cli);
        QCOMPARE(group->record()->clientId(), QStringLiteral("client:c9"));
        // Only one boundary is open at a time: the undo system has one current operation, and nesting would make undo
        // mean something the user cannot predict.
        QVERIFY(session.beginTransaction(QStringLiteral("Nested boundary")) == nullptr);

        const AutomationResult first = gateway.dispatch(AutomationRequest(QStringLiteral("test.set_last_frame"))
                                                             .setArguments({ { QStringLiteral("lastFrame"), 10 } }));
        QVERIFY(first.isSuccess());
        QCOMPARE(first.transactionId(), group->record()->id());
        QCOMPARE(animation->lastFrame(), 10);
        // Nothing reached the undo stack yet, because the boundary is still collecting.
        QCOMPARE(_application->undoStack()->count(), 0);

        // The second command fails after changing the same property: only its own change is taken back, because the
        // first command's change belongs to the same boundary and is still wanted.
        const AutomationResult second = gateway.dispatch(AutomationRequest(QStringLiteral("test.set_last_frame"))
                                                              .setArguments({ { QStringLiteral("lastFrame"), 20 },
                                                                              { QStringLiteral("failAfterChanging"), true } }));
        QVERIFY(second.isError());
        QCOMPARE(animation->lastFrame(), 10);
        QCOMPARE(group->record()->commandCount(), 2);
        QCOMPARE(_application->undoStack()->count(), 0);

        const QString groupId = group->record()->id();
        session.commitTransaction();
        // The boundary handed its record to the registry and keeps none of its own; the caller works with the ID.
        QVERIFY(group->record() == nullptr);
        QCOMPARE(session.openTransaction(), nullptr);
        const AutomationTransactionRecord* committed = session.transactions().find(groupId);
        QVERIFY(committed);
        QCOMPARE(committed->state(), AutomationContract::TransactionState::Committed);
        QCOMPARE(committed->label(), QStringLiteral("Two test changes"));
        QVERIFY(committed->isUndoable());
        QCOMPARE(_application->undoStack()->count(), 1);
        QCOMPARE(_application->undoStack()->undoText(), QStringLiteral("Two test changes"));
        _application->undoStack()->undo();
        QCOMPARE(animation->lastFrame(), 0);

        // A boundary that is aborted takes its commands' changes back and leaves the undo stack alone.
        AutomationTransaction* aborted = session.beginTransaction(QStringLiteral("Aborted test change"));
        QVERIFY(aborted);
        const QString abortedId = aborted->record()->id();
        QVERIFY(gateway.dispatch(AutomationRequest(QStringLiteral("test.set_last_frame"))
                                     .setArguments({ { QStringLiteral("lastFrame"), 77 } }))
                    .isSuccess());
        QCOMPARE(animation->lastFrame(), 77);
        session.abortTransaction();
        QCOMPARE(session.transactions().find(abortedId)->state(), AutomationContract::TransactionState::Aborted);
        QCOMPARE(animation->lastFrame(), 0);
        QCOMPARE(_application->undoStack()->count(), 1);
    }

    // -----------------------------------------------------------------------
    // Permissions and activity
    // -----------------------------------------------------------------------

    void permission_grants_are_recorded_and_cannot_escalate()
    {
        AutomationSession session;
        AutomationGateway gateway(session, AutomationContract::ActivityOrigin::Ai, QStringLiteral("a test agent"));

        // Connecting is an activity of the session, so a client that comes back later can see who was there.
        QVERIFY(gateway.clientId().startsWith(QStringLiteral("client:c")));
        QVERIFY(!session.events().events().isEmpty());
        QCOMPARE(session.events().events().back().kind(), AutomationContract::EventKind::Activity);
        QVERIFY(session.events().events().back().summary().contains(QStringLiteral("a test agent")));
        QCOMPARE(session.events().events().back().origin(), AutomationContract::ActivityOrigin::Ai);

        // A grant is recorded with the capability it granted, which is what "who allowed this" reads.
        const quint64 before = session.events().lastSequence();
        gateway.grantCapability(AutomationContract::Capability::PythonExecute);
        QVERIFY(gateway.permissions().contains(AutomationContract::Capability::PythonExecute));
        QCOMPARE(session.events().lastSequence(), before + 1);
        const AutomationEvent grantEvent = session.events().events(before).front();
        QCOMPARE(grantEvent.kind(), AutomationContract::EventKind::Activity);
        QCOMPARE(grantEvent.clientId(), gateway.clientId());
        QCOMPARE(grantEvent.details().value(QStringLiteral("capability")).toString(), QStringLiteral("python.execute"));

        // Granting what the client already has is not a change and is not recorded twice.
        gateway.grantCapability(AutomationContract::Capability::PythonExecute);
        QCOMPARE(session.events().lastSequence(), before + 1);

        gateway.revokeCapability(AutomationContract::Capability::PythonExecute);
        QVERIFY(!gateway.permissions().contains(AutomationContract::Capability::PythonExecute));
        QCOMPARE(session.events().lastSequence(), before + 2);

        // A denied request is not a way to gain authority: dispatching the whole catalog without a single capability
        // leaves the client with none.
        gateway.clearCapabilities();
        QVERIFY(gateway.permissions().names().isEmpty());
        for(const AutomationOperationDescriptor& descriptor : gateway.operations())
            gateway.dispatch(AutomationRequest(descriptor.id()));
        QVERIFY(gateway.permissions().names().isEmpty());

        // The read-only defaults come back through the recorded path, and reading is possible again.
        gateway.grantReadOnlyDefaults();
        QVERIFY(gateway.permissions().contains(AutomationContract::Capability::SessionRead));
        QVERIFY(gateway.dispatch(AutomationRequest(QStringLiteral("session.describe"))).isSuccess());

        // Every operation of the catalog declares what it needs, and a query never needs a capability that permits a
        // change - that is what makes a read-only client possible.
        for(const AutomationOperationDescriptor& descriptor : gateway.operations()) {
            if(!descriptor.isQuery())
                continue;
            for(AutomationContract::Capability capability : descriptor.requiredCapabilities())
                QVERIFY2(AutomationContract::isReadCapability(capability), qPrintable(descriptor.id()));
        }
    }

private:

    /// The animation settings of the session's active scene.
    ///
    /// They are the undoable core property the transaction tests change. A viewport's camera is deliberately not
    /// undoable - every camera property of Viewport carries PROPERTY_FIELD_NO_UNDO - so it cannot demonstrate an undo
    /// step, and the animation interval is the smallest core property that can.
    static AnimationSettings* animationSettingsOf(AutomationSession& session)
    {
        DataSet* dataSet = session.dataSet();
        const ViewportConfiguration* config = dataSet ? dataSet->viewportConfig() : nullptr;
        Scene* scene = config && !config->viewports().empty() ? config->viewports().front()->scene() : nullptr;
        return scene ? scene->animationSettings() : nullptr;
    }

    /// Registers the command the transaction tests dispatch: it sets the last animation frame of the session's scene,
    /// and it can report a failure after doing so - which is what a rolled-back boundary is made of.
    static void registerTransactionTestCommand(AutomationGateway& gateway, AutomationSession& session)
    {
        gateway.registerOperation(
            AutomationOperationDescriptor(QStringLiteral("test.set_last_frame"), AutomationContract::OperationKind::Command,
                                          QStringLiteral("Sets the last animation frame of the session's scene."))
                .setUndoLabel(QStringLiteral("Set the last animation frame"))
                .addRequiredCapability(AutomationContract::Capability::PipelineWrite)
                .addParameter(AutomationParameter(QStringLiteral("lastFrame"), AutomationParameter::Integer)
                                  .setDescription(QStringLiteral("The last animation frame to set.")))
                .addParameter(AutomationParameter(QStringLiteral("failAfterChanging"), AutomationParameter::Boolean, false)
                                  .setDescription(QStringLiteral("Report a failure after changing the frame."))),
            [&session](const AutomationRequest& request, AutomationResult& result) -> void {
                AnimationSettings* animation = animationSettingsOf(session);
                if(!animation) {
                    result.setError(AutomationContract::ErrorCode::InvalidRequest, QStringLiteral("The session has no scene."));
                    return;
                }
                animation->setLastFrame(request.arguments().value(QStringLiteral("lastFrame")).toInt());
                if(request.arguments().value(QStringLiteral("failAfterChanging")).toBool()) {
                    result.setError(AutomationContract::ErrorCode::InvalidArgument,
                                    QStringLiteral("The test command failed after changing the last frame."));
                    return;
                }
                result.data().insert(QStringLiteral("lastFrame"), animation->lastFrame());
            });
    }

    /// The application every test runs under, and the ambient task of the test function that is running.
    OORef<TestApplication> _application;
    std::shared_ptr<Task> _task;
    std::unique_ptr<Task::Scope> _taskScope;
};

QTEST_MAIN(AutomationContractTest)
#include "tst_automation_contracts.moc"
