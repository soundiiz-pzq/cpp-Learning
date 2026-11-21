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

QString BagDao::nameOf(int id)
{
    Bag b;
    if (!findById(id, &b))
        return QString();
    return b.name;
}
