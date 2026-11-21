#ifndef PROCESSDAO_H
#define PROCESSDAO_H

#include "dao/models.h"

#include <QList>
#include <QString>

// 工序表的数据访问对象：只写 SQL，不做业务判断。
// 除本表的增删改查外，还提供「被谁引用」的查询，供 Service 判断能否删除。
class ProcessDao
{
public:
    QList<Process> listAll();
    bool           findById(int id, Process *out);

    bool insert(Process *out);
    bool update(const Process &p);
    bool remove(int id);

    // ---- 引用校验 ----
    // 被多少款书包配置了单价（bag_process）
    int countBagProcesses(int processId);
    // 被多少条计件明细使用（order_detail）
    int countOrderDetails(int processId);
    // 具体是哪些书包在用它（给提示信息用）
    QList<ReferenceInfo> referencingBags(int processId);

    QString lastError() const { return m_lastError; }

private:
    QString m_lastError;
};

#endif // PROCESSDAO_H
