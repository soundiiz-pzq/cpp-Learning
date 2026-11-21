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
