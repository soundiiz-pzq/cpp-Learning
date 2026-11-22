#ifndef CHARTWORKER_H
#define CHARTWORKER_H

#include "dao/models.h"

#include <QThread>

// 月度产量聚合：在**子线程**里开自己的 SQLite 连接算，算完用信号回主线程。
//
// 注意：QSqlDatabase 有线程亲和性，子线程绝不能借用主线程的 "main" 连接。
// compute() 与 run() 走同一份逻辑，测试里可以直接同步调用 compute()。
class ChartWorker : public QThread
{
    Q_OBJECT

public:
    ChartWorker(const QString &month, int requestId, QObject *parent = 0);

    // 同步执行（在调用者线程里自建连接），便于自检
    ChartData compute() const;

signals:
    void dataReady(const ChartData &data);

protected:
    void run();

private:
    QString m_month;
    int     m_requestId;
};

#endif // CHARTWORKER_H
