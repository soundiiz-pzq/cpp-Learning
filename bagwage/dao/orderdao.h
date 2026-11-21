#ifndef ORDERDAO_H
#define ORDERDAO_H

#include "dao/models.h"

#include <QList>
#include <QString>

// 订单主表的数据访问对象。
// 明细与合计用 LEFT JOIN 聚合，保证「没有明细的订单」也会出现在列表里。
class OrderDao
{
public:
    QList<Order>        listAll();
    QList<OrderSummary> listSummaries();

    // 按「订单」维度分页：每页 N 个订单，不会被明细行数撑乱。
    // empFilter > 0 时只算该员工参与过的订单（同一订单多条明细只算一次）。
    // totalCount 返回符合条件的订单总数（可为 0 表示不关心）。
    QList<OrderSummary> pageSummaries(int pageIndex, int pageSize, int empFilter,
                                      int *totalCount);

    bool findById(int id, Order *out);

    bool insert(Order *out);
    bool update(const Order &o);
    bool remove(int id);

    // 分页要用的计数（阶段④）
    int countAll();
    int countByEmployee(int empId);

    QString lastError() const { return m_lastError; }

private:
    QString m_lastError;
};

#endif // ORDERDAO_H
