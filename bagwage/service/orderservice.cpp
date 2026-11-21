#include "service/orderservice.h"
#include "dao/bagdao.h"
#include "dao/bagprocessdao.h"
#include "dao/employeedao.h"
#include "dao/orderdao.h"
#include "dao/orderdetaildao.h"
#include "dao/processdao.h"
#include "db/transaction.h"

#include <QDate>

namespace {

bool isValidDate(const QString &text)
{
    return QDate::fromString(text, QStringLiteral("yyyy-MM-dd")).isValid();
}

} // namespace

QList<OrderSummary> OrderService::listSummaries() const
{
    OrderDao dao;
    return dao.listSummaries();
}

QList<OrderSummary> OrderService::page(int pageIndex, int pageSize, int empFilter,
                                       int *totalCount) const
{
    OrderDao dao;
    return dao.pageSummaries(pageIndex, pageSize, empFilter, totalCount);
}

bool OrderService::load(int orderId, Order *order, QList<OrderDetail> *details)
{
    OrderDao dao;
    if (!dao.findById(orderId, order))
        return false;

    if (details) {
        OrderDetailDao detailDao;
        *details = detailDao.listByOrder(orderId);
    }
    return true;
}

QList<OrderDetailView> OrderService::detailView(int orderId) const
{
    OrderDetailDao dao;
    return dao.listViewByOrder(orderId);
}

void OrderService::normalize(Order *order)
{
    order->floor = order->floor.trimmed();
    order->orderDate = order->orderDate.trimmed();
}

ServiceResult OrderService::validate(const Order &order)
{
    if (order.bagId <= 0)
        return ServiceResult::failure(QStringLiteral("请选择书包"));

    BagDao bagDao;
    Bag bag;
    if (!bagDao.findById(order.bagId, &bag))
        return ServiceResult::failure(QStringLiteral("所选书包不存在（id=%1）").arg(order.bagId));

    if (order.floor.isEmpty())
        return ServiceResult::failure(QStringLiteral("楼层不能为空"));

    if (order.qty <= 0)
        return ServiceResult::failure(QStringLiteral("订单数量必须大于 0"));

    if (order.qty > 1000000)
        return ServiceResult::failure(QStringLiteral("订单数量看起来过大（上限 100 万）"));

    if (!isValidDate(order.orderDate))
        return ServiceResult::failure(QStringLiteral("订单日期格式应为 yyyy-MM-dd"));

    return ServiceResult::success();
}

ServiceResult OrderService::validateDetails(int bagId, const QList<OrderDetail> &details)
{
    if (details.isEmpty())
        return ServiceResult::failure(
            QStringLiteral("至少录入一条明细（哪个员工做了哪道工序、多少件）"));

    EmployeeDao employeeDao;
    ProcessDao processDao;
    BagProcessDao priceDao;

    for (int i = 0; i < details.size(); ++i) {
        const OrderDetail &d = details.at(i);
        const int line = i + 1;

        if (d.empId <= 0)
            return ServiceResult::failure(QStringLiteral("第 %1 条明细：请选择员工").arg(line));

        Employee emp;
        if (!employeeDao.findById(d.empId, &emp))
            return ServiceResult::failure(
                QStringLiteral("第 %1 条明细：员工不存在（id=%2）").arg(line).arg(d.empId));

        if (d.processId <= 0)
            return ServiceResult::failure(QStringLiteral("第 %1 条明细：请选择工序").arg(line));

        Process process;
        if (!processDao.findById(d.processId, &process))
            return ServiceResult::failure(
                QStringLiteral("第 %1 条明细：工序不存在（id=%2）").arg(line).arg(d.processId));

        double price = 0.0;
        if (!priceDao.findPrice(bagId, d.processId, &price)) {
            return ServiceResult::failure(
                QStringLiteral("第 %1 条明细：这款书包没有给工序「%2」配单价，"
                               "请先到「书包」页补齐单价。")
                    .arg(line)
                    .arg(process.name));
        }

        if (d.qty <= 0)
            return ServiceResult::failure(
                QStringLiteral("第 %1 条明细：数量必须大于 0").arg(line));

        if (!isValidDate(d.workDate))
            return ServiceResult::failure(
                QStringLiteral("第 %1 条明细：日期格式应为 yyyy-MM-dd").arg(line));
    }

    return ServiceResult::success();
}

ServiceResult OrderService::save(Order *order, QList<OrderDetail> details)
{
    if (!order)
        return ServiceResult::failure(QStringLiteral("内部错误：订单数据为空"));

    normalize(order);

    ServiceResult basic = validate(*order);
    if (!basic.ok)
        return basic;

    ServiceResult detailCheck = validateDetails(order->bagId, details);
    if (!detailCheck.ok)
        return detailCheck;

    Transaction tx;
    if (!tx.begin())
        return ServiceResult::failure(QStringLiteral("开启事务失败：%1").arg(tx.lastError()));

    OrderDao orderDao;
    const bool isNew = (order->id <= 0);

    if (isNew) {
        if (!orderDao.insert(order))
            return ServiceResult::failure(
                QStringLiteral("保存订单失败：%1").arg(orderDao.lastError()));
    } else {
        if (!orderDao.update(*order))
            return ServiceResult::failure(
                QStringLiteral("保存订单失败：%1").arg(orderDao.lastError()));
    }

    OrderDetailDao detailDao;

    // 编辑时整组替换明细：先删旧行，避免用户删掉的明细残留
    if (!isNew) {
        if (!detailDao.removeByOrder(order->id)) {
            return ServiceResult::failure(
                QStringLiteral("清理旧明细失败：%1").arg(detailDao.lastError()));
        }
    }

    for (int i = 0; i < details.size(); ++i) {
        OrderDetail d = details.at(i);
        d.orderId = order->id;
        if (!detailDao.insert(d)) {
            return ServiceResult::failure(
                QStringLiteral("写入第 %1 条明细失败：%2").arg(i + 1).arg(detailDao.lastError()));
        }
    }

    if (!tx.commit())
        return ServiceResult::failure(QStringLiteral("提交事务失败：%1").arg(tx.lastError()));

    return ServiceResult::success();
}

ServiceResult OrderService::remove(int orderId)
{
    if (orderId <= 0)
        return ServiceResult::failure(QStringLiteral("内部错误：无效的订单 id"));

    Transaction tx;
    if (!tx.begin())
        return ServiceResult::failure(QStringLiteral("开启事务失败：%1").arg(tx.lastError()));

    OrderDetailDao detailDao;
    if (!detailDao.removeByOrder(orderId))
        return ServiceResult::failure(QStringLiteral("删除明细失败：%1").arg(detailDao.lastError()));

    OrderDao orderDao;
    if (!orderDao.remove(orderId))
        return ServiceResult::failure(QStringLiteral("删除订单失败：%1").arg(orderDao.lastError()));

    if (!tx.commit())
        return ServiceResult::failure(QStringLiteral("提交事务失败：%1").arg(tx.lastError()));

    return ServiceResult::success();
}
