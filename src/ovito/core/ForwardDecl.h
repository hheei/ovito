// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

/**
 * \file
 * \brief Contains forward declarations of OVITO's core classes and namespaces.
 */

#pragma once

namespace Ovito
{
    class Application;
    class FileHandle;
    class FileManager;
    class ObjectSaveStream;
    class ObjectLoadStream;
    class CompressedTextReader;
    class CompressedTextWriter;
    class VideoEncoder;
    class PromiseBase;
    class FutureBase;
    class Task;
    class TaskProgress;
    class TaskManager;
    template<typename R> class Future;
    template<typename R> class ScopedFuture;
    template<typename R> class SharedFuture;
    template<typename R> class Promise;
    template<typename R, bool StructureConcurrency> class CoroutinePromise;
    template<class R, class TaskBase> class TaskWithStorage;
    using TaskPtr = std::shared_ptr<Task>;
    struct InlineExecutor;
    class MainThreadOperation;
    class UserInterface;
    class TriangleMesh;
    class TriangleMeshVis;
    class Controller;
    class AnimationSettings;
    struct AnimationFrameLabel;
    class LookAtController;
    class KeyframeController;
    class PRSTransformationController;
    class Plugin;
    class PluginManager;
    class ApplicationService;
    class NativePlugin;
    class OvitoObject;
    class OvitoClass;
    using OvitoClassPtr = const OvitoClass*;
    template<class T> class OORef;
    class CloneHelper;
    class RefMaker;
    class RefMakerClass;
    class RefTarget;
    class PropertyFieldDescriptor;
    class PropertyFieldBase;
    template<typename property_data_type, int flags> class RuntimePropertyField;
    template<typename property_data_type, int flags> class PropertyField;
    template<typename T> class ReferenceField;
    template<typename T> class VectorReferenceField;
    class ObjectExecutor;
    class DeferredObjectExecutor;
    class DataSet;
    class DataSetContainer;
    class ParameterUnit;
    class UndoStack;
    class UndoableOperation;
    class UndoableTransaction;
    class SceneNode;
    class DataObject;
    class DataObjectReference;
    template<class T> class DataOORef;
    using ConstDataObjectRef = DataOORef<const DataObject>;
    template<typename DataObjectPtr> class OVITO_CORE_EXPORT DataObjectPathTemplate;
    using DataObjectPath = DataObjectPathTemplate<DataObject*>;
    using ConstDataObjectPath = DataObjectPathTemplate<const DataObject*>;
    using ConstDataObjectRefPath = DataObjectPathTemplate<ConstDataObjectRef>;
    class TransformedDataObject;
    class AttributeDataObject;
    class Scene;
    class DataBuffer;
    using DataBufferPtr = DataOORef<DataBuffer>;
    using ConstDataBufferPtr = DataOORef<const DataBuffer>;
    class SelectionSet;
    class Modifier;
    class ModifierClass;
    using ModifierClassPtr = const ModifierClass*;
    class ModifierGroup;
    class ModificationNode;
    class Pipeline;
    class PipelineFlowState;
    class DataCollection;
    class PipelineNode;
    class PipelineCache;
    class PipelineEvaluationRequest;
    class PipelineEvaluationResult;
    class DataVis;
    class StaticSource;
    class ModifierEvaluationRequest;
    using ModifierInitializationRequest = ModifierEvaluationRequest;
    class ModifierDelegate;
    class DelegatingModifier;
    class MultiDelegatingModifier;
    class AbstractCameraObject;
    class AbstractCameraSource;
    class FrameGraph;
    class ObjectPickingMap;
    class SceneRenderer;
    class RendererService;
    class ObjectPickInfo;
    class RenderSettings;
    class FrameBuffer;
    class CylinderPrimitive;
    class ImagePrimitive;
    class LinePrimitive;
    class MarkerPrimitive;
    class MeshPrimitive;
    class ParticlePrimitive;
    class TextPrimitive;
    class OpacityFunction;
    class Viewport;
    class ViewportConfiguration;
    class ViewportLayoutCell;
    class ViewportSettings;
    struct ViewProjectionParameters;
    class ViewportOverlay;
    class ViewportGizmo;
    class ViewportWindow;
    class FileImporter;
    class FileImporterClass;
    class FileExporter;
    class FileExporterClass;
    class FileExportJob;
    class FileSource;
    class FileSourceImporter;
    class RegisteredBufferAccess;
    class ScenePreparation;
    class ColorCodingGradient;

    class ViewportInputManager;   // Note: This class is defined in another plugin module (GuiBase).
    class ActionManager;          // Note: This class is defined in another plugin module (GuiBase).

    namespace detail {
        class TaskDependency;
        class TaskCallbackBase;
        class TaskAwaiter;
        template<typename Derived> class TaskCallback;
        template<typename R, typename TaskBase> class ContinuationTask;
    }
}
