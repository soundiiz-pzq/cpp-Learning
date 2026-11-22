#include "selftest.h"

#include "db/dbmanager.h"
#include "db/transaction.h"
#include "dao/models.h"
#include "dao/processdao.h"
#include "service/bagservice.h"
#include "service/chartworker.h"
#include "service/employeeservice.h"
#include "service/monthcatalog.h"
#include "service/orderservice.h"
#include "service/processservice.h"
#include "service/wageservice.h"
#include "ui/bagdialog.h"
#include "ui/chartpage.h"
#include "ui/mainwindow.h"

#include <QApplication>
#include <QComboBox>
#include <QDate>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFocusEvent>
#include <QLabel>
#include <QTableWidget>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLatin1String>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSet>
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

// 依次尝试候选库路径，成功一个就返回。返回实际使用的路径（失败返回空）。
// 之所以要候选：有的机器上 %TEMP% 写不进 SQLite 库（实测会报
// "out of memory Error opening database"），这时退回到程序同目录。
QString openSelfTestDb(QString *error)
{
    QStringList candidates;
    candidates << QDir(QDir::tempPath())
                      .filePath(QStringLiteral("bagwage_selftest_%1.db")
                                    .arg(QCoreApplication::applicationPid()));
    candidates << QDir(QCoreApplication::applicationDirPath())
                      .filePath(QStringLiteral("bagwage_selftest_%1.db")
                                    .arg(QCoreApplication::applicationPid()));

    for (int i = 0; i < candidates.size(); ++i) {
        const QString path = candidates.at(i);
        QFile::remove(path);
        DatabaseManager::setDatabasePath(path);

        if (DatabaseManager::open(error) && DatabaseManager::ensureSchema(error)) {
            if (i > 0)
                say(QStringLiteral("（临时目录不可用，改用程序目录建自检库）"));
            return path;
        }
        DatabaseManager::close();
    }
    return QString();
}

bool cleanRequire(QString *error)
{
    if (DatabaseManager::open(error) && DatabaseManager::ensureSchema(error)) {
        check(true, QStringLiteral("打开数据库并建表（临时库）"));
        return true;
    }
    say(QStringLiteral("[FAIL] 打开临时数据库失败：%1").arg(*error));
    return false;
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

// 回归测试：点哪一行，「删除选中行」就删哪一行。
// 表格单元格被下拉/数字框占满，点击不会更新 currentRow，靠事件过滤器同步选中行。
void testBagDialogRowDeletion()
{
    QList<Process> processes;
    for (int i = 1; i <= 3; ++i) {
        Process p;
        p.id = i;
        p.name = QStringLiteral("工序%1").arg(i);
        p.price = 0.1 * i;
        processes << p;
    }

    BagDialog dialog(processes);

    for (int i = 0; i < 3; ++i)
        QMetaObject::invokeMethod(&dialog, "addPriceRow", Qt::DirectConnection);

    QTableWidget *table = dialog.findChild<QTableWidget *>();
    check(table != 0 && table->rowCount() == 3, QStringLiteral("对话框可以添加 3 行单价"));

    if (!table || table->rowCount() != 3)
        return;

    QStringList before;
    for (int row = 0; row < 3; ++row) {
        QComboBox *combo = qobject_cast<QComboBox *>(table->cellWidget(row, 0));
        before << (combo ? QString::number(combo->currentData().toInt()) : QString());
    }

    // 模拟用户点第 2 行的数字框（FocusIn 会触发事件过滤器同步选中行）
    QDoubleSpinBox *spin = qobject_cast<QDoubleSpinBox *>(table->cellWidget(1, 1));
    if (spin) {
        QFocusEvent focus(QEvent::FocusIn, Qt::MouseFocusReason);
        QApplication::sendEvent(spin, &focus);
    }

    QMetaObject::invokeMethod(&dialog, "removePriceRow", Qt::DirectConnection);

    check(table->rowCount() == 2, QStringLiteral("删除后剩 2 行"));

    QStringList after;
    for (int row = 0; row < table->rowCount(); ++row) {
        QComboBox *combo = qobject_cast<QComboBox *>(table->cellWidget(row, 0));
        after << (combo ? QString::number(combo->currentData().toInt()) : QString());
    }

    QStringList expected;
    expected << before.at(0) << before.at(2);
    check(after == expected,
          QStringLiteral("删掉的是被点中的第 2 行（剩余 %1）")
              .arg(after.join(QStringLiteral(","))));
}

void testOrderBasics()
{
    OrderService orderService;
    BagService bagService;
    ProcessService processService;
    EmployeeService employeeService;

    // 准备：员工 + 书包 + 两道工序单价
    Employee emp;
    emp.name = QStringLiteral("订单测试员工");
    emp.floor = QStringLiteral("2F");
    employeeService.save(&emp);

    Process p1;
    p1.name = QStringLiteral("订单工序A");
    p1.price = 0.5;
    processService.save(&p1);

    Process p2;
    p2.name = QStringLiteral("订单工序B");
    p2.price = 1.0;
    processService.save(&p2);

    Bag bag;
    bag.name = QStringLiteral("订单测试书包");
    QList<BagProcess> prices;
    BagProcess bp1;
    bp1.processId = p1.id;
    bp1.price = 2.0;
    prices << bp1;
    BagProcess bp2;
    bp2.processId = p2.id;
    bp2.price = 3.0;
    prices << bp2;
    check(bagService.save(&bag, prices).ok, QStringLiteral("准备测试书包（A=2.0 / B=3.0）"));

    const int ordersBefore = countRows(QStringLiteral("orders"));

    // 新建订单 + 两条明细
    Order order;
    order.bagId = bag.id;
    order.floor = QStringLiteral("2F");
    order.qty = 100;
    order.orderDate = QStringLiteral("2026-11-20");

    QList<OrderDetail> details;
    OrderDetail d1;
    d1.empId = emp.id;
    d1.processId = p1.id;
    d1.qty = 10;
    d1.workDate = QStringLiteral("2026-11-20");
    details << d1;
    OrderDetail d2;
    d2.empId = emp.id;
    d2.processId = p2.id;
    d2.qty = 5;
    d2.workDate = QStringLiteral("2026-11-21");
    details << d2;

    ServiceResult r = orderService.save(&order, details);
    check(r.ok && order.id > 0, QStringLiteral("新建订单成功（id=%1）").arg(order.id));
    check(countRows(QStringLiteral("orders")) == ordersBefore + 1,
          QStringLiteral("订单主表新增 1 行"));
    check(countRows(QStringLiteral("order_detail")) > 0, QStringLiteral("明细已写入"));

    // 明细数与合计金额：10*2.0 + 5*3.0 = 35.0
    bool found = false;
    OrderSummary summary;
    const QList<OrderSummary> summaries = orderService.listSummaries();
    for (int i = 0; i < summaries.size(); ++i) {
        if (summaries.at(i).id == order.id) {
            summary = summaries.at(i);
            found = true;
            break;
        }
    }
    check(found && summary.detailCount == 2,
          QStringLiteral("列表页明细数 = 2"));
    check(found && qAbs(summary.totalAmount - 35.0) < 1e-9,
          QStringLiteral("合计金额取书包单价：10*2.0 + 5*3.0 = %1")
              .arg(QString::number(summary.totalAmount, 'f', 2)));

    // 明细视图：员工名/工序名/单价/小计
    const QList<OrderDetailView> view = orderService.detailView(order.id);
    check(view.size() == 2, QStringLiteral("明细视图 2 行"));
    if (view.size() == 2) {
        check(view.at(0).empName == QStringLiteral("订单测试员工")
                  && view.at(0).processName == QStringLiteral("订单工序A")
                  && qAbs(view.at(0).unitPrice - 2.0) < 1e-9
                  && qAbs(view.at(0).amount - 20.0) < 1e-9,
              QStringLiteral("明细 1：员工/工序/单价/小计正确"));
    }
}

void testOrderValidation()
{
    OrderService orderService;
    BagService bagService;
    ProcessService processService;
    EmployeeService employeeService;

    // 挑一个只有部分工序定价的书包：新建一个只配 A 的
    Process priced;
    priced.name = QStringLiteral("已定价工序");
    priced.price = 0.1;
    processService.save(&priced);
    Process unpriced;
    unpriced.name = QStringLiteral("未定价工序");
    unpriced.price = 0.1;
    processService.save(&unpriced);

    Bag bag;
    bag.name = QStringLiteral("半价书包");
    QList<BagProcess> prices;
    BagProcess bp;
    bp.processId = priced.id;
    bp.price = 1.0;
    prices << bp;
    bagService.save(&bag, prices);

    Employee emp;
    emp.name = QStringLiteral("校验员工");
    emp.floor = QStringLiteral("3F");
    employeeService.save(&emp);

    const int ordersBefore = countRows(QStringLiteral("orders"));

    // 明细用了没配单价的工序
    Order o;
    o.bagId = bag.id;
    o.floor = QStringLiteral("3F");
    o.qty = 10;
    o.orderDate = QStringLiteral("2026-11-20");
    QList<OrderDetail> details;
    OrderDetail d;
    d.empId = emp.id;
    d.processId = unpriced.id;
    d.qty = 1;
    d.workDate = QStringLiteral("2026-11-20");
    details << d;
    ServiceResult r = orderService.save(&o, details);
    check(!r.ok && r.message.contains(QStringLiteral("配单价")),
          QStringLiteral("明细工序未配单价 → 拒绝，提示里写明配单价"));
    check(countRows(QStringLiteral("orders")) == ordersBefore,
          QStringLiteral("校验失败没有留下订单"));

    // 员工不存在
    Order o2;
    o2.bagId = bag.id;
    o2.floor = QStringLiteral("3F");
    o2.qty = 10;
    o2.orderDate = QStringLiteral("2026-11-20");
    QList<OrderDetail> badDetails;
    OrderDetail bad;
    bad.empId = 99999;
    bad.processId = priced.id;
    bad.qty = 1;
    bad.workDate = QStringLiteral("2026-11-20");
    badDetails << bad;
    r = orderService.save(&o2, badDetails);
    check(!r.ok && r.message.contains(QStringLiteral("员工不存在")),
          QStringLiteral("明细员工不存在 → 拒绝"));

    // 没有明细
    Order o3;
    o3.bagId = bag.id;
    o3.floor = QStringLiteral("3F");
    o3.qty = 10;
    o3.orderDate = QStringLiteral("2026-11-20");
    r = orderService.save(&o3, QList<OrderDetail>());
    check(!r.ok, QStringLiteral("没有明细 → 拒绝"));

    // 日期格式错
    Order o4;
    o4.bagId = bag.id;
    o4.floor = QStringLiteral("3F");
    o4.qty = 10;
    o4.orderDate = QStringLiteral("2026/11/20");
    QList<OrderDetail> okDetails;
    OrderDetail ok;
    ok.empId = emp.id;
    ok.processId = priced.id;
    ok.qty = 1;
    ok.workDate = QStringLiteral("2026-11-20");
    okDetails << ok;
    r = orderService.save(&o4, okDetails);
    check(!r.ok && r.message.contains(QStringLiteral("日期")),
          QStringLiteral("订单日期格式错 → 拒绝"));
}

void testOrderUpdateAndRemove()
{
    OrderService orderService;
    BagService bagService;
    ProcessService processService;
    EmployeeService employeeService;

    Employee emp;
    emp.name = QStringLiteral("编辑员工");
    emp.floor = QStringLiteral("4F");
    employeeService.save(&emp);

    Process p;
    p.name = QStringLiteral("编辑工序");
    p.price = 0.2;
    processService.save(&p);

    Bag bag;
    bag.name = QStringLiteral("编辑书包");
    QList<BagProcess> prices;
    BagProcess bp;
    bp.processId = p.id;
    bp.price = 1.5;
    prices << bp;
    bagService.save(&bag, prices);

    // 先建 2 条明细
    Order order;
    order.bagId = bag.id;
    order.floor = QStringLiteral("4F");
    order.qty = 50;
    order.orderDate = QStringLiteral("2026-11-10");

    QList<OrderDetail> details;
    OrderDetail d1;
    d1.empId = emp.id;
    d1.processId = p.id;
    d1.qty = 4;
    d1.workDate = QStringLiteral("2026-11-10");
    details << d1;
    OrderDetail d2;
    d2.empId = emp.id;
    d2.processId = p.id;
    d2.qty = 6;
    d2.workDate = QStringLiteral("2026-11-11");
    details << d2;
    orderService.save(&order, details);
    check(countBagProcessRows(bag.id) == 1, QStringLiteral("（准备）书包单价 1 条"));

    // 编辑：数量改 200，明细只留 1 条且数量改 7
    order.qty = 200;
    QList<OrderDetail> newDetails;
    OrderDetail nd;
    nd.empId = emp.id;
    nd.processId = p.id;
    nd.qty = 7;
    nd.workDate = QStringLiteral("2026-11-10");
    newDetails << nd;

    ServiceResult r = orderService.save(&order, newDetails);
    check(r.ok, QStringLiteral("编辑订单成功"));
    check(countRows(QStringLiteral("order_detail")) > 0, QStringLiteral("明细仍然存在"));

    QList<OrderDetail> stored;
    Order reloaded;
    orderService.load(order.id, &reloaded, &stored);
    check(reloaded.qty == 200, QStringLiteral("订单数量已更新为 200"));
    check(stored.size() == 1 && stored.at(0).qty == 7,
          QStringLiteral("旧明细被整组替换（剩 1 条，数量 7）"));

    // 删除订单 → 明细一起走
    const int orderId = order.id;
    r = orderService.remove(orderId);
    check(r.ok, QStringLiteral("删除订单成功"));

    QSqlDatabase db = QSqlDatabase::database(QLatin1String(DatabaseManager::kMainConnection));
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM order_detail WHERE order_id = ?"));
    q.addBindValue(orderId);
    q.exec();
    check(q.next() && q.value(0).toInt() == 0,
          QStringLiteral("订单明细被一并删除"));
    Order gone;
    check(!orderService.load(orderId, &gone, 0), QStringLiteral("订单记录确实没了"));
}

void testOrderTransactionRollback()
{
    OrderService orderService;
    BagService bagService;
    ProcessService processService;
    EmployeeService employeeService;
    QSqlDatabase db = QSqlDatabase::database(QLatin1String(DatabaseManager::kMainConnection));

    Employee emp;
    emp.name = QStringLiteral("回滚员工");
    emp.floor = QStringLiteral("5F");
    employeeService.save(&emp);

    Process p;
    p.name = QStringLiteral("回滚工序");
    p.price = 0.3;
    processService.save(&p);

    Bag bag;
    bag.name = QStringLiteral("回滚书包");
    QList<BagProcess> prices;
    BagProcess bp;
    bp.processId = p.id;
    bp.price = 1.0;
    prices << bp;
    bagService.save(&bag, prices);

    // 让 qty=999 的明细插入失败，模拟「主表已插、第二条明细报错」
    check(execSql(db, QStringLiteral(
                      "CREATE TRIGGER selftest_fail_detail BEFORE INSERT ON order_detail "
                      "WHEN NEW.qty = 999 BEGIN SELECT RAISE(ABORT, 'selftest forced failure'); END")),
          QStringLiteral("创建临时触发器（强制明细写入失败）"));

    const int ordersBefore = countRows(QStringLiteral("orders"));
    const int detailsBefore = countRows(QStringLiteral("order_detail"));

    Order order;
    order.bagId = bag.id;
    order.floor = QStringLiteral("5F");
    order.qty = 10;
    order.orderDate = QStringLiteral("2026-11-12");

    QList<OrderDetail> details;
    OrderDetail ok;
    ok.empId = emp.id;
    ok.processId = p.id;
    ok.qty = 1;
    ok.workDate = QStringLiteral("2026-11-12");
    details << ok;
    OrderDetail bad = ok;
    bad.qty = 999;
    details << bad;

    const ServiceResult r = orderService.save(&order, details);
    check(!r.ok, QStringLiteral("第二条明细失败 → save 返回失败"));
    check(countRows(QStringLiteral("orders")) == ordersBefore,
          QStringLiteral("回滚后订单没留下"));
    check(countRows(QStringLiteral("order_detail")) == detailsBefore,
          QStringLiteral("回滚后第一条明细也没留下"));

    execSql(db, QStringLiteral("DROP TRIGGER selftest_fail_detail"));

    // 故障移除后重试成功
    details.removeLast();
    Order retry;
    retry.bagId = bag.id;
    retry.floor = QStringLiteral("5F");
    retry.qty = 10;
    retry.orderDate = QStringLiteral("2026-11-12");
    const ServiceResult r2 = orderService.save(&retry, details);
    check(r2.ok, QStringLiteral("移除故障后重试成功"));
}

void testOrderPagination()
{
    OrderService orderService;
    BagService bagService;
    ProcessService processService;
    EmployeeService employeeService;

    Employee emp;
    emp.name = QStringLiteral("分页员工");
    emp.floor = QStringLiteral("6F");
    employeeService.save(&emp);

    Process p;
    p.name = QStringLiteral("分页工序");
    p.price = 0.1;
    processService.save(&p);

    Bag bag;
    bag.name = QStringLiteral("分页书包");
    QList<BagProcess> prices;
    BagProcess bp;
    bp.processId = p.id;
    bp.price = 1.0;
    prices << bp;
    check(bagService.save(&bag, prices).ok, QStringLiteral("准备分页测试书包（单价 1.0）"));

    // 15 个订单，每单 3 条明细（同一员工），总额 15 单
    int created = 0;
    for (int i = 0; i < 15; ++i) {
        Order o;
        o.bagId = bag.id;
        o.floor = QStringLiteral("6F");
        o.qty = 10 + i;
        o.orderDate = QStringLiteral("2026-10-01");

        QList<OrderDetail> details;
        for (int j = 0; j < 3; ++j) {
            OrderDetail d;
            d.empId = emp.id;
            d.processId = p.id;
            d.qty = 1 + j;
            d.workDate = QStringLiteral("2026-10-01");
            details << d;
        }
        if (orderService.save(&o, details).ok)
            ++created;
    }
    check(created == 15, QStringLiteral("造 15 个订单 × 3 条明细（实际 %1）").arg(created));

    int total = 0;
    QList<OrderSummary> page0 = orderService.page(0, 10, emp.id, &total);
    check(total == 15, QStringLiteral("按员工筛：共 15 个订单"));
    check(page0.size() == 10, QStringLiteral("第 1 页 10 个订单"));
    check(!page0.isEmpty() && page0.at(0).detailCount == 3,
          QStringLiteral("每单 3 条明细，分页没被明细撑乱"));

    QList<OrderSummary> page1 = orderService.page(1, 10, emp.id, &total);
    check(page1.size() == 5, QStringLiteral("第 2 页 5 个订单"));

    QList<OrderSummary> page2 = orderService.page(2, 10, emp.id, &total);
    check(page2.isEmpty(), QStringLiteral("超出范围的第 3 页为空"));

    QSet<int> ids;
    for (int i = 0; i < page0.size(); ++i)
        ids.insert(page0.at(i).id);
    for (int i = 0; i < page1.size(); ++i)
        ids.insert(page1.at(i).id);
    check(ids.size() == 15, QStringLiteral("两页合起来正好 15 个不重复的订单"));

    // 不筛选：总数包含前面用例造的订单，每页仍是 10 个
    int totalAll = 0;
    const QList<OrderSummary> all0 = orderService.page(0, 10, 0, &totalAll);
    check(totalAll >= 15 && all0.size() == 10,
          QStringLiteral("不筛选：总数 %1，第 1 页 10 个").arg(totalAll));

    // 另一个员工筛不到这些订单
    Employee other;
    other.name = QStringLiteral("无关员工");
    other.floor = QStringLiteral("6F");
    employeeService.save(&other);
    int otherTotal = 0;
    const QList<OrderSummary> others = orderService.page(0, 10, other.id, &otherTotal);
    check(otherTotal == 0 && others.isEmpty(),
          QStringLiteral("其他员工筛选不到这些订单"));

    // 每页 5 个时：15 单正好 3 页
    int total5 = 0;
    const QList<OrderSummary> p5 = orderService.page(0, 5, emp.id, &total5);
    check(total5 == 15 && p5.size() == 5, QStringLiteral("每页 5 个时第 1 页 5 个"));

    // 越界的页码不能崩，也不能返回上一页的数据
    int totalBig = 0;
    const QList<OrderSummary> big = orderService.page(99, 10, emp.id, &totalBig);
    check(big.isEmpty(), QStringLiteral("超大页码返回空而不是重复数据"));
}

void testWageSummary()
{
    BagService bagService;
    ProcessService processService;
    EmployeeService employeeService;
    QSqlDatabase db = QSqlDatabase::database(QLatin1String(DatabaseManager::kMainConnection));

    // 用 2025-03 / 2025-04 两个月，避开前面用例造的数据
    Employee w1;
    w1.name = QStringLiteral("工资甲");
    w1.floor = QStringLiteral("7F");
    employeeService.save(&w1);

    Employee w2;
    w2.name = QStringLiteral("工资乙");
    w2.floor = QStringLiteral("7F");
    employeeService.save(&w2);

    Process p1;
    p1.name = QStringLiteral("工资工序甲");
    p1.price = 0.45;
    processService.save(&p1);

    Process p2;
    p2.name = QStringLiteral("工资工序乙");
    p2.price = 0.25;
    processService.save(&p2);

    Bag bag;
    bag.name = QStringLiteral("工资书包");
    QList<BagProcess> prices;
    BagProcess b1;
    b1.processId = p1.id;
    b1.price = 0.45;
    prices << b1;
    BagProcess b2;
    b2.processId = p2.id;
    b2.price = 0.25;
    prices << b2;
    check(bagService.save(&bag, prices).ok, QStringLiteral("准备工资测试书包（0.45 / 0.25）"));

    // 2025-03 的订单 + 明细
    check(execSql(db, QStringLiteral(
                      "INSERT INTO orders(bag_id, floor, qty, order_date) "
                      "VALUES (%1, '7F', 100, '2025-03-01')").arg(bag.id)),
          QStringLiteral("插入 3 月订单"));
    QSqlQuery oq(db);
    oq.exec(QStringLiteral("SELECT id FROM orders ORDER BY id DESC LIMIT 1"));
    const int orderId = oq.next() ? oq.value(0).toInt() : 0;

    const int rows[][3] = {
        {w1.id, p1.id, 60},
        {w1.id, p2.id, 50},
        {w2.id, p2.id, 40},
    };
    for (int i = 0; i < 3; ++i) {
        execSql(db, QStringLiteral(
                        "INSERT INTO order_detail(order_id, emp_id, process_id, qty, work_date) "
                        "VALUES (%1, %2, %3, %4, '2025-03-10')")
                        .arg(orderId).arg(rows[i][0]).arg(rows[i][1]).arg(rows[i][2]));
    }

    // 4 月：只给甲记一条，用来验证月份隔离
    execSql(db, QStringLiteral(
                    "INSERT INTO orders(bag_id, floor, qty, order_date) "
                    "VALUES (%1, '7F', 10, '2025-04-02')").arg(bag.id));
    QSqlQuery oq2(db);
    oq2.exec(QStringLiteral("SELECT id FROM orders ORDER BY id DESC LIMIT 1"));
    const int aprilOrder = oq2.next() ? oq2.value(0).toInt() : 0;
    execSql(db, QStringLiteral(
                    "INSERT INTO order_detail(order_id, emp_id, process_id, qty, work_date) "
                    "VALUES (%1, %2, %3, 10, '2025-04-05')")
                    .arg(aprilOrder).arg(w1.id).arg(p1.id));

    // 未定价：另一款书包只配了 p1 的价，却有一条 p2 的明细
    Bag bag2;
    bag2.name = QStringLiteral("缺价书包");
    QList<BagProcess> prices2;
    BagProcess only1;
    only1.processId = p1.id;
    only1.price = 1.0;
    prices2 << only1;
    bagService.save(&bag2, prices2);

    execSql(db, QStringLiteral(
                    "INSERT INTO orders(bag_id, floor, qty, order_date) "
                    "VALUES (%1, '7F', 5, '2025-03-03')").arg(bag2.id));
    QSqlQuery oq3(db);
    oq3.exec(QStringLiteral("SELECT id FROM orders ORDER BY id DESC LIMIT 1"));
    const int missingOrder = oq3.next() ? oq3.value(0).toInt() : 0;
    execSql(db, QStringLiteral(
                    "INSERT INTO order_detail(order_id, emp_id, process_id, qty, work_date) "
                    "VALUES (%1, %2, %3, 7, '2025-03-20')")
                    .arg(missingOrder).arg(w2.id).arg(p2.id));

    WageService service;

    // 月份列表
    const QStringList months = service.months();
    check(months.contains(QStringLiteral("2025-03"))
              && months.contains(QStringLiteral("2025-04")),
          QStringLiteral("月份列表包含 2025-03 与 2025-04"));

    // 3 月汇总
    QList<WageDetailLine> unpriced;
    const QList<WageSummary> march = service.monthlySummary(QStringLiteral("2025-03"), &unpriced);
    check(march.size() == 2, QStringLiteral("3 月有 2 个员工有工资"));

    qint64 total = 0;
    qint64 w1Cents = -1;
    qint64 w2Cents = -1;
    for (int i = 0; i < march.size(); ++i) {
        total += march.at(i).totalCents;
        if (march.at(i).empId == w1.id)
            w1Cents = march.at(i).totalCents;
        if (march.at(i).empId == w2.id)
            w2Cents = march.at(i).totalCents;
    }
    check(w1Cents == 3950, QStringLiteral("甲 = 60×0.45 + 50×0.25 = %1 分")
                               .arg(w1Cents));
    check(w2Cents == 1000, QStringLiteral("乙 = 40×0.25 = %1 分（未定价的 7 件没算进来）")
                               .arg(w2Cents));
    check(total == 4950, QStringLiteral("3 月合计 = %1 分").arg(total));

    // 未定价单独列出
    check(unpriced.size() == 1, QStringLiteral("未配置单价的明细 1 条"));
    check(!unpriced.isEmpty() && unpriced.at(0).processName == QStringLiteral("工资工序乙")
              && unpriced.at(0).qty == 7,
          QStringLiteral("未定价明细的属性正确（工序乙 / 7 件）"));

    // 构成
    const QList<WageBreakdown> w1Rows = service.breakdown(w1.id, QStringLiteral("2025-03"));
    check(w1Rows.size() == 2, QStringLiteral("甲的构成 2 道工序"));
    qint64 sum = 0;
    for (int i = 0; i < w1Rows.size(); ++i)
        sum += w1Rows.at(i).amountCents;
    check(sum == 3950, QStringLiteral("构成小计相加等于总额"));

    // 月份隔离
    QList<WageDetailLine> aprilUnpriced;
    const QList<WageSummary> april = service.monthlySummary(QStringLiteral("2025-04"), &aprilUnpriced);
    check(april.size() == 1 && april.at(0).totalCents == 450,
          QStringLiteral("4 月只有甲 10×0.45 = 450 分"));

    // 空月份
    QList<WageDetailLine> emptyUnpriced;
    const QList<WageSummary> empty = service.monthlySummary(QStringLiteral("2020-01"), &emptyUnpriced);
    check(empty.isEmpty() && emptyUnpriced.isEmpty(),
          QStringLiteral("空月份返回空，不显示 0 元"));

    // 金额格式与精度
    check(WageService::formatCents(3950) == QStringLiteral("39.50"),
          QStringLiteral("formatCents(3950) = 39.50"));
    check(WageService::formatCents(5) == QStringLiteral("0.05"),
          QStringLiteral("formatCents(5) = 0.05"));
    check(WageService::centsOf(3, 0.1) == 30 && WageService::centsOf(1, 0.29) == 29,
          QStringLiteral("浮点单价按分取整（0.1×3=30 分，0.29=29 分）"));
}

// 回归：空月份必须能从下拉里选到，且不显示「合计 0.00 元」
void testWageMonthSelectorAndEmptyState()
{
    WageService service;
    const QDate today = QDate::currentDate();
    const QString current = today.toString(QStringLiteral("yyyy-MM"));
    const QString elevenAgo = today.addMonths(-11).toString(QStringLiteral("yyyy-MM"));

    const QStringList options = service.selectableMonths(12);
    check(options.contains(current), QStringLiteral("月份下拉包含本月 %1").arg(current));
    check(options.contains(elevenAgo), QStringLiteral("月份下拉包含 11 个月前 %1").arg(elevenAgo));
    check(options.size() >= 12, QStringLiteral("月份下拉至少 12 项（实际 %1）").arg(options.size()));
    check(options.contains(QStringLiteral("2025-03")),
          QStringLiteral("月份下拉也包含库里有记录的 2025-03"));

    bool descending = true;
    for (int i = 1; i < options.size(); ++i) {
        if (options.at(i - 1) <= options.at(i))
            descending = false;
    }
    check(descending, QStringLiteral("月份下拉按时间倒序（第一个是 %1）")
                          .arg(options.isEmpty() ? QString() : options.first()));

    // UI 回归：选一个没有数据的月份
    MainWindow window;
    QComboBox *combo = window.findChild<QComboBox *>(QStringLiteral("wageMonthCombo"));
    QLabel *total = window.findChild<QLabel *>(QStringLiteral("wageTotalLabel"));
    QLabel *status = window.findChild<QLabel *>(QStringLiteral("wageStatusLabel"));
    check(combo && total && status, QStringLiteral("工资页控件就绪"));
    if (!combo || !total || !status)
        return;

    const QStringList withData = service.months();
    QString emptyMonth;
    for (int i = 0; i < options.size(); ++i) {
        if (!withData.contains(options.at(i))) {
            emptyMonth = options.at(i);
            break;
        }
    }
    check(!emptyMonth.isEmpty(), QStringLiteral("下拉里存在没有数据的月份"));
    if (emptyMonth.isEmpty())
        return;

    const int index = combo->findData(emptyMonth);
    check(index >= 0, QStringLiteral("空月份 %1 在下拉里可选").arg(emptyMonth));
    if (index < 0)
        return;

    combo->setCurrentIndex(index);

    check(status->text().contains(QStringLiteral("无计件记录")),
          QStringLiteral("空月份提示「本月无计件记录」（实际：%1）").arg(status->text()));
    check(total->text().isEmpty(),
          QStringLiteral("空月份不显示「合计 0.00」（实际：%1）").arg(total->text()));

    // 切回有数据的月份，合计要恢复
    const int dataIndex = combo->findData(QStringLiteral("2025-03"));
    if (dataIndex >= 0) {
        combo->setCurrentIndex(dataIndex);
        check(!total->text().isEmpty() && total->text().contains(QStringLiteral("合计")),
              QStringLiteral("切回有数据的月份后合计恢复（实际：%1）").arg(total->text()));
        check(status->text().isEmpty(), QStringLiteral("有数据的月份不显示空提示"));
    }
}

// 图表：子线程聚合 + 过期结果丢弃
void testChartAggregation()
{
    ChartWorker sync(QStringLiteral("2025-03"), 7);
    const ChartData data = sync.compute();

    check(data.ok, QStringLiteral("图表聚合执行成功"));
    check(data.month == QStringLiteral("2025-03") && data.requestId == 7,
          QStringLiteral("回传的月份和 requestId 正确"));

    int w1 = -1;
    int w2 = -1;
    for (int i = 0; i < data.employees.size(); ++i) {
        if (data.employees.at(i).label == QStringLiteral("工资甲"))
            w1 = data.employees.at(i).qty;
        if (data.employees.at(i).label == QStringLiteral("工资乙"))
            w2 = data.employees.at(i).qty;
    }
    check(w1 == 110, QStringLiteral("员工甲产量 = 60 + 50 = %1").arg(w1));
    check(w2 == 47, QStringLiteral("员工乙产量 = 40 + 7（未定价也算产量）= %1").arg(w2));

    int p1 = -1;
    int p2 = -1;
    for (int i = 0; i < data.processes.size(); ++i) {
        if (data.processes.at(i).label == QStringLiteral("工资工序甲"))
            p1 = data.processes.at(i).qty;
        if (data.processes.at(i).label == QStringLiteral("工资工序乙"))
            p2 = data.processes.at(i).qty;
    }
    check(p1 == 60, QStringLiteral("工序甲件数 = %1").arg(p1));
    check(p2 == 97, QStringLiteral("工序乙件数 = 50 + 40 + 7 = %1").arg(p2));

    // 空月份不能崩，返回空列表
    ChartWorker empty(QStringLiteral("2021-07"), 8);
    const ChartData emptyData = empty.compute();
    check(emptyData.ok && emptyData.employees.isEmpty() && emptyData.processes.isEmpty(),
          QStringLiteral("空月份返回空数据而不是脏数据"));

    // 真线程跑一遍，结果要和同步计算一致
    ChartWorker threaded(QStringLiteral("2025-03"), 9);
    ChartData got;
    QObject::connect(&threaded, &ChartWorker::dataReady, &threaded,
                     [&got](const ChartData &d) { got = d; }, Qt::DirectConnection);
    threaded.start();
    const bool finished = threaded.wait(8000);
    check(finished, QStringLiteral("统计线程在 8 秒内结束"));
    check(got.ok && got.requestId == 9 && got.employees.size() == data.employees.size()
              && got.processes.size() == data.processes.size(),
          QStringLiteral("子线程聚合结果与同步计算一致"));
}

// 图表页 UI：默认月份能出图；空月份显示提示且不画图
void testChartPage()
{
    ChartPage page;
    QLabel *status = page.findChild<QLabel *>(QStringLiteral("chartStatusLabel"));
    QLabel *empty = page.findChild<QLabel *>(QStringLiteral("chartEmptyLabel"));
    QComboBox *combo = page.findChild<QComboBox *>(QStringLiteral("chartMonthCombo"));
    check(status && empty && combo, QStringLiteral("图表页控件就绪"));
    if (!status || !empty || !combo)
        return;

    // 等子线程结果回到主线程
    bool ready = false;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 8000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (status->text().contains(QStringLiteral("统计完成"))
            || !empty->text().isEmpty()) {
            ready = true;
            break;
        }
        QThread::msleep(10);
    }
    check(ready, QStringLiteral("图表页默认月份统计完成"));
    check(status->text().contains(QStringLiteral("统计完成")),
          QStringLiteral("默认月份出图（状态：%1）").arg(status->text()));

    // 找一个没有数据的月份
    const QStringList withData = MonthCatalog::withRecords();
    QString emptyMonth;
    for (int i = 0; i < combo->count(); ++i) {
        const QString m = combo->itemData(i).toString();
        if (!withData.contains(m)) {
            emptyMonth = m;
            break;
        }
    }
    check(!emptyMonth.isEmpty(), QStringLiteral("图表页下拉里有空月份"));
    if (emptyMonth.isEmpty())
        return;

    combo->setCurrentIndex(combo->findData(emptyMonth));

    bool emptyShown = false;
    timer.restart();
    while (timer.elapsed() < 8000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (empty->text().contains(QStringLiteral("没有计件记录"))) {
            emptyShown = true;
            break;
        }
        QThread::msleep(10);
    }
    check(emptyShown, QStringLiteral("空月份显示「该月没有计件记录」"));
}

} // namespace

int runSelfTest()
{
    g_pass = 0;
    g_fail = 0;

    say(QStringLiteral("===== bagwage 自检（①②③④⑤）====="));

    QString error;
    QString usedPath = openSelfTestDb(&error);
    if (usedPath.isEmpty()) {
        say(QStringLiteral("[FAIL] 无法创建自检数据库：%1").arg(error));
        DatabaseManager::close();
        return 2;
    }

    say(QStringLiteral("临时库：%1").arg(QDir::toNativeSeparators(usedPath)));

    if (!cleanRequire(&error)) {
        DatabaseManager::close();
        QFile::remove(usedPath);
        return 2;
    }

    testSchema();
    testBagDialogRowDeletion();
    testProcessCrud();
    testProcessReferences();
    testTransactionRollback();
    testTransactionCommit();
    testBagBasics();
    testBagValidation();
    testBagTransaction();
    testBagRemoval();
    testOrderBasics();
    testOrderValidation();
    testOrderUpdateAndRemove();
    testOrderTransactionRollback();
    testOrderPagination();
    testWageSummary();
    testWageMonthSelectorAndEmptyState();
    testChartAggregation();
    testChartPage();

    DatabaseManager::close();
    QFile::remove(usedPath);
    QFile::remove(usedPath + QStringLiteral("-journal"));

    say(QStringLiteral("===== 结果：PASS %1 / FAIL %2 =====").arg(g_pass).arg(g_fail));
    return g_fail == 0 ? 0 : 2;
}
