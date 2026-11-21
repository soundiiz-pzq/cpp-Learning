#include "dao/bagdao.h"
#include "db/dbmanager.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

static QSqlDatabase mainDb()
{
    return QSqlDatabase::database(QLatin1String(DatabaseManager::kMainConnection), false);
}

QList<Bag> BagDao::listAll()
{
    QList<Bag> result;
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return result;
    }

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT id, name FROM bag ORDER BY id"))) {
        m_lastError = q.lastError().text();
        return result;
    }
    while (q.next()) {
        Bag b;
        b.id = q.value(0).toInt();
        b.name = q.value(1).toString();
        result << b;
    }
    return result;
}

QList<BagSummary> BagDao::listAllWithPriceCount()
{
    QList<BagSummary> result;
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return result;
    }

    // LEFT JOIN：没配任何单价的书包也要出现在列表里（计数为 0）
    QSqlQuery q(db);
    const QString sql = QStringLiteral(
        "SELECT b.id, b.name, COUNT(bp.id) "
        "FROM bag b LEFT JOIN bag_process bp ON bp.bag_id = b.id "
        "GROUP BY b.id, b.name ORDER BY b.id");
    if (!q.exec(sql)) {
        m_lastError = q.lastError().text();
        return result;
    }
    while (q.next()) {
        BagSummary s;
        s.id = q.value(0).toInt();
        s.name = q.value(1).toString();
        s.priceCount = q.value(2).toInt();
        result << s;
    }
    return result;
}

bool BagDao::findById(int id, Bag *out)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT id, name FROM bag WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    if (!q.next()) {
        m_lastError = QStringLiteral("书包不存在（id=%1）").arg(id);
        return false;
    }
    if (out) {
        out->id = q.value(0).toInt();
        out->name = q.value(1).toString();
    }
    return true;
}

bool BagDao::insert(Bag *out)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO bag(name) VALUES (?)"));
    q.addBindValue(out->name);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    out->id = q.lastInsertId().toInt();
    return true;
}

bool BagDao::update(const Bag &b)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE bag SET name = ? WHERE id = ?"));
    q.addBindValue(b.name);
    q.addBindValue(b.id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    if (q.numRowsAffected() == 0) {
        m_lastError = QStringLiteral("没有找到要更新的书包（id=%1）").arg(b.id);
        return false;
    }
    return true;
}

bool BagDao::remove(int id)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM bag WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

int BagDao::countOrders(int bagId)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return -1;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM orders WHERE bag_id = ?"));
    q.addBindValue(bagId);
    if (!q.exec() || !q.next()) {
        m_lastError = q.lastError().text();
        return -1;
    }
    return q.value(0).toInt();
}

int BagDao::countBagProcesses(int bagId)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return -1;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM bag_process WHERE bag_id = ?"));
    q.addBindValue(bagId);
    if (!q.exec() || !q.next()) {
        m_lastError = q.lastError().text();
        return -1;
    }
    return q.value(0).toInt();
}

QString BagDao::nameOf(int id)
{
    Bag b;
    if (!findById(id, &b))
        return QString();
    return b.name;
}
