#ifndef BAGSERVICE_H
#define BAGSERVICE_H

#include "dao/models.h"
#include "service/serviceresult.h"

#include <QList>

// 书包 + 单价表的业务逻辑。
//
// 「编辑书包」和「维护单价」是一件事：保存时一个事务里
//   1. 新增则 INSERT bag 拿 lastInsertId()，编辑则 UPDATE bag
//   2. 删掉该书包现有的全部 bag_process 行
//   3. 逐条插入本次提交的单价
// 这样用户删掉某道工序的单价后，旧行不会残留在表里继续参与工资计算。
class BagService
{
public:
    QList<BagSummary> listAll() const;

    // 读一个书包 + 它的全部单价（编辑时用）
    bool load(int bagId, Bag *bag, QList<BagProcess> *prices);

    // bag->id == 0 表示新增；prices 可以为空（先建书包、以后再定价）
    ServiceResult save(Bag *bag, const QList<BagProcess> &prices);

    // 只写 bag_process，用在批量维护单价、不改书包名的场景
    ServiceResult savePrices(int bagId, const QList<BagProcess> &prices);

    // 被订单引用时拒绝删除；没有订单引用时连带删掉它的单价行
    ServiceResult remove(int bagId);

private:
    static void normalize(Bag *bag);
    static ServiceResult validatePrices(const QList<BagProcess> &prices);
    static ServiceResult writePrices(int bagId, const QList<BagProcess> &prices);
};

#endif // BAGSERVICE_H
