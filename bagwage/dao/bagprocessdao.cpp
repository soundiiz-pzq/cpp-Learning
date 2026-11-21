#include "dao/bagprocessdao.h"
#include "db/dbmanager.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

static QSqlDatabase mainDb()
{
    return QSqlDatabase::database(QLatin1String(DatabaseManager::kMainConnection), false);
}

QList<BagProcess> BagProcessDao::listByBag(int bagId)
{
    QList<BagProcess> result;
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return result;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT id, bag_id, process_id, price FROM bag_process "
        "WHERE bag_id = ? ORDER BY process_id"));
    q.addBindValue(bagId);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return result;
    }
    while (q.next()) {
        BagProcess bp;
        bp.id = q.value(0).toInt();
        bp.bagId = q.value(1).toInt();
        bp.processId = q.value(2).toInt();
        bp.price = q.value(3).toDouble();
        result << bp;
    }
    return result;
}

int BagProcessDao::countByBag(int bagId)
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

bool BagProcessDao::removeByBag(int bagId)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM bag_process WHERE bag_id = ?"));
    q.addBindValue(bagId);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool BagProcessDao::insert(const BagProcess &bp)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO bag_process(bag_id, process_id, price) VALUES (?, ?, ?)"));
    q.addBindValue(bp.bagId);
    q.addBindValue(bp.processId);
    q.addBindValue(bp.price);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool BagProcessDao::findPrice(int bagId, int processId, double *price)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT price FROM bag_process WHERE bag_id = ? AND process_id = ?"));
    q.addBindValue(bagId);
    q.addBindValue(processId);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    if (!q.next())
        return false;   // 没有配置单价，不是错误，调用方按「未定价」处理

    if (price)
        *price = q.value(0).toDouble();
    return true;
}
