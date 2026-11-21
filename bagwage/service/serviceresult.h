#ifndef SERVICERESULT_H
#define SERVICERESULT_H

#include <QString>

// Service 层的统一返回结果：成功/失败 + 给用户看的中文原因。
// 所有 Service 共用这一个类型，避免互相 include 对方的头文件。
struct ServiceResult
{
    bool    ok;
    QString message;

    ServiceResult() : ok(false) {}

    static ServiceResult success()
    {
        ServiceResult r;
        r.ok = true;
        return r;
    }
    static ServiceResult failure(const QString &msg)
    {
        ServiceResult r;
        r.ok = false;
        r.message = msg;
        return r;
    }
};

#endif // SERVICERESULT_H
