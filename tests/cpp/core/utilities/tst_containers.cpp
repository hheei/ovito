// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#include <QTest>
#include <ovito/core/utilities/BoundedPriorityQueue.h>
#include <ovito/core/utilities/DisjointSet.h>

using namespace Ovito;

class ContainersTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // -----------------------------------------------------------------------
    // BoundedPriorityQueue
    // -----------------------------------------------------------------------

    void bpq_empty() {
        BoundedPriorityQueue<int, std::greater<int>, 8> q(4);
        QVERIFY(q.empty());
        QVERIFY(!q.full());
        QCOMPARE(q.size(), 0);
    }

    void bpq_insert_below_capacity() {
        // With std::greater the queue is a min-heap that tracks the k largest elements.
        // top() returns the minimum of the kept elements (the "worst" of the best).
        BoundedPriorityQueue<int, std::greater<int>, 8> q(4);
        q.insert(3);
        q.insert(1);
        q.insert(4);
        QCOMPARE(q.size(), 3);
        QVERIFY(!q.full());
        QCOMPARE(q.top(), 1);  // min of {1, 3, 4}
    }

    void bpq_insert_at_capacity_replaces_worst() {
        // Keep the 3 largest integers. top() is the minimum of those 3.
        BoundedPriorityQueue<int, std::greater<int>, 8> q(3);
        q.insert(5);
        q.insert(2);
        q.insert(8);
        QVERIFY(q.full());
        QCOMPARE(q.top(), 2);  // min of {2, 5, 8} — the element that would be ejected next

        // Insert 9 > top(2): 2 is ejected, queue becomes {5, 8, 9}.
        q.insert(9);
        QCOMPARE(q.size(), 3);
        QCOMPARE(q.top(), 5);  // min of {5, 8, 9}
    }

    void bpq_insert_worse_than_worst_does_nothing() {
        // Insert a value smaller than the current minimum — queue is unchanged.
        BoundedPriorityQueue<int, std::greater<int>, 8> q(3);
        q.insert(5);
        q.insert(8);
        q.insert(3);
        // full, elements = {3, 5, 8}, top = 3
        QCOMPARE(q.top(), 3);
        // Inserting 1 < top(3): skip — queue unchanged.
        q.insert(1);
        QCOMPARE(q.size(), 3);
        QCOMPARE(q.top(), 3);
    }

    void bpq_clear() {
        BoundedPriorityQueue<int, std::greater<int>, 8> q(4);
        q.insert(1);
        q.insert(2);
        q.clear();
        QVERIFY(q.empty());
        QCOMPARE(q.size(), 0);
    }

    void bpq_iteration() {
        BoundedPriorityQueue<int, std::greater<int>, 8> q(4);
        q.insert(3);
        q.insert(1);
        q.insert(4);
        q.insert(1);

        // All inserted values should appear in the iteration.
        std::vector<int> vals(q.begin(), q.end());
        std::sort(vals.begin(), vals.end());
        QCOMPARE(vals, (std::vector<int>{1, 1, 3, 4}));
    }

    // -----------------------------------------------------------------------
    // DisjointSet
    // -----------------------------------------------------------------------

    void disjoint_set_initial_state() {
        DisjointSet ds(5);
        // Each element is its own set initially.
        for(size_t i = 0; i < 5; ++i)
            QCOMPARE(ds.find(i), i);
    }

    void disjoint_set_merge() {
        DisjointSet ds(4);  // elements: 0 1 2 3

        ds.merge(0, 1);
        // 0 and 1 are in the same set.
        QCOMPARE(ds.find(0), ds.find(1));
        // 2 is still separate.
        QVERIFY(ds.find(2) != ds.find(0));

        ds.merge(2, 3);
        QCOMPARE(ds.find(2), ds.find(3));

        // Merge the two groups.
        ds.merge(0, 2);
        QCOMPARE(ds.find(0), ds.find(1));
        QCOMPARE(ds.find(0), ds.find(2));
        QCOMPARE(ds.find(0), ds.find(3));
    }

    void disjoint_set_merge_already_same_set() {
        DisjointSet ds(3);
        ds.merge(0, 1);
        size_t root_before = ds.find(0);
        ds.merge(0, 1);  // no-op
        QCOMPARE(ds.find(0), root_before);
        QVERIFY(ds.find(2) != ds.find(0));
    }

    void disjoint_set_clear() {
        DisjointSet ds(4);
        ds.merge(0, 1);
        ds.merge(2, 3);
        ds.clear();
        // After clear, every element should be its own root.
        for(size_t i = 0; i < 4; ++i)
            QCOMPARE(ds.find(i), i);
    }
};

QTEST_MAIN(ContainersTest)
#include "tst_containers.moc"
