#include "dao/employeedao.h"
#include "db/dbmanager.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

static QSqlDatabase mainDb()
{
    return QSqlDatabase::database(QLatin1String(DatabaseManager::kMainConnection), false);
}

QList<Employee> EmployeeDao::listAll()
{
    QList<Employee> result;
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return result;
    }

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT id, name, floor FROM employee ORDER BY id"))) {
        m_lastError = q.lastError().text();
        return result;
    }
    while (q.next()) {
        Employee e;
        e.id = q.value(0).toInt();
        e.name = q.value(1).toString();
        e.floor = q.value(2).toString();
        result << e;
    }
    return result;
}

bool EmployeeDao::findById(int id, Employee *out)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT id, name, floor FROM employee WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    if (!q.next()) {
        m_lastError = QStringLiteral("员工不存在（id=%1）").arg(id);
        return false;
    }
    if (out) {
        out->id = q.value(0).toInt();
        out->name = q.value(1).toString();
        out->floor = q.value(2).toString();
    }
    return true;
}

bool EmployeeDao::insert(Employee *out)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    // id 传 NULL，交给 AUTOINCREMENT 生成
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO employee(name, floor) VALUES (?, ?)"));
    q.addBindValue(out->name);
    q.addBindValue(out->floor);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    out->id = q.lastInsertId().toInt();
    return true;
}

bool EmployeeDao::update(const Employee &e)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE employee SET name = ?, floor = ? WHERE id = ?"));
    q.addBindValue(e.name);
    q.addBindValue(e.floor);
    q.addBindValue(e.id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    if (q.numRowsAffected() == 0) {
        m_lastError = QStringLiteral("没有找到要更新的员工（id=%1）").arg(e.id);
        return false;
    }
    return true;
}

bool EmployeeDao::remove(int id)
{
    QSqlDatabase db = mainDb();
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM employee WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

int EmployeeDao::countOrderDetails(int empId)
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
