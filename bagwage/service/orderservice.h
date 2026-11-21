#ifndef ORDERSERVICE_H
#define ORDERSERVICE_H

#include "dao/models.h"
#include "service/serviceresult.h"

#include <QList>

// 订单业务逻辑：事务写入订单主表 + 批量明细，并在写库前校验一遍引用。
//
// 关键点：
//   * 明细必须能在这款书包的单价表（bag_process）里找到单价，
//     否则等到工资核算时才会发现是 0 元，而且不知道是哪条错了；
//   * 编辑订单时先删旧明细再全量插入，和书包单价表一个思路；
//   * 整个保存在一个 Transaction 里，任何一条明细失败都整体回滚。
class OrderService
{
public:
    QList<OrderSummary> listSummaries() const;

    bool load(int orderId, Order *order, QList<OrderDetail> *details);
    QList<OrderDetailView> detailView(int orderId) const;

    // order->id == 0 表示新建；details 至少一条
    ServiceResult save(Order *order, QList<OrderDetail> details);

    ServiceResult remove(int orderId);

private:
    static void normalize(Order *order);
    static ServiceResult validate(const Order &order);
    static ServiceResult validateDetails(int bagId, const QList<OrderDetail> &details);
};

#endif // ORDERSERVICE_H
