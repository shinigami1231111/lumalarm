#include <QtTest>

#include "models/AlarmModel.h"

// ============================================================================
// Regression tests for the selectedId selection behavior documented in
// qml/AlarmListView.qml and qml/main.qml
// (luma.md — Open Question #2, resolved in 1.0.2).
//
// The selection rules live ONLY in QML, so this file mirrors them 1:1 in a
// minimal harness without a running QML engine (per task step 6). No
// production code was extracted for this test — AlarmManager/AlarmModel
// remain untouched.
//
//   Rule 1 — AlarmListView.qml (MouseArea onClicked):
//       clicking a row sets selectedId = that row's alarmId.
//   Rule 2 — main.qml (onDeleteAlarm): deleting a row only calls
//       alarmManager.removeAlarm(i); selectedId is NEVER modified by a
//       removal. Therefore deleting a DIFFERENT row preserves the selection,
//       and deleting the SELECTED row leaves a stale id that matches no row
//       (no highlight until the next row click).
// ============================================================================

class SelectionState
{
public:
    QString selectedId;

    // Rule 1: row click selects by alarm id.
    void rowClicked(const QString &alarmId)
    {
        selectedId = alarmId;
    }

    // Rule 2: a removal never touches selectedId (mirrors main.qml, which
    // only forwards the index to alarmManager.removeAlarm()).
    void rowRemoved(int /*row*/) {}

    // Does any row currently show the selection highlight?
    bool anyRowHighlighted(const AlarmModel &model) const
    {
        return !selectedId.isEmpty() && model.indexOfId(selectedId) != -1;
    }
};

static Alarm makeAlarm(const QString &id, int hour, const QString &name)
{
    Alarm a;
    a.id = id;
    a.hour = hour;
    a.name = name;
    return a;
}

class TestSelectionBehavior : public QObject
{
    Q_OBJECT

private slots:
    void rowClickSelectsById();
    void removingNonSelectedRowPreservesSelection();
    void removingSelectedRowLeavesNoHighlight();
};

void TestSelectionBehavior::rowClickSelectsById()
{
    AlarmModel model;
    model.addAlarm(makeAlarm("id-a", 6, "A"));
    model.addAlarm(makeAlarm("id-b", 7, "B"));
    model.addAlarm(makeAlarm("id-c", 9, "C"));
    QCOMPARE(model.rowCount(), 3);

    SelectionState sel;
    QVERIFY(!sel.anyRowHighlighted(model)); // nothing selected initially

    sel.rowClicked("id-a");
    QCOMPARE(sel.selectedId, QString("id-a"));
    QVERIFY(sel.anyRowHighlighted(model));
    QCOMPARE(model.at(model.indexOfId("id-a")).name, QString("A"));
}

void TestSelectionBehavior::removingNonSelectedRowPreservesSelection()
{
    AlarmModel model;
    model.addAlarm(makeAlarm("id-a", 6, "A"));
    model.addAlarm(makeAlarm("id-b", 7, "B"));
    model.addAlarm(makeAlarm("id-c", 9, "C"));

    SelectionState sel;
    sel.rowClicked("id-a");

    // Remove B (a DIFFERENT, non-selected row) — display order A,B,C.
    const int rowB = model.indexOfId("id-b");
    QCOMPARE(rowB, 1);
    model.removeAlarm(rowB);
    sel.rowRemoved(rowB);

    // selectedId untouched and still resolves to the same alarm.
    QCOMPARE(sel.selectedId, QString("id-a"));
    QCOMPARE(model.rowCount(), 2);
    QVERIFY(model.indexOfId("id-b") == -1);
    QCOMPARE(model.indexOfId("id-a"), 0); // A is still the first row
    QVERIFY(sel.anyRowHighlighted(model));
    QCOMPARE(model.at(0).name, QString("A"));
}

void TestSelectionBehavior::removingSelectedRowLeavesNoHighlight()
{
    AlarmModel model;
    model.addAlarm(makeAlarm("id-a", 6, "A"));
    model.addAlarm(makeAlarm("id-b", 7, "B"));
    model.addAlarm(makeAlarm("id-c", 9, "C"));

    SelectionState sel;
    sel.rowClicked("id-a");

    // Remove A itself (the SELECTED row).
    const int rowA = model.indexOfId("id-a");
    QCOMPARE(rowA, 0);
    model.removeAlarm(rowA);
    sel.rowRemoved(rowA);

    // Documented behavior: selectedId still holds the removed id (stale), but
    // it matches no row — no highlight is shown until the next row click.
    QCOMPARE(sel.selectedId, QString("id-a"));
    QVERIFY(model.indexOfId("id-a") == -1);
    QVERIFY(!sel.anyRowHighlighted(model));
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.at(0).name, QString("B"));
    QCOMPARE(model.at(1).name, QString("C"));

    // Selection is recoverable with the next click.
    sel.rowClicked("id-c");
    QVERIFY(sel.anyRowHighlighted(model));
    QCOMPARE(model.indexOfId("id-c"), 1);
}

QTEST_GUILESS_MAIN(TestSelectionBehavior)
#include "tst_selectionbehavior.moc"
