#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "dao/models.h"

#include <QHash>
#include <QList>
#include <QMainWindow>

class QComboBox;
class QLabel;
class QTableWidget;
class QTabWidget;
class QPushButton;

// 主窗口：承载各功能页。
// 已完成：员工、工序、书包（含单价表）。
// 其余页是占位页，实现时换成真页面即可。
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);

private slots:
    // 员工
    void refreshEmployees();
    void addEmployee();
    void editEmployee();
    void removeEmployee();

    // 工序
    void refreshProcesses();
    void addProcess();
    void editProcess();
    void removeProcess();

    // 书包
    void refreshBags();
    void addBag();
    void editBag();
    void removeBag();

    // 订单
    void refreshOrders();
    void addOrder();
    void editOrder();
    void removeOrder();
    void onOrderFilterChanged();
    void goPrevOrderPage();
    void goNextOrderPage();

private:
    enum { kOrderPageSize = 10 };

    QWidget *createEmployeePage();
    QWidget *createProcessPage();
    QWidget *createBagPage();
    QWidget *createOrderPage();
    QWidget *makePlaceholderPage(const QString &title,
                                 const QString &summary,
                                 const QStringList &todo);

    bool loadBagPriceMap(QList<Bag> *bags, QHash<int, QList<BagProcess> > *pricesByBag);
    void populateOrderEmployeeFilter();
    void updateOrderPageControls();

    int  currentEmployeeId() const;
    int  currentEmployeeRow() const;
    int  currentProcessId() const;
    int  currentProcessRow() const;
    int  currentBagId() const;
    int  currentBagRow() const;
    int  currentOrderId() const;
    int  currentOrderRow() const;
    void showServiceFailure(const QString &message);

    QTabWidget *m_tabs;
    QLabel     *m_dbInfo;

    QTableWidget *m_employeeTable;
    QPushButton  *m_editButton;
    QPushButton  *m_removeButton;

    QTableWidget *m_processTable;
    QPushButton  *m_editProcessButton;
    QPushButton  *m_removeProcessButton;

    QTableWidget *m_bagTable;
    QPushButton  *m_editBagButton;
    QPushButton  *m_removeBagButton;

    QTableWidget *m_orderTable;
    QPushButton  *m_editOrderButton;
    QPushButton  *m_removeOrderButton;
    QComboBox    *m_orderEmpFilterCombo;
    QLabel       *m_orderPageLabel;
    QPushButton  *m_prevOrderPageButton;
    QPushButton  *m_nextOrderPageButton;

    int m_orderPage;        // 0 基
    int m_orderEmpFilter;   // 0 = 全部员工
    int m_orderTotal;
};

#endif // MAINWINDOW_H
