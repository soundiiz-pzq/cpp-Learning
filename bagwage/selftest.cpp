#include "selftest.h"

#include "db/dbmanager.h"
#include "db/transaction.h"
#include "dao/models.h"
#include "dao/processdao.h"
#include "service/bagservice.h"
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

int countBagProcessRows(int bagId)
{
    QSqlDatabase db = QSqlDatabase::database(QLatin1String(DatabaseManager::kMainConnection));
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM bag_process WHERE bag_id = ?"));
    q.addBindValue(bagId);
    if (!q.exec() || !q.next())
        return -1;
    return q.value(0).toInt();
}

int firstId(const QString &table)
{
    QSqlDatabase db = QSqlDatabase::database(QLatin1String(DatabaseManager::kMainConnection));
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT id FROM %1 ORDER BY id LIMIT 1").arg(table)) || !q.next())
        return -1;
    return q.value(0).toInt();
}

double priceOfProcess(int bagId, int processId)
{
    QSqlDatabase db = QSqlDatabase::database(QLatin1String(DatabaseManager::kMainConnection));
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT price FROM bag_process WHERE bag_id = ? AND process_id = ?"));
    q.addBindValue(bagId);
    q.addBindValue(processId);
    if (!q.exec() || !q.next())
        return -1.0;
    return q.value(0).toDouble();
}

void testBagBasics()
{
    BagService bagService;
    ProcessService processService;

    // 两道工序：缝纫 / 包装
    Process sew;
    sew.name = QStringLiteral("缝纫");
    sew.price = 0.45;
    processService.save(&sew);

    Process pack;
    pack.name = QStringLiteral("包装");
    pack.price = 1.2;
    processService.save(&pack);

    check(sew.id > 0 && pack.id > 0, QStringLiteral("准备两道工序（缝纫/包装）"));

    // 书包 A：缝纫 2.5 / 包装 1.2
    Bag bagA;
    bagA.name = QStringLiteral("双肩包A");
    QList<BagProcess> pricesA;
    BagProcess row1;
    row1.processId = sew.id;
    row1.price = 2.5;
    pricesA << row1;
    BagProcess row2;
    row2.processId = pack.id;
    row2.price = 1.2;
    pricesA << row2;

    ServiceResult r = bagService.save(&bagA, pricesA);
    check(r.ok && bagA.id > 0, QStringLiteral("新增书包A（id=%1）").arg(bagA.id));
    check(countBagProcessRows(bagA.id) == 2, QStringLiteral("书包A写入 2 条单价"));
    check(qAbs(priceOfProcess(bagA.id, sew.id) - 2.5) < 1e-9,
          QStringLiteral("书包A 缝纫单价 = 2.5"));
    check(qAbs(priceOfProcess(bagA.id, pack.id) - 1.2) < 1e-9,
          QStringLiteral("书包A 包装单价 = 1.2"));

    // 读回
    Bag loaded;
    QList<BagProcess> loadedPrices;
    check(bagService.load(bagA.id, &loaded, &loadedPrices)
              && loaded.name == QStringLiteral("双肩包A")
              && loadedPrices.size() == 2,
          QStringLiteral("按 id 读回书包和 2 条单价"));

    // 书包 B：同一道缝纫工序，单价 3.0
    Bag bagB;
    bagB.name = QStringLiteral("单肩包B");
    QList<BagProcess> pricesB;
    BagProcess rowB;
    rowB.processId = sew.id;
    rowB.price = 3.0;
    pricesB << rowB;
    r = bagService.save(&bagB, pricesB);
    check(r.ok && qAbs(priceOfProcess(bagB.id, sew.id) - 3.0) < 1e-9,
          QStringLiteral("书包B 同一工序不同价（3.0）"));
    check(qAbs(priceOfProcess(bagA.id, sew.id) - 2.5) < 1e-9,
          QStringLiteral("改书包B 不影响书包A 的单价"));

    // 改名后单价不丢
    Bag renamed = loaded;
    renamed.name = QStringLiteral("双肩包A（改）");
    r = bagService.save(&renamed, loadedPrices);
    check(r.ok, QStringLiteral("书包A 改名成功"));
    Bag afterRename;
    QList<BagProcess> afterPrices;
    bagService.load(bagA.id, &afterRename, &afterPrices);
    check(afterRename.name == QStringLiteral("双肩包A（改）") && afterPrices.size() == 2,
          QStringLiteral("改名后 2 条单价仍在"));

    // 删掉缝纫的单价再保存 → 只剩包装
    QList<BagProcess> onlyPack;
    BagProcess packOnly;
    packOnly.processId = pack.id;
    packOnly.price = 1.2;
    onlyPack << packOnly;
    r = bagService.save(&afterRename, onlyPack);
    check(r.ok, QStringLiteral("删掉一条单价后保存成功"));
    check(countBagProcessRows(bagA.id) == 1,
          QStringLiteral("旧单价行已清理（剩 1 条）"));
    check(priceOfProcess(bagA.id, sew.id) < 0.0,
          QStringLiteral("缝纫单价确实从表里删掉了"));

    // 列表页聚合
    bool foundA = false;
    const QList<BagSummary> summaries = bagService.listAll();
    for (int i = 0; i < summaries.size(); ++i) {
        if (summaries.at(i).id == bagA.id)
            foundA = (summaries.at(i).priceCount == 1);
    }
    check(foundA, QStringLiteral("列表页工序数统计正确（书包A=1）"));
}

void testBagValidation()
{
    BagService bagService;
    const int sewId = firstId(QStringLiteral("process"));

    // 空名称
    Bag noName;
    noName.name = QStringLiteral("   ");
    QList<BagProcess> prices;
    BagProcess p;
    p.processId = sewId;
    p.price = 1.0;
    prices << p;
    check(!bagService.save(&noName, prices).ok, QStringLiteral("拒绝空书包名"));

    // 重复工序
    Bag dupBag;
    dupBag.name = QStringLiteral("重复工序");
    QList<BagProcess> dupPrices;
    BagProcess d1;
    d1.processId = sewId;
    d1.price = 1.0;
    BagProcess d2;
    d2.processId = sewId;
    d2.price = 2.0;
    dupPrices << d1 << d2;
    ServiceResult r = bagService.save(&dupBag, dupPrices);
    check(!r.ok && r.message.contains(QStringLiteral("重复")),
          QStringLiteral("同一工序重复配置被拒绝"));

    // 负单价
    Bag negative;
    negative.name = QStringLiteral("负单价");
    QList<BagProcess> negPrices;
    BagProcess n;
    n.processId = sewId;
    n.price = -1.0;
    negPrices << n;
    check(!bagService.save(&negative, negPrices).ok, QStringLiteral("拒绝负单价"));

    // 不存在的工序
    const int bagsBeforeGhost = countRows(QStringLiteral("bag"));
    Bag ghost;
    ghost.name = QStringLiteral("幽灵工序");
    QList<BagProcess> ghostPrices;
    BagProcess g;
    g.processId = 99999;
    g.price = 1.0;
    ghostPrices << g;
    r = bagService.save(&ghost, ghostPrices);
    check(!r.ok, QStringLiteral("引用不存在的工序被拒绝"));
    check(countRows(QStringLiteral("bag")) == bagsBeforeGhost,
          QStringLiteral("校验失败没有产生新书包"));

    // 允许先建书包、不配单价
    Bag emptyPriceBag;
    emptyPriceBag.name = QStringLiteral("待定价书包");
    r = bagService.save(&emptyPriceBag, QList<BagProcess>());
    check(r.ok && emptyPriceBag.id > 0 && countBagProcessRows(emptyPriceBag.id) == 0,
          QStringLiteral("允许先建书包、暂不配单价"));
}

void testBagTransaction()
{
    BagService bagService;
    QSqlDatabase db = QSqlDatabase::database(QLatin1String(DatabaseManager::kMainConnection));
    const int sewId = firstId(QStringLiteral("process"));

    // 用一个触发器强制让 bag_process 插入失败，模拟"主表已插入、子表写入报错"
    check(execSql(db, QStringLiteral(
                      "CREATE TRIGGER selftest_fail BEFORE INSERT ON bag_process "
                      "BEGIN SELECT RAISE(ABORT, 'selftest forced failure'); END")),
          QStringLiteral("创建临时触发器（强制子表写入失败）"));

    const int bagsBefore = countRows(QStringLiteral("bag"));
    const int pricesBefore = countRows(QStringLiteral("bag_process"));

    Bag willFail;
    willFail.name = QStringLiteral("会回滚的书包");
    QList<BagProcess> prices;
    BagProcess p;
    p.processId = sewId;
    p.price = 9.9;
    prices << p;

    const ServiceResult r = bagService.save(&willFail, prices);
    check(!r.ok, QStringLiteral("事务内子表写入失败 → save 返回失败"));
    check(countRows(QStringLiteral("bag")) == bagsBefore,
          QStringLiteral("回滚后书包没留下"));
    check(countRows(QStringLiteral("bag_process")) == pricesBefore,
          QStringLiteral("回滚后单价没留下"));

    execSql(db, QStringLiteral("DROP TRIGGER selftest_fail"));

    // 触发器删掉后同一份数据能正常保存，说明失败是临时的、没污染状态
    Bag retry;
    retry.name = QStringLiteral("重试书包");
    const ServiceResult r2 = bagService.save(&retry, prices);
    check(r2.ok && countBagProcessRows(retry.id) == 1,
          QStringLiteral("移除故障后重试成功"));
}

void testBagRemoval()
{
    BagService bagService;
    QSqlDatabase db = QSqlDatabase::database(QLatin1String(DatabaseManager::kMainConnection));
    const int sewId = firstId(QStringLiteral("process"));

    // 没有订单引用：可删，且连带删除单价
    Bag removable;
    removable.name = QStringLiteral("待删书包");
    QList<BagProcess> prices;
    BagProcess p;
    p.processId = sewId;
    p.price = 1.5;
    prices << p;
    bagService.save(&removable, prices);
    const int removableId = removable.id;
    check(countBagProcessRows(removableId) == 1, QStringLiteral("待删书包已配 1 条单价"));

    ServiceResult r = bagService.remove(removableId);
    check(r.ok, QStringLiteral("无订单引用 → 删除成功"));
    check(countBagProcessRows(removableId) == 0, QStringLiteral("关联单价一起删掉了"));
    Bag gone;
    check(!bagService.load(removableId, &gone, 0), QStringLiteral("书包记录确实没了"));

    // 被订单引用：拦住
    Bag referenced;
    referenced.name = QStringLiteral("被订单引用");
    QList<BagProcess> refPrices;
    BagProcess rp;
    rp.processId = sewId;
    rp.price = 2.0;
    refPrices << rp;
    bagService.save(&referenced, refPrices);

    check(execSql(db, QStringLiteral(
                      "INSERT INTO orders(bag_id, floor, qty, order_date) "
                      "VALUES (%1, '1F', 10, '2026-11-01')").arg(referenced.id)),
          QStringLiteral("给该书包插入一个订单"));

    r = bagService.remove(referenced.id);
    check(!r.ok && r.message.contains(QStringLiteral("订单")),
          QStringLiteral("被订单引用时拒绝删除，提示里写明订单"));
    check(countBagProcessRows(referenced.id) == 1,
          QStringLiteral("删除失败时单价没被误删"));
}

} // namespace

int runSelfTest()
{
    g_pass = 0;
    g_fail = 0;

    const QString tempPath = QDir(QDir::tempPath())
                                 .filePath(QStringLiteral("bagwage_selftest_%1.db")
                                               .arg(QCoreApplication::applicationPid()));

    say(QStringLiteral("===== bagwage 自检（阶段① 工序 + 阶段② 书包）====="));
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
    testBagBasics();
    testBagValidation();
    testBagTransaction();
    testBagRemoval();

    DatabaseManager::close();
    QFile::remove(tempPath);

    say(QStringLiteral("===== 结果：PASS %1 / FAIL %2 =====").arg(g_pass).arg(g_fail));
    return g_fail == 0 ? 0 : 2;
}
