#ifndef BAGDAO_H
#define BAGDAO_H

#include "dao/models.h"

#include <QList>
#include <QString>

// 书包表的数据访问对象。
class BagDao
{
public:
    QList<Bag> listAll();

    // 列表页用：书包 + 已配置单价的工序数（一条 SQL 聚合，避免 N+1 查询）
    QList<BagSummary> listAllWithPriceCount();

    bool findById(int id, Bag *out);

    bool insert(Bag *out);
    bool update(const Bag &b);
    bool remove(int id);

    // ---- 引用校验 ----
    int countOrders(int bagId);
    int countBagProcesses(int bagId);

    QString nameOf(int id);
    QString lastError() const { return m_lastError; }

private:
    QString m_lastError;
};

#endif // BAGDAO_H
