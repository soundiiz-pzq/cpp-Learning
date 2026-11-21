#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "dao/models.h"

#include <QMainWindow>

class QLabel;
class QTableWidget;
class QTabWidget;
class QPushButton;

// 主窗口：承载各功能页。
// 骨架阶段只有「员工」页是完整的 DAO→Service→UI 示范，其余页是占位。
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);

private slots:
    void refreshEmployees();
    void addEmployee();
    void editEmployee();
    void removeEmployee();

private:
    QWidget *createEmployeePage();
    QWidget *makePlaceholderPage(const QString &title,
                                 const QString &summary,
                                 const QStringList &todo);

    int     currentEmployeeId() const;
    int     currentEmployeeRow() const;
    void    showServiceFailure(const QString &message);

    QTabWidget   *m_tabs;
    QLabel       *m_dbInfo;
    QTableWidget *m_employeeTable;
    QPushButton  *m_editButton;
    QPushButton  *m_removeButton;
};

#endif // MAINWINDOW_H
