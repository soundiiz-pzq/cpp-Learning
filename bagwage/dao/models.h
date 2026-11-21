#ifndef MODELS_H
#define MODELS_H

#include <QString>

// 实体定义（纯数据，不含 SQL，不依赖 Qt Sql 模块）。
// 需求清单里的 6 个实体都在这里；骨架阶段只用到 Employee，
// 其余先留空结构体，写对应模块时按需补字段。

struct Employee
{
    int     id = 0;
    QString name;
    QString floor;
};

struct Process
{
    int     id = 0;
    QString name;
    double  price = 0.0;
};

struct Bag
{
    int     id = 0;
    QString name;
};

// 书包 × 工序 的计件单价（同一道工序，不同书包可以有不同单价）
struct BagProcess
{
    int    id = 0;
    int    bagId = 0;
    int    processId = 0;
    double price = 0.0;
};

struct Order
{
    int     id = 0;
    int     bagId = 0;
    QString floor;
    int     qty = 0;
};

struct OrderDetail
{
    int     id = 0;
    int     orderId = 0;
    int     empId = 0;
    int     processId = 0;
    int     qty = 0;
    QString workDate;   // 'yyyy-MM-dd'
};

#endif // MODELS_H
