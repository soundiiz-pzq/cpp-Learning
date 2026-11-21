#include "dao/processdao.h"
#include "db/dbmanager.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

static QSqlDatabase mainDb()
{
    return QSqlDatabase::database(QLatin1String(DatabaseManager::kMainConnection), false);
}

QList<Process> ProcessDao::listAll()
{
    QList<Process> result;
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return result;
    }

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT id, name, price FROM process ORDER BY id"))) {
        m_lastError = q.lastError().text();
        return result;
    }
    while (q.next()) {
        Process p;
        p.id = q.value(0).toInt();
        p.name = q.value(1).toString();
        p.price = q.value(2).toDouble();
        result << p;
    }
    return result;
}

bool ProcessDao::findById(int id, Process *out)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT id, name, price FROM process WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    if (!q.next()) {
        m_lastError = QStringLiteral("工序不存在（id=%1）").arg(id);
        return false;
    }
    if (out) {
        out->id = q.value(0).toInt();
        out->name = q.value(1).toString();
        out->price = q.value(2).toDouble();
    }
    return true;
}

bool ProcessDao::insert(Process *out)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO process(name, price) VALUES (?, ?)"));
    q.addBindValue(out->name);
    q.addBindValue(out->price);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    out->id = q.lastInsertId().toInt();
    return true;
}

bool ProcessDao::update(const Process &p)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE process SET name = ?, price = ? WHERE id = ?"));
    q.addBindValue(p.name);
    q.addBindValue(p.price);
    q.addBindValue(p.id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    if (q.numRowsAffected() == 0) {
        m_lastError = QStringLiteral("没有找到要更新的工序（id=%1）").arg(p.id);
        return false;
    }
    return true;
}

bool ProcessDao::remove(int id)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM process WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

int ProcessDao::countBagProcesses(int processId)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return -1;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM bag_process WHERE process_id = ?"));
    q.addBindValue(processId);
    if (!q.exec() || !q.next()) {
        m_lastError = q.lastError().text();
        return -1;
    }
    return q.value(0).toInt();
}

int ProcessDao::countOrderDetails(int processId)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return -1;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM order_detail WHERE process_id = ?"));
    q.addBindValue(processId);
    if (!q.exec() || !q.next()) {
        m_lastError = q.lastError().text();
        return -1;
    }
    return q.value(0).toInt();
}

QList<ReferenceInfo> ProcessDao::referencingBags(int processId)
{
    QList<ReferenceInfo> result;
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return result;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT DISTINCT b.id, b.name "
        "FROM bag_process bp JOIN bag b ON b.id = bp.bag_id "
        "WHERE bp.process_id = ? ORDER BY b.id"));
    q.addBindValue(processId);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return result;
    }
    while (q.next()) {
        ReferenceInfo info;
        info.id = q.value(0).toInt();
        info.name = q.value(1).toString();
        result << info;
    }
    return result;
}
