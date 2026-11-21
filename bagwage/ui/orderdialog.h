#ifndef ORDERDIALOG_H
#define ORDERDIALOG_H

#include "dao/models.h"

#include <QDialog>
#include <QHash>
#include <QList>

class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;

// 订单录入/编辑对话框：订单头 + 批量明细（员工 / 工序 / 数量 / 日期）。
// 工序下拉里会标出「这款书包」对应的单价，没配价的标「未定价」，保存时 Service 会拦。
class OrderDialog : public QDialog
{
    Q_OBJECT

public:
    OrderDialog(const QList<Bag> &bags,
                const QList<Employee> &employees,
                const QList<Process> &processes,
                const QHash<int, QList<BagProcess> > &pricesByBag,
                QWidget *parent = 0);

    void setOrder(const Order &order, const QList<OrderDetail> &details);

    Order              order() const;
    QList<OrderDetail> details() const;

private slots:
    void addDetailRow();
    void removeDetailRow();
    void onBagChanged();
    void onAccept();

protected:
    // 明细表格的单元格被控件占满，点击不会更新表格 currentRow，
    // 这里在控件获得焦点/被点击时同步选中行，保证「删除选中行」删的是点的那行。
    bool eventFilter(QObject *watched, QEvent *event);

private:
    void addRowWithValues(int empId, int processId, int qty, const QString &date);
    void refreshProcessLabels();
    void updatePriceHint();
    void highlightRow(int row);
    int  rowOfWidget(const QObject *widget) const;
    double priceFor(int processId) const;

    QComboBox    *m_bagCombo;
    QComboBox    *m_floorCombo;
    QSpinBox     *m_qtySpin;
    QDateEdit    *m_dateEdit;
    QLabel       *m_priceHint;
    QTableWidget *m_detailTable;
    QPushButton  *m_addRowButton;
    QPushButton  *m_removeRowButton;

    QList<Bag>      m_bags;
    QList<Employee> m_employees;
    QList<Process>  m_processes;
    QHash<int, QList<BagProcess> > m_pricesByBag;

    Order m_order;   // 保留 id，编辑时用
};

#endif // ORDERDIALOG_H
