#include "dao/orderdao.h"
#include "db/dbmanager.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

static QSqlDatabase mainDb()
{
    return QSqlDatabase::database(QLatin1String(DatabaseManager::kMainConnection), false);
}

QList<Order> OrderDao::listAll()
{
    QList<Order> result;
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return result;
    }

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral(
            "SELECT id, bag_id, floor, qty, COALESCE(order_date, '') "
            "FROM orders ORDER BY id DESC"))) {
        m_lastError = q.lastError().text();
        return result;
    }
    while (q.next()) {
        Order o;
        o.id = q.value(0).toInt();
        o.bagId = q.value(1).toInt();
        o.floor = q.value(2).toString();
        o.qty = q.value(3).toInt();
        o.orderDate = q.value(4).toString();
        result << o;
    }
    return result;
}

QList<OrderSummary> OrderDao::listSummaries()
{
    QList<OrderSummary> result;
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return result;
    }

    // 合计金额取 bag_process 的单价（书包×工序），不是 process.price
    QSqlQuery q(db);
    const QString sql = QStringLiteral(
        "SELECT o.id, COALESCE(o.order_date, ''), o.bag_id, COALESCE(b.name, '(已删除)'), "
        "       o.floor, o.qty, COUNT(d.id) AS detail_count, "
        "       COALESCE(SUM(d.qty * bp.price), 0.0) AS total_amount "
        "FROM orders o "
        "LEFT JOIN bag b ON b.id = o.bag_id "
        "LEFT JOIN order_detail d ON d.order_id = o.id "
        "LEFT JOIN bag_process bp ON bp.bag_id = o.bag_id AND bp.process_id = d.process_id "
        "GROUP BY o.id "
        "ORDER BY o.id DESC");
    if (!q.exec(sql)) {
        m_lastError = q.lastError().text();
        return result;
    }
    while (q.next()) {
        OrderSummary s;
        s.id = q.value(0).toInt();
        s.orderDate = q.value(1).toString();
        s.bagId = q.value(2).toInt();
        s.bagName = q.value(3).toString();
        s.floor = q.value(4).toString();
        s.qty = q.value(5).toInt();
        s.detailCount = q.value(6).toInt();
        s.totalAmount = q.value(7).toDouble();
        result << s;
    }
    return result;
}

bool OrderDao::findById(int id, Order *out)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT id, bag_id, floor, qty, COALESCE(order_date, '') FROM orders WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    if (!q.next()) {
        m_lastError = QStringLiteral("订单不存在（id=%1）").arg(id);
        return false;
    }
    if (out) {
        out->id = q.value(0).toInt();
        out->bagId = q.value(1).toInt();
        out->floor = q.value(2).toString();
        out->qty = q.value(3).toInt();
        out->orderDate = q.value(4).toString();
    }
    return true;
}

bool OrderDao::insert(Order *out)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO orders(bag_id, floor, qty, order_date) VALUES (?, ?, ?, ?)"));
    q.addBindValue(out->bagId);
    q.addBindValue(out->floor);
    q.addBindValue(out->qty);
    q.addBindValue(out->orderDate);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    out->id = q.lastInsertId().toInt();
    return true;
}

bool OrderDao::update(const Order &o)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE orders SET bag_id = ?, floor = ?, qty = ?, order_date = ? WHERE id = ?"));
    q.addBindValue(o.bagId);
    q.addBindValue(o.floor);
    q.addBindValue(o.qty);
    q.addBindValue(o.orderDate);
    q.addBindValue(o.id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    if (q.numRowsAffected() == 0) {
        m_lastError = QStringLiteral("没有找到要更新的订单（id=%1）").arg(o.id);
        return false;
    }
    return true;
}

bool OrderDao::remove(int id)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM orders WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

int OrderDao::countAll()
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen())
        return -1;

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM orders")) || !q.next())
        return -1;
    return q.value(0).toInt();
}

int OrderDao::countByEmployee(int empId)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return -1;
    }

    // 同一订单有多条明细属于该员工时只算一个订单
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT COUNT(DISTINCT order_id) FROM order_detail WHERE emp_id = ?"));
    q.addBindValue(empId);
    if (!q.exec() || !q.next()) {
        m_lastError = q.lastError().text();
        return -1;
    }
    return q.value(0).toInt();
}
