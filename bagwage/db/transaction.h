#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <QSqlDatabase>
#include <QString>
#include <QtGlobal>

// RAII 事务：构造时 begin，析构时若还没 commit 就自动 rollback。
//
//   {
//       Transaction tx;
//       if (!tx.begin()) { ... }
//       ... 多条 DAO 写操作 ...
//       if (!ok) return;          // 这里直接 return 也会自动回滚
//       tx.commit();
//   }
//
// 这样「漏写 rollback」这个坑就自动消失了。
// 注意：SQLite 不支持真正的嵌套事务，别在一个 Transaction 里再开一个。
class Transaction
{
public:
    // connectionName 默认用主连接；子线程可以传自己的连接名
    explicit Transaction(const char *connectionName = 0);
    ~Transaction();

    bool begin();
    bool commit();
    void rollback();

    bool        isActive() const { return m_active; }
    QString     lastError() const { return m_lastError; }

private:
    Q_DISABLE_COPY(Transaction)

    QSqlDatabase m_db;
    bool         m_active;
    QString      m_lastError;
};

#endif // TRANSACTION_H
