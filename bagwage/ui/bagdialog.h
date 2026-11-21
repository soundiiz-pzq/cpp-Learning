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

protected:
    // 表格单元格被 QComboBox / QDoubleSpinBox 占满，点击落在控件上，
    // 表格自己的 currentRow 不会跟着变。这里在控件收到点击/焦点时同步选中行，
    // 否则「删除选中行」永远删的是最后一次 selectRow 的那行。
    bool eventFilter(QObject *watched, QEvent *event);

private:
    void    setRowProcess(int row, int processId);
    void    setRowPrice(int row, double price);
    int     nextUnusedProcessId() const;
    void    highlightRow(int row);
    int     rowOfWidget(const QObject *widget) const;

    QLineEdit    *m_nameEdit;
    QTableWidget *m_priceTable;
    QPushButton  *m_addRowButton;
    QPushButton  *m_removeRowButton;
    QList<Process> m_processes;
    Bag          m_bag;   // 保留 id，修改时用
};

#endif // BAGDIALOG_H
