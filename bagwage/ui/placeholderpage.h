#ifndef PLACEHOLDERPAGE_H
#define PLACEHOLDERPAGE_H

#include <QWidget>
#include <QStringList>

// 骨架期的占位页：写清楚这一页将来要做什么、按需求文档该怎么做，
// 免得看着空白页不知道从哪下手。
// 实现某个模块时，把它换成真正的 Widget 即可。
class PlaceholderPage : public QWidget
{
    Q_OBJECT

public:
    PlaceholderPage(const QString &title,
                    const QString &summary,
                    const QStringList &todo,
                    QWidget *parent = 0);
};

#endif // PLACEHOLDERPAGE_H
