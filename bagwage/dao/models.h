#ifndef MODELS_H
#define MODELS_H

#include <QString>

// 实体定义（纯数据，不含 SQL，不依赖 Qt Sql 模块）。
// 需求清单里的 6 个实体都在这里。

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
    QString orderDate;   // 'yyyy-MM-dd'，留空表示未填
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

// 引用校验时给用户看的「被谁引用」
struct ReferenceInfo
{
    int     id = 0;
    QString name;
};

// 书包列表页的一行：书包 + 已配置单价的工序数
struct BagSummary
{
    int     id = 0;
    QString name;
    int     priceCount = 0;
};

// 订单列表页的一行：订单 + 书包名 + 明细数 + 合计金额
struct OrderSummary
{
    int     id = 0;
    QString orderDate;
    int     bagId = 0;
    QString bagName;
    QString floor;
    int     qty = 0;
    int     detailCount = 0;
    double  totalAmount = 0.0;
};

// 明细的展示形态：带上员工名、工序名、该书包的单价和小计
struct OrderDetailView
{
    int     id = 0;
    int     orderId = 0;
    int     empId = 0;
    int     processId = 0;
    int     qty = 0;
    QString workDate;
    QString empName;
    QString processName;
    double  unitPrice = 0.0;
    double  amount = 0.0;
};

#endif // MODELS_H
