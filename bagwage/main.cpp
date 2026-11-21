#include "db/dbmanager.h"
#include "ui/mainwindow.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStringList>

#include <cstdio>

// 控制台中文输出：Qt5 的 QTextStream 默认按本地编码写，
// 在 UTF-8 终端里会变乱码。这里统一转成本地编码后用 printf 输出，
// 效果等同于把 QString 交给系统控制台，自动适配 936 / 65001。
static void say(const QString &text)
{
    printf("%s\n", text.toLocal8Bit().constData());
    fflush(stdout);
}

// 无界面自检：验证「能开库、6 张表都在、外键真的生效」。
// 用法：bagwage.exe --selftest
static int runSelfTest()
{
    say(QStringLiteral("===== bagwage 骨架自检 ====="));
    say(QStringLiteral("数据库文件 : %1")
            .arg(QDir::toNativeSeparators(DatabaseManager::databasePath())));

    QString error;
    if (!DatabaseManager::open(&error)) {
        say(QStringLiteral("[FAIL] 打开数据库失败：%1").arg(error));
        return 1;
    }
    say(QStringLiteral("[PASS] 打开数据库"));

    const QStringList required = QStringList()
        << QStringLiteral("bag")
        << QStringLiteral("bag_process")
        << QStringLiteral("employee")
        << QStringLiteral("order_detail")
        << QStringLiteral("orders")
        << QStringLiteral("process");

    const QStringList existing = DatabaseManager::existingTables(&error);
    if (existing.isEmpty()) {
        say(QStringLiteral("[FAIL] 读取表清单失败：%1").arg(error));
        return 1;
    }

    int missing = 0;
    for (int i = 0; i < required.size(); ++i) {
        const bool ok = existing.contains(required.at(i));
        say(QStringLiteral("%1 表存在：%2")
                .arg(ok ? QStringLiteral("[PASS]") : QStringLiteral("[FAIL]"),
                     required.at(i)));
        if (!ok)
            ++missing;
    }

    // 外键检查：插入一条引用不存在员工的明细，必须被拒绝
    QSqlQuery q(QSqlDatabase::database(QLatin1String(DatabaseManager::kMainConnection)));
    const bool badInsert = q.exec(QStringLiteral(
        "INSERT INTO order_detail(order_id, emp_id, process_id, qty, work_date) "
        "VALUES (99999, 99999, 99999, 1, '2000-01-01')"));
    say(QStringLiteral("%1 外键约束生效（非法引用被拒绝）")
            .arg(badInsert ? QStringLiteral("[FAIL]") : QStringLiteral("[PASS]")));

    say(QStringLiteral("===== 结果：%1 =====")
            .arg((missing == 0 && !badInsert) ? QStringLiteral("PASS")
                                              : QStringLiteral("FAIL")));
    return (missing == 0 && !badInsert) ? 0 : 2;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("bagwage"));
    app.setOrganizationName(QStringLiteral("bagwage"));

    QString error;
    if (!DatabaseManager::open(&error)) {
        fprintf(stderr, "open db failed: %s\n", qPrintable(error));
        return 1;
    }
    if (!DatabaseManager::ensureSchema(&error)) {
        fprintf(stderr, "create schema failed: %s\n", qPrintable(error));
        return 1;
    }

    if (app.arguments().contains(QStringLiteral("--selftest")))
        return runSelfTest();

    MainWindow window;
    window.show();

    const int code = app.exec();
    // 主连接在退出时关闭；注意必须在所有 QSqlQuery 析构之后
    DatabaseManager::close();
    return code;
}
