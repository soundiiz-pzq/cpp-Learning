#include "db/dbmanager.h"
#include "selftest.h"
#include "ui/mainwindow.h"

#include <QApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QStringList>
#include <QTranslator>

#include <cstdio>

// Qt 自带的 qt_zh_CN.qm 只翻译了 QMessageBox 自己那部分文案，
// 标准按钮（Yes/No/Cancel…）走的是 QPlatformTheme 这个上下文，而它没有中文。
// 结果就是确认框里出现英文 Yes/No。这里补一层最小翻译，只覆盖标准按钮。
class StandardButtonTranslator : public QTranslator
{
public:
    QString translate(const char *context, const char *sourceText,
                      const char *disambiguation = 0, int n = -1) const override
    {
        Q_UNUSED(disambiguation);
        Q_UNUSED(n);
        if (qstrcmp(context, "QPlatformTheme") != 0)
            return QString();

        const QString text = QString::fromUtf8(sourceText);
        if (text == QLatin1String("Yes") || text == QLatin1String("&Yes"))
            return QStringLiteral("是");
        if (text == QLatin1String("No") || text == QLatin1String("&No"))
            return QStringLiteral("否");
        if (text == QLatin1String("OK"))
            return QStringLiteral("确定");
        if (text == QLatin1String("Cancel"))
            return QStringLiteral("取消");
        if (text == QLatin1String("Close"))
            return QStringLiteral("关闭");
        return QString();
    }

    bool isEmpty() const override { return false; }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("bagwage"));
    app.setOrganizationName(QStringLiteral("bagwage"));

    // 1) Qt 自带翻译：QMessageBox 自己的文案（如「确定」「取消」）
    //    先找程序目录下的 translations（打包后就是它），再退回 Qt 安装目录（开发机）
    QTranslator qtTranslator;
    const QStringList translationDirs = QStringList()
        << QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("translations"))
        << QLibraryInfo::location(QLibraryInfo::TranslationsPath);
    for (int i = 0; i < translationDirs.size(); ++i) {
        if (qtTranslator.load(QLocale(), QStringLiteral("qt"), QStringLiteral("_"),
                              translationDirs.at(i))) {
            app.installTranslator(&qtTranslator);
            break;
        }
    }

    // 2) 补 QPlatformTheme 的标准按钮文案（后安装的优先级更高）
    StandardButtonTranslator buttonTranslator;
    app.installTranslator(&buttonTranslator);

    // 自检走临时库，绝不碰正式的 bagwage.db。
    // 必须在打开主库之前处理，否则连接会先建到正式库上。
    if (app.arguments().contains(QStringLiteral("--selftest")))
        return runSelfTest();

    QString error;
    if (!DatabaseManager::open(&error)) {
        fprintf(stderr, "open db failed: %s\n", qPrintable(error));
        return 1;
    }
    if (!DatabaseManager::ensureSchema(&error)) {
        fprintf(stderr, "create schema failed: %s\n", qPrintable(error));
        return 1;
    }

    MainWindow window;
    window.show();

    const int code = app.exec();
    // 主连接在退出时关闭；必须在所有 QSqlQuery 析构之后
    DatabaseManager::close();
    return code;
}
