# bagwage — 书包生产记账系统

Qt 5.9.1 / MinGW 5.3（32 位）/ qmake / SQLite。

> **当前进度：阶段①（工序模块）已完成。**
> 已完成：数据库层、公共件（ServiceResult / Transaction / 临时库自检）、员工模块、工序模块。
> 未完成：书包+单价表、订单+明细、订单分页、工资核算、图表、打包。

---

## 一、目录结构

```
bagwage/
├── bagwage.pro            qmake 工程（QT += sql charts, CONFIG += c++11）
├── main.cpp               入口：--selftest 走自检，否则开库建表 → 主窗口
├── selftest.cpp/.h        无界面自检（跑在临时库上，不碰正式库）
├── build.bat              一键编译 / 运行 / 清理
├── dao/                   一张表一个 DAO，只写 SQL
│   ├── models.h           实体结构体
│   ├── employeedao.*      员工
│   ├── processdao.*       工序（含 bag_process / order_detail 引用查询）
│   └── bagdao.*           书包（阶段②补全增删改查）
├── db/
│   ├── dbmanager.*        连接 + 建表 + 索引 + 轻量迁移
│   └── transaction.*      RAII 事务（析构自动回滚）
├── service/               业务：校验、引用检查、事务编排
│   ├── serviceresult.h    所有 Service 共用的返回类型
│   ├── employeeservice.*
│   └── processservice.*
└── ui/                    界面：只收集输入、弹窗、刷新表格
    ├── mainwindow.*
    ├── employeedialog.*
    ├── processdialog.*
    └── placeholderpage.*
```

分层规矩：**ui 不碰 dao、不开事务**；SQL 只在 dao；事务和校验只在 service。

---

## 二、构建与运行

### Qt Creator（推荐）

打开 `bagwage.pro`，Kit 选 `Desktop Qt 5.9.1 MinGW 32bit`，`Ctrl+B` 编译、`Ctrl+R` 运行。

### 命令行

```powershell
$env:Path = "D:\Qt5.9.1\5.9.1\mingw53_32\bin;D:\Qt5.9.1\Tools\mingw530_32\bin;" + $env:Path
cd C:\Users\PYH\Desktop\cpp\bagwage
qmake bagwage.pro
mingw32-make -j4
.\bagwage.exe
```

或者直接用 `build.bat`（编译）、`build.bat run`（运行）、`build.bat test`（自检）、`build.bat clean`（清理）。

> `build.bat` 刻意只用 ASCII 和 CRLF：cmd 按系统 OEM 代码页（936）解析批处理文件，
> 中文经错误解码后可能变成 `&` 或 `|`，把一行拆成两条命令（曾导致无限自我调用）。
> 修改这个文件时请不要加中文。

### 自检（改完代码先跑这个）

```powershell
.\bagwage.exe --selftest
```

**自检跑在临时库上**（`%TEMP%\bagwage_selftest_<pid>.db`），跑完自动删除，绝不会往正式 `bagwage.db` 里插数据。
当前阶段①共 24 项检查，期望最后一行 `===== 结果：PASS 24 / FAIL 0 =====`，退出码 0。

想手工指定库文件路径（比如拿一份测试数据练手）：

```powershell
$env:BAGWAGE_DB = "D:\test\demo.db"
.\bagwage.exe
```

---

## 三、⚠️ 三个必须知道的坑

### 1. PATH 里的 MSYS2 会抢 Qt 的 DLL

本机 `PATH` 里有 `C:\msys64\mingw64\bin`，里面有 **Qt 6.12** 的 DLL。直接运行 `bagwage.exe` 可能加载到 Qt 6 的 DLL，报 `0xC000007B` 闪退。

**解决**：运行前把 Qt 5.9.1 的 bin 放到 PATH 最前面（上面的命令和 `build.bat` 都这么做了），或用 Qt Creator 运行。

### 2. `.pro` 必须是 UTF-8 **无 BOM**

qmake 读到带 BOM 的 `.pro` 会直接报 `Unexpected UTF-8 BOM`。
PowerShell 的 `Set-Content -Encoding utf8` 在 5.1 会加 BOM，改 `.pro` 请用编辑器另存为「UTF-8 无 BOM」。

### 3. 32 位 + C++11

- 生成 32 位程序，不能链接 64 位库；
- g++ 5.3 只到 C++14，**代码按 C++11 写**，别用 `std::optional`、`std::filesystem`、结构化绑定。

---

## 四、已完成的功能

| 功能 | 状态 | 位置 |
|---|---|---|
| 6 张表 + 6 个索引，事务内建表 | ✅ | `db/dbmanager.cpp` |
| 老库自动补列（`orders.order_date`） | ✅ 实测：老库 4 列 → 自动变 5 列 | `db/dbmanager.cpp` |
| 外键约束 | ✅ 每个连接单独 `PRAGMA foreign_keys = ON` | `db/dbmanager.cpp` |
| RAII 事务（忘写 rollback 自动回滚） | ✅ 自检里验证了回滚与提交 | `db/transaction.*` |
| 员工 增/改/删/查 + 界面 | ✅ 删除时校验 `order_detail` 引用 | `service/employeeservice.cpp` |
| 工序 增/改/删/查 + 界面 | ✅ **两处**引用校验 | `service/processservice.cpp` |
| 自检跑临时库 | ✅ 24 项 | `selftest.cpp` |
| 书包 / 订单 / 工资 / 图表 | ⬜ 占位页 | `ui/placeholderpage.cpp` |

**工序删除的引用校验（阶段①重点）**：

- 查 `bag_process`（哪些书包给它配了单价）——会列出书包名
- 查 `order_detail`（多少条计件明细用了它）
- 任意一处有引用就拒绝，提示里写清楚「被 2 款书包配了单价（书包A、书包B），且被 3 条计件明细使用」

只查一处就会漏拦，或者只看到数据库的英文报错。

---

## 五、下一步（阶段②：书包 + 单价表）

1. `dao/bagdao.*` 补全 `insert/update/remove/countReferences`。
2. `dao/bagprocessdao.*` 新建：按书包查单价列表、批量替换单价。
3. `service/bagservice.*`：`save(bag, 单价列表)` 用 `Transaction` 包住：
   - 新增则 `INSERT bag` 拿 `lastInsertId()`，编辑则 `UPDATE`
   - 单价**先全删该书包的行再全量插入**（避免用户删掉某道工序的单价后旧行残留继续算工资）
   - 任何一步失败整体回滚
4. `ui/bagdialog.*`：书包名 + 单价表格（工序 / 单价两列，可增删行）。
5. 主窗口把「书包」占位页换成真页面，`.pro` 加新文件，**重新 qmake**。
6. `selftest.cpp` 加用例：同一书包两道工序不同价、改名后单价不丢、删掉一道单价保存后确实没了。
7. 跑 `--selftest` 全绿 → 提交 `阶段②：书包与单价表`。

> 提示：Qt 5.9 自带的 SQLite 不支持 `ON CONFLICT DO UPDATE`（要 3.24+），用 `INSERT OR REPLACE` 或「先删后插」。
