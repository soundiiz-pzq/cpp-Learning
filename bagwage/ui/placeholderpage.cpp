#include "ui/placeholderpage.h"

#include <QLabel>
#include <QVBoxLayout>

PlaceholderPage::PlaceholderPage(const QString &title,
                                 const QString &summary,
                                 const QStringList &todo,
                                 QWidget *parent)
    : QWidget(parent)
{
    QLabel *titleLabel = new QLabel(title, this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    QLabel *summaryLabel = new QLabel(summary, this);
    summaryLabel->setWordWrap(true);
    summaryLabel->setTextFormat(Qt::RichText);

    QString html = QStringLiteral("<p><b>待办清单</b></p><ul>");
    for (int i = 0; i < todo.size(); ++i)
        html += QStringLiteral("<li>%1</li>").arg(todo.at(i));
    html += QStringLiteral("</ul>");

    QLabel *todoLabel = new QLabel(html, this);
    todoLabel->setWordWrap(true);
    todoLabel->setTextFormat(Qt::RichText);
    todoLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(titleLabel);
    layout->addWidget(summaryLabel);
    layout->addWidget(todoLabel);
    layout->addStretch(1);
}
