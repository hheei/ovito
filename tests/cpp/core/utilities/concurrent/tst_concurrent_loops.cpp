// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

//
// Tests for the coroutine-based async loop combinators in the task framework:
//   - for_each_sequential() / reduce_sequential() (ForEach.h, Reduce.h)
//   - whenAll() (WhenAll.h)
//
// These run fully inline: the loops use the InlineExecutor (so the leading ExecutorAwaiter
// hop resumes synchronously) and every iteration awaits an already-finished future (so the
// FutureAwaiter / RewindingFutureAwaiter never suspend). No Application / TaskManager is needed.
//

#include <QTest>
#include <ovito/core/Core.h>
#include <ovito/core/utilities/Exception.h>
#include <ovito/core/utilities/concurrent/Future.h>
#include <ovito/core/utilities/concurrent/SharedFuture.h>
#include <ovito/core/utilities/concurrent/ScopedFuture.h>
#include <ovito/core/utilities/concurrent/InlineExecutor.h>
#include <ovito/core/utilities/concurrent/ForEach.h>
#include <ovito/core/utilities/concurrent/Reduce.h>
#include <ovito/core/utilities/concurrent/WhenAll.h>

using namespace Ovito;

namespace {

// A scope-aware coroutine producing an immediate value. Used to verify that for_each_sequential()
// can now consume a ScopedFuture from its start function (the old machinery could not).
ScopedFuture<int> immediateScoped(int x) { co_return x; }

} // anonymous namespace

class ConcurrentLoopsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // -----------------------------------------------------------------------
    // for_each_sequential()
    // -----------------------------------------------------------------------

    void for_each_accumulates_result() {
        std::vector<int> items{1, 2, 3, 4};
        ScopedFuture<int> f = for_each_sequential(
            items,
            InlineExecutor{},
            [](int x) { return Future<int>::createImmediate(x); },       // start
            [](int /*item*/, int result, int& acc) { acc += result; },   // complete(item, result, acc&)
            0);
        QVERIFY(f.isFinished());
        QVERIFY(!f.isCanceled());
        QCOMPARE(f.result(), 10);
    }

    void for_each_start_takes_accumulator() {
        // The start function may itself receive the accumulator by reference.
        std::vector<int> items{2, 3, 4};
        ScopedFuture<int> f = for_each_sequential(
            items,
            InlineExecutor{},
            [](int x, int& acc) { acc += x; return Future<void>::createImmediateEmpty(); },
            [](int /*item*/) {},   // complete(item)
            0);
        QVERIFY(f.isFinished());
        QCOMPARE(f.result(), 9);
    }

    void for_each_void_result() {
        // No initial result -> the loop produces void; complete() collects a side effect.
        std::vector<int> items{1, 2, 3};
        int sum = 0;
        ScopedFuture<void> f = for_each_sequential(
            items,
            InlineExecutor{},
            [](int) { return Future<void>::createImmediateEmpty(); },
            [&](int x) { sum += x; });
        QVERIFY(f.isFinished());
        QVERIFY(!f.isCanceled());
        QCOMPARE(sum, 6);
    }

    void for_each_empty_range() {
        std::vector<int> items;
        int calls = 0;
        ScopedFuture<int> f = for_each_sequential(
            items,
            InlineExecutor{},
            [&](int x) { calls++; return Future<int>::createImmediate(x); },
            [](int, int, int&) {},
            123);
        QVERIFY(f.isFinished());
        QCOMPARE(calls, 0);
        QCOMPARE(f.result(), 123);   // the untouched initial accumulator is yielded
    }

    void for_each_consumes_iteration_result() {
        // complete(item, result) — no accumulator, but the iteration future carries a value.
        std::vector<int> items{10, 20};
        int collected = 0;
        ScopedFuture<void> f = for_each_sequential(
            items,
            InlineExecutor{},
            [](int x) { return Future<int>::createImmediate(x * 2); },
            [&](int /*item*/, int result) { collected += result; });
        QVERIFY(f.isFinished());
        QCOMPARE(collected, 60);
    }

    void for_each_awaits_scoped_future() {
        // Regression: the start function may return a ScopedFuture (Tier-1 owned child). The old
        // for_each_sequential could not handle this; the coroutine version awaits it like any future.
        std::vector<int> items{1, 2, 3};
        ScopedFuture<int> f = for_each_sequential(
            items,
            InlineExecutor{},
            [](int x) { return immediateScoped(x); },
            [](int, int result, int& acc) { acc += result; },
            0);
        QVERIFY(f.isFinished());
        QCOMPARE(f.result(), 6);
    }

    void for_each_propagates_start_exception() {
        std::vector<int> items{1, 2, 3};
        ScopedFuture<int> f = for_each_sequential(
            items,
            InlineExecutor{},
            [](int x) -> Future<int> {
                if(x == 2) throw Exception(QStringLiteral("boom"));
                return Future<int>::createImmediate(x);
            },
            [](int, int result, int& acc) { acc += result; },
            0);
        QVERIFY(f.isFinished());
        QVERIFY(!f.isCanceled());                       // an error is not a cancellation
        QVERIFY(f.getExceptionIfFailed().has_value());  // surfaces on the error channel
    }

    void for_each_propagates_awaited_error() {
        // An error from an awaited iteration future becomes the loop's error channel.
        std::vector<int> items{1, 2, 3};
        ScopedFuture<int> f = for_each_sequential(
            items,
            InlineExecutor{},
            [](int x) -> Future<int> {
                if(x == 2) return Future<int>::createFailed(Exception(QStringLiteral("iter failed")));
                return Future<int>::createImmediate(x);
            },
            [](int, int result, int& acc) { acc += result; },
            0);
        QVERIFY(f.isFinished());
        QVERIFY(!f.isCanceled());
        QVERIFY(f.getExceptionIfFailed().has_value());
    }

    // -----------------------------------------------------------------------
    // reduce_sequential()
    // -----------------------------------------------------------------------

    void reduce_sequential_sums() {
        ScopedFuture<int> f = reduce_sequential(
            0,
            std::vector<int>{1, 2, 3, 4, 5},
            InlineExecutor{},
            [](int x, int& acc) { acc += x; return Future<void>::createImmediateEmpty(); });
        QVERIFY(f.isFinished());
        QCOMPARE(f.result(), 15);
    }

    // -----------------------------------------------------------------------
    // whenAll()
    // -----------------------------------------------------------------------

    void when_all_preserves_finished_futures() {
        std::vector<Future<int>> futures;
        for(int i = 0; i < 3; i++)
            futures.push_back(Future<int>::createImmediate(i + 10));

        ScopedFuture<std::vector<Future<int>>> f = whenAll(std::move(futures));
        QVERIFY(f.isFinished());

        std::vector<Future<int>> result = f.result();   // futures handed back, finished and readable
        QCOMPARE((int)result.size(), 3);
        QCOMPARE(result[0].result(), 10);
        QCOMPARE(result[1].result(), 11);
        QCOMPARE(result[2].result(), 12);
    }

    void when_all_empty_range() {
        std::vector<Future<int>> futures;
        ScopedFuture<std::vector<Future<int>>> f = whenAll(std::move(futures));
        QVERIFY(f.isFinished());
        QVERIFY(f.result().empty());
    }

    void when_all_shared_futures() {
        std::vector<SharedFuture<int>> futures;
        for(int i = 0; i < 2; i++)
            futures.push_back(SharedFuture<int>(Future<int>::createImmediate(i + 1)));

        ScopedFuture<std::vector<SharedFuture<int>>> f = whenAll(std::move(futures));
        QVERIFY(f.isFinished());

        std::vector<SharedFuture<int>> result = f.result();
        QCOMPARE((int)result.size(), 2);
        QCOMPARE(result[0].result(), 1);
        QCOMPARE(result[1].result(), 2);
    }
};

QTEST_MAIN(ConcurrentLoopsTest)
#include "tst_concurrent_loops.moc"
