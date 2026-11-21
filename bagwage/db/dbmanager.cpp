#include "db/dbmanager.h"

#include <QDir>
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStringList>
#include <QVariant>

const char *DatabaseManager::kMainConnection = "main";

QString DatabaseManager::databasePath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("bagwage.db"));
}

bool DatabaseManager::open(QString *error)
{
    QSqlDatabase db = QSqlDatabase::database(kMainConnection, false);
    if (!db.isValid()) {
        db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                       QLatin1String(kMainConnection));
        db.setDatabaseName(databasePath());
    }

    if (!db.isOpen() && !db.open()) {
        if (error)
            *error = db.lastError().text();
        return false;
    }

    // 外键约束默认是关的，每个连接都要单独开一次
    QSqlQuery pragma(db);
    if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        if (error)
            *error = pragma.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::ensureSchema(QString *error)
{
    QSqlDatabase db = QSqlDatabase::database(kMainConnection, false);
    if (!db.isOpen()) {
        if (error)
            *error = QStringLiteral("数据库连接尚未打开，请先调用 DatabaseManager::open()");
        return false;
    }

    // 表名用 orders：order 是 SQL 关键字，直接写会语法报错
    // 自动编号用 INTEGER PRIMARY KEY AUTOINCREMENT，插入时 id 写 NULL 即可
    const QStringList ddl = QStringList()
        << QStringLiteral(
               "CREATE TABLE IF NOT EXISTS employee("
               "  id    INTEGER PRIMARY KEY AUTOINCREMENT,"
               "  name  TEXT NOT NULL,"
               "  floor TEXT NOT NULL)")
        << QStringLiteral(
               "CREATE TABLE IF NOT EXISTS process("
               "  id    INTEGER PRIMARY KEY AUTOINCREMENT,"
               "  name  TEXT NOT NULL,"
               "  price REAL NOT NULL)")
        << QStringLiteral(
               "CREATE TABLE IF NOT EXISTS bag("
               "  id   INTEGER PRIMARY KEY AUTOINCREMENT,"
               "  name TEXT NOT NULL)")
        << QStringLiteral(
               "CREATE TABLE IF NOT EXISTS bag_process("
               "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
               "  bag_id     INTEGER NOT NULL REFERENCES bag(id),"
               "  process_id INTEGER NOT NULL REFERENCES process(id),"
               "  price      REAL NOT NULL,"
               "  UNIQUE(bag_id, process_id))")
        << QStringLiteral(
               "CREATE TABLE IF NOT EXISTS orders("
               "  id     INTEGER PRIMARY KEY AUTOINCREMENT,"
               "  bag_id INTEGER NOT NULL REFERENCES bag(id),"
               "  floor  TEXT NOT NULL,"
               "  qty    INTEGER NOT NULL)")
        << QStringLiteral(
               "CREATE TABLE IF NOT EXISTS order_detail("
               "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
               "  order_id   INTEGER NOT NULL REFERENCES orders(id) ON DELETE CASCADE,"
               "  emp_id     INTEGER NOT NULL REFERENCES employee(id),"
               "  process_id INTEGER NOT NULL REFERENCES process(id),"
               "  qty        INTEGER NOT NULL,"
               "  work_date  TEXT NOT NULL)");

    // 索引：为「按员工查订单」「月度工资」「订单明细回查」服务
    const QStringList idx = QStringList()
        << QStringLiteral("CREATE INDEX IF NOT EXISTS idx_order_detail_emp ON order_detail(emp_id)")
        << QStringLiteral("CREATE INDEX IF NOT EXISTS idx_order_detail_order ON order_detail(order_id)")
        << QStringLiteral("CREATE INDEX IF NOT EXISTS idx_order_detail_date ON order_detail(work_date)")
        << QStringLiteral("CREATE INDEX IF NOT EXISTS idx_orders_bag ON orders(bag_id)")
        << QStringLiteral("CREATE INDEX IF NOT EXISTS idx_orders_floor ON orders(floor)")
        << QStringLiteral("CREATE INDEX IF NOT EXISTS idx_bag_process_bag ON bag_process(bag_id)");

    // 建表 + 建索引放进一个事务：中途失败整体回滚，不留半套表
    if (!db.transaction()) {
        if (error)
            *error = db.lastError().text();
        return false;
    }

    bool ok = true;
    QString failed;

    for (int i = 0; i < ddl.size() && ok; ++i) {
        QSqlQuery q(db);
        if (!q.exec(ddl.at(i))) {
            ok = false;
            failed = q.lastError().text() + QStringLiteral(" | SQL: ") + ddl.at(i);
        }
    }
    for (int i = 0; i < idx.size() && ok; ++i) {
        QSqlQuery q(db);
        if (!q.exec(idx.at(i))) {
            ok = false;
            failed = q.lastError().text() + QStringLiteral(" | SQL: ") + idx.at(i);
        }
    }

    if (!ok) {
        db.rollback();
        if (error)
            *error = failed;
        return false;
    }
    if (!db.commit()) {
        if (error)
            *error = db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}

QStringList DatabaseManager::existingTables(QString *error)
{
    QStringList result;
    QSqlDatabase db = QSqlDatabase::database(kMainConnection, false);
    if (!db.isOpen()) {
        if (error)
            *error = QStringLiteral("数据库连接尚未打开");
        return result;
    }

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral(
            "SELECT name FROM sqlite_master "
            "WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name"))) {
        if (error)
            *error = q.lastError().text();
        return result;
    }
    while (q.next())
        result << q.value(0).toString();
    return result;
}

void DatabaseManager::close()
{
    {
        QSqlDatabase db = QSqlDatabase::database(kMainConnection, false);
        if (db.isValid() && db.isOpen())
            db.close();
    }
    // 必须让上面的 db 对象离开作用域后再 removeDatabase，否则 Qt 会警告
    // "connection is still in use"
    if (QSqlDatabase::contains(QLatin1String(kMainConnection)))
        QSqlDatabase::removeDatabase(QLatin1String(kMainConnection));
}
