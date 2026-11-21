#include "dao/wagedao.h"
#include "db/dbmanager.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

static QSqlDatabase mainDb()
{
    return QSqlDatabase::database(QLatin1String(DatabaseManager::kMainConnection), false);
}

QStringList WageDao::months()
{
    QStringList result;
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return result;
    }

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral(
            "SELECT DISTINCT substr(work_date, 1, 7) AS m FROM order_detail "
            "ORDER BY m DESC"))) {
        m_lastError = q.lastError().text();
        return result;
    }
    while (q.next()) {
        const QString m = q.value(0).toString();
        if (!m.isEmpty())
            result << m;
    }
    return result;
}

QList<WageDetailLine> WageDao::monthLines(const QString &month)
{
    QList<WageDetailLine> result;
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return result;
    }

    // 单价取「订单的书包 × 明细的工序」：书包没配这道工序的价时 bp.price 为 NULL
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT d.emp_id, e.name, d.process_id, p.name, COALESCE(b.name, ''), "
        "       d.work_date, d.qty, COALESCE(bp.price, 0.0), "
        "       CASE WHEN bp.id IS NULL THEN 0 ELSE 1 END "
        "FROM order_detail d "
        "JOIN employee e ON e.id = d.emp_id "
        "JOIN process p ON p.id = d.process_id "
        "JOIN orders o ON o.id = d.order_id "
        "LEFT JOIN bag b ON b.id = o.bag_id "
        "LEFT JOIN bag_process bp ON bp.bag_id = o.bag_id AND bp.process_id = d.process_id "
        "WHERE substr(d.work_date, 1, 7) = ? "
        "ORDER BY d.emp_id, d.process_id, d.id"));
    q.addBindValue(month);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return result;
    }
    while (q.next()) {
        WageDetailLine line;
        line.empId = q.value(0).toInt();
        line.empName = q.value(1).toString();
        line.processId = q.value(2).toInt();
        line.processName = q.value(3).toString();
        line.bagName = q.value(4).toString();
        line.workDate = q.value(5).toString();
        line.qty = q.value(6).toInt();
        line.unitPrice = q.value(7).toDouble();
        line.priced = (q.value(8).toInt() == 1);
        result << line;
    }
    return result;
}
