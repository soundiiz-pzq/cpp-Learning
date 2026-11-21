#ifndef BAGDAO_H
#define BAGDAO_H

#include "dao/models.h"

#include <QList>
#include <QString>

// 书包表的数据访问对象。
// 阶段①只需要「按 id 取名」给工序的引用提示用，
// 完整的书包增删改查在阶段②补齐。
class BagDao
{
public:
    QList<Bag> listAll();
    bool       findById(int id, Bag *out);
    QString    nameOf(int id);

    QString lastError() const { return m_lastError; }

private:
    QString m_lastError;
};

#endif // BAGDAO_H
