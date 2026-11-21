#ifndef BAGDIALOG_H
#define BAGDIALOG_H

#include "dao/models.h"

#include <QDialog>
#include <QList>

class QLineEdit;
class QPushButton;
class QTableWidget;

// 书包录入/编辑对话框：书包名 + 单价表（工序 / 单价）。
// 单价表里可以增删行；保存时整组替换，业务校验和事务在 BagService。
class BagDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BagDialog(const QList<Process> &processes, QWidget *parent = 0);

    void setBag(const Bag &bag, const QList<BagProcess> &prices);
    Bag  bag() const;
    QList<BagProcess> prices() const;

private slots:
    void addPriceRow();
    void removePriceRow();
    void onAccept();

private:
    void    setRowProcess(int row, int processId);
    void    setRowPrice(int row, double price);
    int     nextUnusedProcessId() const;

    QLineEdit    *m_nameEdit;
    QTableWidget *m_priceTable;
    QPushButton  *m_addRowButton;
    QPushButton  *m_removeRowButton;
    QList<Process> m_processes;
    Bag          m_bag;   // 保留 id，修改时用
};

#endif // BAGDIALOG_H
