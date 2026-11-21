#include "service/bagservice.h"
#include "dao/bagdao.h"
#include "dao/bagprocessdao.h"
#include "dao/processdao.h"
#include "db/transaction.h"

#include <QSet>
#include <cmath>

QList<BagSummary> BagService::listAll() const
{
    BagDao dao;
    return dao.listAllWithPriceCount();
}

bool BagService::load(int bagId, Bag *bag, QList<BagProcess> *prices)
{
    BagDao dao;
    if (!dao.findById(bagId, bag))
        return false;

    if (prices) {
        BagProcessDao bpDao;
        *prices = bpDao.listByBag(bagId);
    }
    return true;
}

void BagService::normalize(Bag *bag)
{
    bag->name = bag->name.trimmed();
}

ServiceResult BagService::validatePrices(const QList<BagProcess> &prices)
{
    QSet<int> seen;
    ProcessDao processDao;

    for (int i = 0; i < prices.size(); ++i) {
        const BagProcess &bp = prices.at(i);

        if (bp.processId <= 0)
            return ServiceResult::failure(QStringLiteral("第 %1 行的工序无效").arg(i + 1));

        if (bp.price < 0.0)
            return ServiceResult::failure(QStringLiteral("单价不能为负数（第 %1 行）").arg(i + 1));

        if (bp.price > 10000.0)
            return ServiceResult::failure(
                QStringLiteral("单价看起来过大，上限 10000（第 %1 行）").arg(i + 1));

        if (seen.contains(bp.processId))
            return ServiceResult::failure(
                QStringLiteral("同一道工序只能配一个单价，第 %1 行重复了").arg(i + 1));
        seen.insert(bp.processId);

        // 工序必须真实存在，否则等到外键报错时提示很难懂
        Process p;
        if (!processDao.findById(bp.processId, &p))
            return ServiceResult::failure(
                QStringLiteral("第 %1 行引用的工序不存在（id=%2）")
                    .arg(i + 1)
                    .arg(bp.processId));
    }
    return ServiceResult::success();
}

ServiceResult BagService::writePrices(int bagId, const QList<BagProcess> &prices)
{
    BagProcessDao dao;

    // 先清空该书包的旧单价：用户删掉的工序不能残留，否则会继续参与工资计算
    if (!dao.removeByBag(bagId))
        return ServiceResult::failure(QStringLiteral("清空旧单价失败：%1").arg(dao.lastError()));

    for (int i = 0; i < prices.size(); ++i) {
        BagProcess row = prices.at(i);
        row.bagId = bagId;   // 以参数为准，不信调用方传进来的 bagId
        if (!dao.insert(row)) {
            return ServiceResult::failure(
                QStringLiteral("写入单价失败（第 %1 行）：%2").arg(i + 1).arg(dao.lastError()));
        }
    }
    return ServiceResult::success();
}

ServiceResult BagService::save(Bag *bag, const QList<BagProcess> &prices)
{
    if (!bag)
        return ServiceResult::failure(QStringLiteral("内部错误：书包数据为空"));

    normalize(bag);
    if (bag->name.isEmpty())
        return ServiceResult::failure(QStringLiteral("书包名称不能为空"));

    ServiceResult priceCheck = validatePrices(prices);
    if (!priceCheck.ok)
        return priceCheck;

    Transaction tx;
    if (!tx.begin())
        return ServiceResult::failure(QStringLiteral("开启事务失败：%1").arg(tx.lastError()));

    BagDao bagDao;
    const bool isNew = (bag->id <= 0);
    if (isNew) {
        if (!bagDao.insert(bag))
            return ServiceResult::failure(QStringLiteral("保存书包失败：%1").arg(bagDao.lastError()));
    } else {
        if (!bagDao.update(*bag))
            return ServiceResult::failure(QStringLiteral("保存书包失败：%1").arg(bagDao.lastError()));
    }

    ServiceResult written = writePrices(bag->id, prices);
    if (!written.ok)
        return written;   // Transaction 析构时自动回滚，书包本身也不会留下

    if (!tx.commit())
        return ServiceResult::failure(QStringLiteral("提交事务失败：%1").arg(tx.lastError()));

    return ServiceResult::success();
}

ServiceResult BagService::savePrices(int bagId, const QList<BagProcess> &prices)
{
    if (bagId <= 0)
        return ServiceResult::failure(QStringLiteral("内部错误：无效的书包 id"));

    ServiceResult priceCheck = validatePrices(prices);
    if (!priceCheck.ok)
        return priceCheck;

    Transaction tx;
    if (!tx.begin())
        return ServiceResult::failure(QStringLiteral("开启事务失败：%1").arg(tx.lastError()));

    ServiceResult written = writePrices(bagId, prices);
    if (!written.ok)
        return written;

    if (!tx.commit())
        return ServiceResult::failure(QStringLiteral("提交事务失败：%1").arg(tx.lastError()));

    return ServiceResult::success();
}

ServiceResult BagService::remove(int bagId)
{
    if (bagId <= 0)
        return ServiceResult::failure(QStringLiteral("内部错误：无效的书包 id"));

    BagDao dao;

    // 被订单引用就不能删（order_detail 通过 orders 间接引用）
    const int orderRefs = dao.countOrders(bagId);
    if (orderRefs < 0)
        return ServiceResult::failure(QStringLiteral("检查订单引用时出错：%1").arg(dao.lastError()));

    if (orderRefs > 0) {
        return ServiceResult::failure(
            QStringLiteral("这款书包已被 %1 个订单使用，不能删除。\n"
                           "请先删除相关订单。").arg(orderRefs));
    }

    Transaction tx;
    if (!tx.begin())
        return ServiceResult::failure(QStringLiteral("开启事务失败：%1").arg(tx.lastError()));

    // 单价行是书包自己的子数据，随书包一起删
    BagProcessDao bpDao;
    if (!bpDao.removeByBag(bagId))
        return ServiceResult::failure(QStringLiteral("删除单价失败：%1").arg(bpDao.lastError()));

    if (!dao.remove(bagId))
        return ServiceResult::failure(QStringLiteral("删除书包失败：%1").arg(dao.lastError()));

    if (!tx.commit())
        return ServiceResult::failure(QStringLiteral("提交事务失败：%1").arg(tx.lastError()));

    return ServiceResult::success();
}
