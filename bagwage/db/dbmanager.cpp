#include "db/dbmanager.h"
#include "db/transaction.h"

#include <QCoreApplication>
#include <QDir>
#include <QProcessEnvironment>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

const char *DatabaseManager::kMainConnection = "main";

// 可覆盖的库文件路径（自检用临时库；为空表示用默认路径）
static QString g_databasePathOverride;

QString DatabaseManager::databasePath(){
    if (!g_databasePathOverride.isEmpty())
        return g_databasePathOverride;

    // 环境变量兜底：方便在 Qt Creator 的「运行环境」里临时指定测试库
    const QString fromEnv =
        QProcessEnvironment::systemEnvironment().value(QStringLiteral("BAGWAGE_DB"));
    if (!fromEnv.isEmpty())
        return fromEnv;

    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("bagwage.db"));
}

void DatabaseManager::setDatabasePath(const QString &path)
{
    g_databasePathOverride = path;
}

// 表里有没有这一列（没找到列或查询失败都按 false 处理，交给 ALTER 去报错）
static bool hasColumn(QSqlDatabase &db, const QString &table, const QString &column)
{
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table)))
        return false;
    while (q.next()) {
        if (q.value(1).toString().compare(column, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

// 缺列就 ALTER TABLE 补上（幂等）
static bool ensureColumn(QSqlDatabase &db, const QString &table, const QString &column,
                         const QString &type, QString *error)
{
    if (hasColumn(db, table, column))
        return true;

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3")
                    .arg(table, column, type))) {
        if (error)
            *error = q.lastError().text()
                     + QStringLiteral(" | ALTER TABLE %1 ADD %2").arg(table, column);
        return false;
    }
    return true;
}

bool DatabaseManager::open(QString *error)
{
    const QString wanted = databasePath();

    QSqlDatabase db = QSqlDatabase::database(kMainConnection, false);
    if (!db.isValid()) {
        db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                       QLatin1String(kMainConnection));
        db.setDatabaseName(wanted);
    } else if (db.isOpen() && db.databaseName() != wanted) {
        // 调用方换了库路径（比如自检切临时库）：先把旧连接关掉再换文件，
        // 否则会静默地继续往旧库写数据。
        db.close();
        db.setDatabaseName(wanted);
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
    // id 用 INTEGER PRIMARY KEY AUTOINCREMENT，插入时 id 写 NULL 交给数据库生成
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
               "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
               "  bag_id     INTEGER NOT NULL REFERENCES bag(id),"
               "  floor      TEXT NOT NULL,"
               "  qty        INTEGER NOT NULL,"
               "  order_date TEXT)")
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
    {
        Transaction tx;
        if (!tx.begin()) {
            if (error)
                *error = tx.lastError();
            return false;
        }

        QString failed;
        for (int i = 0; i < ddl.size(); ++i) {
            QSqlQuery q(db);
            if (!q.exec(ddl.at(i))) {
                failed = q.lastError().text() + QStringLiteral(" | SQL: ") + ddl.at(i);
                if (error)
                    *error = failed;
                return false;   // Transaction 析构会自动回滚
            }
        }
        for (int i = 0; i < idx.size(); ++i) {
            QSqlQuery q(db);
            if (!q.exec(idx.at(i))) {
                failed = q.lastError().text() + QStringLiteral(" | SQL: ") + idx.at(i);
                if (error)
                    *error = failed;
                return false;
            }
        }

        // 轻量迁移：老版本建的库没有 orders.order_date，补一列
        // （CREATE TABLE IF NOT EXISTS 不会给已存在的表加列）
        if (!ensureColumn(db, QStringLiteral("orders"), QStringLiteral("order_date"),
                          QStringLiteral("TEXT"), &failed)) {
            if (error)
                *error = failed;
            return false;
        }

        if (!tx.commit()) {
            if (error)
                *error = tx.lastError();
            return false;
        }
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
