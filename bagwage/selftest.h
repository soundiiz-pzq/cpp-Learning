#ifndef SELFTEST_H
#define SELFTEST_H

// 无界面自检：在**临时数据库**上跑一遍关键流程，跑完删掉。
// 不会碰正式的 bagwage.db。
// 返回 0 表示全部通过。
int runSelfTest();

#endif // SELFTEST_H
