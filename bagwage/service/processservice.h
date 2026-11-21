#ifndef PROCESSSERVICE_H
#define PROCESSSERVICE_H

#include "dao/models.h"
#include "service/serviceresult.h"

#include <QList>

// 工序业务逻辑：输入校验 + 两处引用校验。
//
// 注意和员工的区别：工序被**两处**引用
//   1. bag_process（某款书包给这道工序定了单价）
//   2. order_detail（某条计件明细用了这道工序）
// 只查其中一处，删除时就会漏拦或者只看到数据库的英文报错。
class ProcessService
{
public:
    QList<Process> listAll() const;

    // 新增（p.id == 0）或修改（p.id > 0）。成功时把新 id 写回 p->id。
    ServiceResult save(Process *p);

    // 被书包单价表或计件明细引用时拒绝删除，并说明被谁引用
    ServiceResult remove(int processId);

private:
    static void normalize(Process *p);
};

#endif // PROCESSSERVICE_H
