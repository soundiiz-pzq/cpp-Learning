#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QString>
#include <QStringList>

class QSqlDatabase;

// 数据库连接 + 建表的唯一入口。
//
// 约定：
//   * 主线程永远用连接名 "main"（见 kMainConnection）；
//   * SQLite 连接有线程亲和性，子线程必须 addDatabase 一个自己的新连接，
//     只需要 DatabaseManager::databasePath() 拿到库文件路径即可；
//   * DAO 层一律通过 QSqlDatabase::database(kMainConnection) 取连接，
//     不自己 addDatabase，避免连接名混乱。
class DatabaseManager
{
public:
    static const char *kMainConnection;

    // 库文件默认放在程序同目录；可用环境变量 BAGWAGE_DB 覆盖（自检用临时库）。
    static QString databasePath();
    static void    setDatabasePath(const QString &path);

    // 打开主连接；连接已存在时直接复用（重复打开不会报错）
    static bool open(QString *error = 0);

    // 把 6 张表 + 索引建好（幂等，可重复调用）
    static bool ensureSchema(QString *error = 0);

    // 已存在的表名（自检和界面展示用）
    static QStringList existingTables(QString *error = 0);

    static void close();
};

#endif // DBMANAGER_H
