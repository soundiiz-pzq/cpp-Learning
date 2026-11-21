#include "db/dbmanager.h"
#include "selftest.h"
#include "ui/mainwindow.h"

#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QStringList>
#include <QTranslator>

#include <cstdio>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("bagwage"));
    app.setOrganizationName(QStringLiteral("bagwage"));

    // 加载 Qt 自带的中文翻译，让 QMessageBox 的 Yes/No/OK 显示成「是/否/确定」。
    // 翻译文件不在时静默跳过，不影响运行（打包时别加 --no-translations）。
    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale(), QStringLiteral("qt"), QStringLiteral("_"),
                          QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    }

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
