// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

//
// Unit tests for the parts of the asynchronous task framework that need a live thread pool, i.e. a
// TaskManager. Unlike tst_concurrent.cpp (which runs without an Application and only exercises the
// InlineExecutor / direct-Task paths), these tests stand up a minimal Application so that
// Application::instance()->taskManager() — and therefore ThreadPoolExecutor, asyncLaunch(), the
// worker-thread Task::waitFor() path, and parallelFor() — are available.
//
// They pin the behavior introduced/changed in the structured-concurrency Phase 5 + single-pool work:
//   - a worker thread blocked in Task::waitFor() releases its pool slot, so a *saturated* pool does
//     not deadlock (QThreadPool::releaseThread()/reserveThread() in Task::waitFor());
//   - high-priority work oversubscribes the single pool (reserveThread()+startOnReservedThread()), so
//     it runs promptly even when every regular thread is busy with long low-priority work.
//
// Synchronization uses QTRY_VERIFY_WITH_TIMEOUT so that a regression (which would otherwise deadlock)
// fails with a timeout instead of hanging the test run.
//

#include <QTest>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <ovito/core/Core.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/concurrent/TaskManager.h>
#include <ovito/core/utilities/concurrent/Task.h>
#include <ovito/core/utilities/concurrent/Future.h>
#include <ovito/core/utilities/concurrent/Launch.h>
#include <ovito/core/utilities/concurrent/ThreadPoolExecutor.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>

using namespace Ovito;

namespace {

/// A minimal concrete Application that serves purely as a test environment: it provides the global
/// Application::instance() (and thus a TaskManager with its thread pool) without pulling in plugin
/// loading, command-line parsing, or a real Qt application object. The only pure-virtual member of
/// Application is createQtApplicationImpl(); the test never calls it (QTest already owns the
/// QCoreApplication), so the override just returns nullptr.
class TestApplication : public Application
{
public:
    using Application::Application;
protected:
    QCoreApplication* createQtApplicationImpl(bool /*supportGui*/, int& /*argc*/, char** /*argv*/) override {
        return nullptr;
    }
};

/// Brief cooperative sleep used by the busy-wait helpers below.
inline void tinySleep() { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }

} // anonymous namespace

class PoolConcurrentTest : public QObject
{
    Q_OBJECT

private:

    OORef<TestApplication> _app;
    int _defaultMaxThreadCount = 0;

private Q_SLOTS:

    void initTestCase() {
        // The Application is an OvitoObject, so it must be created through OORef::create() (which clears the
        // BeingConstructed flag and routes destruction through the proper OvitoObject deletion path); plain
        // construction would trip an assertion when the object is later destroyed.
        _app = OORef<TestApplication>::create();
        // Prime the thread-identity cache from the main thread, so isMainThread() classifies it correctly.
        QVERIFY(this_task::isMainThread());
        _defaultMaxThreadCount = _app->taskManager().maxThreadCount();
        QVERIFY(_defaultMaxThreadCount >= 1);
    }

    void cleanup() {
        // Restore the default pool size and wait for the pool to become fully idle, so each test starts from
        // a clean, quiescent state (tests share one TaskManager, and leftover in-flight workers from a
        // previous test would otherwise perturb the careful saturation set-ups below).
        _app->taskManager().setMaxThreadCount(_defaultMaxThreadCount);
        QTRY_VERIFY_WITH_TIMEOUT(_app->taskManager().threadPool()->activeThreadCount() == 0, 15000);
    }

    void cleanupTestCase() {
        // Put the TaskManager into the shutting-down state (drains the work queue and joins the pool)
        // before the Application is destroyed — its destructor asserts that this happened.
        _app->taskManager().requestShutdown();
        _app = {};
    }

    // -----------------------------------------------------------------------
    // The pool executes work and delivers the result through the value channel.
    // -----------------------------------------------------------------------

    void async_launch_delivers_result() {
        Future<int> f = asyncLaunch([]() { return 6 * 7; });
        QTRY_VERIFY_WITH_TIMEOUT(f.isFinished(), 15000);
        QVERIFY(!f.isCanceled());
        QCOMPARE(f.result(), 42);  // result() does not block once finished; no active task required
    }

    void async_launch_propagates_error() {
        Future<int> f = asyncLaunch([]() -> int { throw Exception(QStringLiteral("boom")); });
        QTRY_VERIFY_WITH_TIMEOUT(f.isFinished(), 15000);
        QVERIFY(!f.isCanceled());                       // an error is not a cancellation
        QVERIFY(f.getExceptionIfFailed().has_value());  // surfaced on the error channel
    }

    // -----------------------------------------------------------------------
    // Phase 5: a worker thread blocked in waitFor() releases its pool slot, so a saturated pool
    // still makes progress instead of deadlocking. Each of N parent tasks fills one of the pool's N
    // threads and then blocks waiting for a freshly launched child task — which itself needs a pool
    // thread to run. Without the slot release this deadlocks; with it, the children run and the
    // parents complete.
    // -----------------------------------------------------------------------

    void blocking_waitFor_does_not_deadlock_saturated_pool() {
        TaskManager& tm = _app->taskManager();
        constexpr int N = 2;
        tm.setMaxThreadCount(N);

        std::atomic<int> childrenRun{0};
        std::atomic<int> parentsDone{0};

        std::vector<Future<void>> parents;
        for(int i = 0; i < N; i++) {
            parents.push_back(asyncLaunch([&, i]() {
                // Running on a pool thread, with this parent as the active task.
                Future<int> child = asyncLaunch([&, i]() -> int {
                    childrenRun.fetch_add(1);
                    return i + 100;
                });
                // Block on the child. This is the worker-thread waitFor() path that must release the
                // parent's pool slot so the (otherwise unschedulable) child can run.
                int value = child.blockForResult();
                if(value == i + 100)
                    parentsDone.fetch_add(1);
            }));
        }

        QTRY_VERIFY_WITH_TIMEOUT(parentsDone.load() == N, 15000);
        QCOMPARE(childrenRun.load(), N);
    }

    void blocking_waitFor_does_not_deadlock_deep_chain() {
        TaskManager& tm = _app->taskManager();
        tm.setMaxThreadCount(1);  // a single thread: every level must release its slot for the next to run

        std::atomic<int> reached{0};

        // A 3-deep chain of tasks, each waiting on the next, all sharing one pool thread.
        Future<void> root = asyncLaunch([&]() {
            reached.fetch_add(1);
            Future<void> level1 = asyncLaunch([&]() {
                reached.fetch_add(1);
                Future<void> level2 = asyncLaunch([&]() {
                    reached.fetch_add(1);
                });
                level2.waitForFinished(false);
            });
            level1.waitForFinished(false);
        });

        QTRY_VERIFY_WITH_TIMEOUT(reached.load() == 3, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(root.isFinished(), 15000);
        QVERIFY(!root.isCanceled());
    }

    // -----------------------------------------------------------------------
    // Single-pool design: high-priority work oversubscribes the pool, so it starts immediately even
    // when every regular thread is occupied by long-running low-priority work.
    // -----------------------------------------------------------------------

    void high_priority_task_oversubscribes_saturated_pool() {
        TaskManager& tm = _app->taskManager();
        tm.setMaxThreadCount(1);  // exactly one regular thread

        std::atomic<bool> blockerStarted{false};
        std::atomic<bool> release{false};
        std::atomic<bool> blockerDone{false};
        std::atomic<bool> highPriorityDone{false};

        // Occupy the single regular thread with a low-priority task that spins until released. A generous
        // safety deadline guarantees the worker eventually exits even if an assertion below fails early, so a
        // regression cannot wedge the shared pool and hang shutdown.
        Future<void> blocker = asyncLaunch([&]() {
            blockerStarted.store(true);
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
            while(!release.load() && std::chrono::steady_clock::now() < deadline)
                tinySleep();
            blockerDone.store(true);
        });

        // Wait until the only regular thread is definitely occupied.
        QTRY_VERIFY_WITH_TIMEOUT(blockerStarted.load(), 15000);

        // Launch a high-priority task. It must run even though the pool is saturated — only possible via the
        // oversubscription performed by TaskManager::startWork() for high-priority work.
        Future<void> highPriority = launchFunctionAsTask(ThreadPoolExecutor(/*highPriority*/ true), [&]() {
            highPriorityDone.store(true);
        });

        QTRY_VERIFY_WITH_TIMEOUT(highPriorityDone.load(), 15000);
        // The blocker is still holding the only regular thread: the high-priority task did not merely
        // wait for it to finish, it ran concurrently on an oversubscribed thread.
        QVERIFY(!blockerDone.load());

        // Release the blocker and let everything wind down.
        release.store(true);
        QTRY_VERIFY_WITH_TIMEOUT(blockerDone.load(), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(blocker.isFinished() && highPriority.isFinished(), 15000);
    }

    // -----------------------------------------------------------------------
    // parallelFor() runs its kernel across the pool's worker threads and produces correct results.
    // (Also exercises the per-worker priority handling on the parallelFor submission path.)
    // -----------------------------------------------------------------------

    void parallel_for_runs_on_pool() {
        TaskManager& tm = _app->taskManager();
        tm.setMaxThreadCount(std::max(2, _defaultMaxThreadCount));

        constexpr size_t count = 10000;
        std::atomic<uint64_t> sum{0};
        std::atomic<bool> done{false};

        // parallelFor() requires an active task, so run it inside a pool task.
        Future<void> f = asyncLaunch([&]() {
            parallelFor(count, 1, TaskProgress::Ignore, [&](size_t i) {
                sum.fetch_add(i, std::memory_order_relaxed);
            });
            done.store(true);
        });

        QTRY_VERIFY_WITH_TIMEOUT(done.load(), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(f.isFinished(), 15000);
        QCOMPARE(sum.load(), uint64_t(count) * (count - 1) / 2);
    }
};

QTEST_MAIN(PoolConcurrentTest)
#include "tst_concurrent_pool.moc"
