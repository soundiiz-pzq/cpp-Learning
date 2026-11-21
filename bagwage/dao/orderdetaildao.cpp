#include "dao/orderdetaildao.h"
#include "db/dbmanager.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

static QSqlDatabase mainDb()
{
    return QSqlDatabase::database(QLatin1String(DatabaseManager::kMainConnection), false);
}

QList<OrderDetail> OrderDetailDao::listByOrder(int orderId)
{
    QList<OrderDetail> result;
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return result;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT id, order_id, emp_id, process_id, qty, work_date "
        "FROM order_detail WHERE order_id = ? ORDER BY id"));
    q.addBindValue(orderId);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return result;
    }
    while (q.next()) {
        OrderDetail d;
        d.id = q.value(0).toInt();
        d.orderId = q.value(1).toInt();
        d.empId = q.value(2).toInt();
        d.processId = q.value(3).toInt();
        d.qty = q.value(4).toInt();
        d.workDate = q.value(5).toString();
        result << d;
    }
    return result;
}

QList<OrderDetailView> OrderDetailDao::listViewByOrder(int orderId)
{
    QList<OrderDetailView> result;
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return result;
    }

    // 单价取该订单书包的 bag_process；没配价时 COALESCE 成 0，界面可据此提示
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT d.id, d.order_id, d.emp_id, d.process_id, d.qty, d.work_date, "
        "       COALESCE(e.name, '(已删除)'), COALESCE(p.name, '(已删除)'), "
        "       COALESCE(bp.price, 0.0), COALESCE(d.qty * bp.price, 0.0) "
        "FROM order_detail d "
        "LEFT JOIN employee e ON e.id = d.emp_id "
        "LEFT JOIN process p ON p.id = d.process_id "
        "LEFT JOIN orders o ON o.id = d.order_id "
        "LEFT JOIN bag_process bp ON bp.bag_id = o.bag_id AND bp.process_id = d.process_id "
        "WHERE d.order_id = ? ORDER BY d.id"));
    q.addBindValue(orderId);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return result;
    }
    while (q.next()) {
        OrderDetailView v;
        v.id = q.value(0).toInt();
        v.orderId = q.value(1).toInt();
        v.empId = q.value(2).toInt();
        v.processId = q.value(3).toInt();
        v.qty = q.value(4).toInt();
        v.workDate = q.value(5).toString();
        v.empName = q.value(6).toString();
        v.processName = q.value(7).toString();
        v.unitPrice = q.value(8).toDouble();
        v.amount = q.value(9).toDouble();
        result << v;
    }
    return result;
}

bool OrderDetailDao::insert(const OrderDetail &d)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO order_detail(order_id, emp_id, process_id, qty, work_date) "
        "VALUES (?, ?, ?, ?, ?)"));
    q.addBindValue(d.orderId);
    q.addBindValue(d.empId);
    q.addBindValue(d.processId);
    q.addBindValue(d.qty);
    q.addBindValue(d.workDate);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool OrderDetailDao::removeByOrder(int orderId)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM order_detail WHERE order_id = ?"));
    q.addBindValue(orderId);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool OrderDetailDao::remove(int id)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM order_detail WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

int OrderDetailDao::countByOrder(int orderId)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return -1;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM order_detail WHERE order_id = ?"));
    q.addBindValue(orderId);
    if (!q.exec() || !q.next()) {
        m_lastError = q.lastError().text();
        return -1;
    }
    return q.value(0).toInt();
}

int OrderDetailDao::countByEmployee(int empId)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return -1;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM order_detail WHERE emp_id = ?"));
    q.addBindValue(empId);
    if (!q.exec() || !q.next()) {
        m_lastError = q.lastError().text();
        return -1;
    }
    return q.value(0).toInt();
}
