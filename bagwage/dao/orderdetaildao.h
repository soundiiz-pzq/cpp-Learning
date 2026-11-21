#ifndef ORDERDETAILDAO_H
#define ORDERDETAILDAO_H

#include "dao/models.h"

#include <QList>

// 订单明细表：谁、做了什么工序、多少件、哪天。
class OrderDetailDao
{
public:
    QList<OrderDetail>     listByOrder(int orderId);
    QList<OrderDetailView> listViewByOrder(int orderId);

    bool insert(const OrderDetail &d);
    bool removeByOrder(int orderId);
    bool remove(int id);

    int  countByOrder(int orderId);
    int  countByEmployee(int empId);

    QString lastError() const { return m_lastError; }

private:
    QString m_lastError;
};

#endif // ORDERDETAILDAO_H
