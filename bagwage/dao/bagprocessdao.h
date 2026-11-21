#ifndef BAGPROCESSDAO_H
#define BAGPROCESSDAO_H

#include "dao/models.h"

#include <QList>

// bag_process 表：书包 × 工序 的计件单价。
// 这张表没有独立界面，永远跟着「编辑书包」走，所以只需要按书包整体读写。
class BagProcessDao
{
public:
    QList<BagProcess> listByBag(int bagId);
    int               countByBag(int bagId);

    // 整组替换：先删该书包所有单价行，再逐条插入。
    // 调用方负责包事务（见 BagService::save）。
    bool removeByBag(int bagId);
    bool insert(const BagProcess &bp);

    // 查某书包某工序的单价；没有配置时返回 false（金额不能按 0 算）
    bool findPrice(int bagId, int processId, double *price);

    QString lastError() const { return m_lastError; }

private:
    QString m_lastError;
};

#endif // BAGPROCESSDAO_H
