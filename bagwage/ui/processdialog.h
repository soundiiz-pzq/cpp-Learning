#ifndef PROCESSDIALOG_H
#define PROCESSDIALOG_H

#include "dao/models.h"

#include <QDialog>

class QDoubleSpinBox;
class QLineEdit;

// 工序录入/编辑对话框。
// UI 只收集输入，业务校验（含单价上限）在 ProcessService。
class ProcessDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProcessDialog(QWidget *parent = 0);

    void    setProcess(const Process &p);
    Process process() const;

private slots:
    void onAccept();

private:
    QLineEdit      *m_nameEdit;
    QDoubleSpinBox *m_priceSpin;
    Process         m_process;   // 保留 id，修改时用
};

#endif // PROCESSDIALOG_H
