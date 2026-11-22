#include "service/chartworker.h"
#include "db/dbmanager.h"

#include <QAtomicInt>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

QAtomicInt g_connectionSeq;

} // namespace

ChartWorker::ChartWorker(const QString &month, int requestId, QObject *parent)
    : QThread(parent)
    , m_month(month)
    , m_requestId(requestId)
{
    static bool registered = false;
    if (!registered) {
        qRegisterMetaType<ChartData>("ChartData");
        registered = true;
    }
}

ChartData ChartWorker::compute() const
{
    ChartData data;
    data.month = m_month;
    data.requestId = m_requestId;

    // 子线程必须用自己的连接名，不能碰主线程的 "main"
    const QString connectionName =
        QStringLiteral("chart_conn_%1").arg(g_connectionSeq.fetchAndAddOrdered(1));

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(DatabaseManager::databasePath());

        if (!db.open()) {
            data.ok = false;
            data.error = db.lastError().text();
        } else {
            {
                QSqlQuery q(db);
                q.prepare(QStringLiteral(
                    "SELECT e.name, SUM(d.qty) AS total "
                    "FROM order_detail d "
                    "JOIN employee e ON e.id = d.emp_id "
                    "WHERE substr(d.work_date, 1, 7) = ? "
                    "GROUP BY e.id "
                    "ORDER BY total DESC, e.id"));
                q.addBindValue(m_month);
                if (!q.exec()) {
                    data.error = q.lastError().text();
                } else {
                    while (q.next()) {
                        ChartBar bar;
                        bar.label = q.value(0).toString();
                        bar.qty = q.value(1).toInt();
                        data.employees << bar;
                    }
                    data.ok = true;
                }
            }   // QSqlQuery 先析构

            if (data.ok) {
                QSqlQuery q(db);
                q.prepare(QStringLiteral(
                    "SELECT p.name, SUM(d.qty) AS total "
                    "FROM order_detail d "
                    "JOIN process p ON p.id = d.process_id "
                    "WHERE substr(d.work_date, 1, 7) = ? "
                    "GROUP BY p.id "
                    "ORDER BY total DESC, p.id"));
                q.addBindValue(m_month);
                if (!q.exec()) {
                    data.error = q.lastError().text();
                    data.ok = false;
                } else {
                    while (q.next()) {
                        ChartSlice slice;
                        slice.label = q.value(0).toString();
                        slice.qty = q.value(1).toInt();
                        data.processes << slice;
                    }
                }
            }

            db.close();
        }
    }   // QSqlDatabase 也析构后

    QSqlDatabase::removeDatabase(connectionName);
    return data;
}

void ChartWorker::run()
{
    emit dataReady(compute());
}
