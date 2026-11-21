#include "selftest.h"

#include "db/dbmanager.h"
#include "db/transaction.h"
#include "dao/models.h"
#include "dao/processdao.h"
#include "service/processservice.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLatin1String>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>
#include <QtGlobal>

#include <cstdio>

namespace {

int g_pass = 0;
int g_fail = 0;

// 输出中文：转成本地编码后用 printf，936 / 65001 控制台都能正常显示
void say(const QString &text)
{
    printf("%s\n", text.toLocal8Bit().constData());
    fflush(stdout);
}

void check(bool ok, const QString &what)
{
    if (ok)
        ++g_pass;
    else
        ++g_fail;
    say(QStringLiteral("%1 %2").arg(ok ? QStringLiteral("[PASS]")
                                       : QStringLiteral("[FAIL]"), what));
}

bool execSql(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery q(db);
    return q.exec(sql);
}

int countRows(const QString &table)
{
    QSqlDatabase db = QSqlDatabase::database(QLatin1String(DatabaseManager::kMainConnection));
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table)) || !q.next())
        return -1;
    return q.value(0).toInt();
}

bool cleanRequire(QString *error)
{
    if (!DatabaseManager::open(error)) {
        say(QStringLiteral("[FAIL] 打开临时数据库失败：%1").arg(*error));
        return false;
    }
    if (!DatabaseManager::ensureSchema(error)) {
        say(QStringLiteral("[FAIL] 建表失败：%1").arg(*error));
        return false;
    }
    check(true, QStringLiteral("打开数据库并建表（临时库）"));
    return true;
}

void testSchema()
{
    const QStringList required = QStringList()
        << QStringLiteral("bag") << QStringLiteral("bag_process")
        << QStringLiteral("employee") << QStringLiteral("order_detail")
        << QStringLiteral("orders") << QStringLiteral("process");

    const QStringList existing = DatabaseManager::existingTables();
    int missing = 0;
    for (int i = 0; i < required.size(); ++i) {
        if (!existing.contains(required.at(i)))
            ++missing;
    }
    check(missing == 0, QStringLiteral("6 张表齐全（缺 %1 张）").arg(missing));

    QSqlDatabase db = QSqlDatabase::database(QLatin1String(DatabaseManager::kMainConnection));
    bool hasOrderDate = false;
    QSqlQuery info(db);
    if (info.exec(QStringLiteral("PRAGMA table_info(orders)"))) {
        while (info.next()) {
            if (info.value(1).toString() == QLatin1String("order_date"))
                hasOrderDate = true;
        }
    }
    check(hasOrderDate, QStringLiteral("orders 表有 order_date 列（阶段③要用）"));

    // 外键：引用不存在的员工必须被拒
    QSqlQuery bad(db);
    const bool inserted = bad.exec(QStringLiteral(
        "INSERT INTO order_detail(order_id, emp_id, process_id, qty, work_date) "
        "VALUES (99999, 99999, 99999, 1, '2000-01-01')"));
    check(!inserted, QStringLiteral("外键约束生效（非法引用被拒绝）"));
}

void testProcessCrud()
{
    ProcessDao dao;
    ProcessService service;

    // 新增
    Process p;
    p.name = QStringLiteral("缝纫");
    p.price = 0.456;   // 会被规范化成 0.46
    ServiceResult r = service.save(&p);
    check(r.ok && p.id > 0, QStringLiteral("工序新增成功（id=%1）").arg(p.id));
    check(qAbs(p.price - 0.46) < 1e-9, QStringLiteral("单价规范化到两位小数（0.456 → %1）")
                                           .arg(QString::number(p.price, 'f', 2)));

    // 查询
    Process loaded;
    check(dao.findById(p.id, &loaded) && loaded.name == QStringLiteral("缝纫"),
          QStringLiteral("按 id 查回工序，中文正常"));

    // 修改
    loaded.name = QStringLiteral("缝纫（改）");
    loaded.price = 2.5;
    r = service.save(&loaded);
    Process reloaded;
    dao.findById(p.id, &reloaded);
    check(r.ok && qAbs(reloaded.price - 2.5) < 1e-9
              && reloaded.name == QStringLiteral("缝纫（改）"),
          QStringLiteral("工序修改成功"));

    // 校验：空名称
    Process emptyName;
    emptyName.name = QStringLiteral("   ");
    emptyName.price = 1.0;
    check(!service.save(&emptyName).ok, QStringLiteral("拒绝空名称"));

    // 校验：负单价
    Process negative;
    negative.name = QStringLiteral("负数单价");
    negative.price = -1.0;
    check(!service.save(&negative).ok, QStringLiteral("拒绝负单价"));

    // 无引用时删除
    Process toDelete;
    toDelete.name = QStringLiteral("临时工序");
    toDelete.price = 0.1;
    service.save(&toDelete);
    r = service.remove(toDelete.id);
    Process gone;
    check(r.ok && !dao.findById(toDelete.id, &gone),
          QStringLiteral("无引用时删除成功"));
}

void testProcessReferences()
{
    ProcessDao dao;
    ProcessService service;
    QSqlDatabase db = QSqlDatabase::database(QLatin1String(DatabaseManager::kMainConnection));

    // 准备：一款书包 + 一条明细，都引用同一道工序
    Process p;
    p.name = QStringLiteral("包装");
    p.price = 1.2;
    service.save(&p);

    execSql(db, QStringLiteral("INSERT INTO bag(name) VALUES ('书包A')"));
    QSqlQuery bagQuery(db);
    bagQuery.exec(QStringLiteral("SELECT id FROM bag ORDER BY id DESC LIMIT 1"));
    const int realBagId = bagQuery.next() ? bagQuery.value(0).toInt() : 0;
    check(realBagId > 0, QStringLiteral("插入书包A（id=%1）").arg(realBagId));

    check(execSql(db, QStringLiteral("INSERT INTO bag_process(bag_id, process_id, price) "
                                     "VALUES (%1, %2, 1.5)").arg(realBagId).arg(p.id)),
          QStringLiteral("插入 bag_process 引用（单价 1.5）"));

    check(dao.countBagProcesses(p.id) == 1, QStringLiteral("引用计数：书包单价 1 处"));
    check(dao.countOrderDetails(p.id) == 0, QStringLiteral("引用计数：计件明细 0 处"));
    check(dao.referencingBags(p.id).size() == 1
              && dao.referencingBags(p.id).at(0).name == QStringLiteral("书包A"),
          QStringLiteral("引用名称正确（书包A）"));

    // 被书包引用时必须拦住
    ServiceResult r = service.remove(p.id);
    check(!r.ok && r.message.contains(QStringLiteral("书包")),
          QStringLiteral("被书包引用时拒绝删除，提示里写明书包"));

    // 再加一条明细引用，提示要同时提到两处
    Process detailProcess;
    detailProcess.name = QStringLiteral("剪线");
    service.save(&detailProcess);

    execSql(db, QStringLiteral("INSERT INTO employee(name, floor) VALUES ('张三', '1F')"));
    QSqlQuery empQuery(db);
    empQuery.exec(QStringLiteral("SELECT id FROM employee ORDER BY id DESC LIMIT 1"));
    const int empId = empQuery.next() ? empQuery.value(0).toInt() : 0;
    execSql(db, QStringLiteral("INSERT INTO orders(bag_id, floor, qty) VALUES (%1, '1F', 10)")
                    .arg(realBagId));
    QSqlQuery orderQuery(db);
    orderQuery.exec(QStringLiteral("SELECT id FROM orders ORDER BY id DESC LIMIT 1"));
    const int orderId = orderQuery.next() ? orderQuery.value(0).toInt() : 0;

    check(execSql(db, QStringLiteral(
                      "INSERT INTO order_detail(order_id, emp_id, process_id, qty, work_date) "
                      "VALUES (%1, %2, %3, 5, '2026-11-01')")
                      .arg(orderId).arg(empId).arg(detailProcess.id)),
          QStringLiteral("插入 order_detail 引用"));

    check(dao.countOrderDetails(detailProcess.id) == 1,
          QStringLiteral("引用计数：计件明细 1 处"));
    r = service.remove(detailProcess.id);
    check(!r.ok && r.message.contains(QStringLiteral("明细")),
          QStringLiteral("被计件明细引用时拒绝删除，提示里写明明细"));
}

void testTransactionRollback()
{
    ProcessService service;
    const int before = countRows(QStringLiteral("process"));

    {
        Transaction tx;
        if (!tx.begin()) {
            check(false, QStringLiteral("开启事务失败"));
            return;
        }

        Process p;
        p.name = QStringLiteral("事务测试工序");
        p.price = 3.0;
        service.save(&p);   // 在事务内插入

        // 故意违反外键：emp_id 不存在
        QSqlDatabase db = QSqlDatabase::database(QLatin1String(DatabaseManager::kMainConnection));
        QSqlQuery bad(db);
        const bool badOk = bad.exec(QStringLiteral(
            "INSERT INTO order_detail(order_id, emp_id, process_id, qty, work_date) "
            "VALUES (99999, 99999, 1, 1, '2026-11-01')"));
        check(!badOk, QStringLiteral("事务内的非法写入被数据库拒绝"));
        // 这里不 commit，Transaction 析构会自动 rollback
    }

    check(countRows(QStringLiteral("process")) == before,
          QStringLiteral("未 commit → 自动回滚，process 行数不变"));
}

void testTransactionCommit()
{
    ProcessService service;
    const int before = countRows(QStringLiteral("process"));

    {
        Transaction tx;
        if (!tx.begin()) {
            check(false, QStringLiteral("开启事务失败"));
            return;
        }
        Process p;
        p.name = QStringLiteral("已提交工序");
        p.price = 0.8;
        service.save(&p);
        check(tx.commit(), QStringLiteral("事务 commit 成功"));
    }

    check(countRows(QStringLiteral("process")) == before + 1,
          QStringLiteral("commit 后数据落盘"));
}

} // namespace

int runSelfTest()
{
    g_pass = 0;
    g_fail = 0;

    const QString tempPath = QDir(QDir::tempPath())
                                 .filePath(QStringLiteral("bagwage_selftest_%1.db")
                                               .arg(QCoreApplication::applicationPid()));

    say(QStringLiteral("===== bagwage 自检（阶段①：工序模块）====="));
    say(QStringLiteral("临时库：%1").arg(QDir::toNativeSeparators(tempPath)));

    // 确保不碰正式库
    QFile::remove(tempPath);
    DatabaseManager::setDatabasePath(tempPath);

    QString error;
    if (!cleanRequire(&error)) {
        DatabaseManager::close();
        return 2;
    }

    testSchema();
    testProcessCrud();
    testProcessReferences();
    testTransactionRollback();
    testTransactionCommit();

    DatabaseManager::close();
    QFile::remove(tempPath);

    say(QStringLiteral("===== 结果：PASS %1 / FAIL %2 =====").arg(g_pass).arg(g_fail));
    return g_fail == 0 ? 0 : 2;
}
