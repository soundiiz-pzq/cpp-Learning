#include "db/transaction.h"
#include "db/dbmanager.h"

#include <QSqlError>

Transaction::Transaction(const char *connectionName)
    : m_db(QSqlDatabase::database(
          connectionName ? QLatin1String(connectionName)
                         : QLatin1String(DatabaseManager::kMainConnection),
          false))
    , m_active(false)
{
}

Transaction::~Transaction()
{
    if (m_active)
        rollback();
}

bool Transaction::begin()
{
    if (!m_db.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开，无法开启事务");
        return false;
    }
    if (!m_db.transaction()) {
        m_lastError = m_db.lastError().text();
        return false;
    }
    m_active = true;
    return true;
}

bool Transaction::commit()
{
    if (!m_active)
        return false;

    if (!m_db.commit()) {
        m_lastError = m_db.lastError().text();
        rollback();
        return false;
    }
    m_active = false;
    return true;
}

void Transaction::rollback()
{
    if (!m_active)
        return;
    m_db.rollback();
    m_active = false;
}
