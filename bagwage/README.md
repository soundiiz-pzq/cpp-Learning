# bagwage — 书包生产记账系统

Qt 5.9.1 / MinGW 5.3（32 位）/ qmake / SQLite。

> **当前进度：阶段①（工序）、②（书包+单价表）、③（订单+明细）、④（订单分页）、⑤（工资核算）、⑥（图表）已完成。**
> 已完成：数据库层、公共件、员工、工序、书包+单价表、订单+明细、分页筛选、月度工资、月度图表。
> 未完成：打包。

---

## 一、目录结构

```
bagwage/
├── bagwage.pro            qmake 工程（QT += sql charts, CONFIG += c++11）
├── main.cpp               入口：加载中文翻译 → --selftest 或开库建表 → 主窗口
├── selftest.cpp/.h        无界面自检（跑在临时库上，不碰正式库）
├── build.bat              一键编译 / 运行 / 自检 / 清理
├── dao/                   一张表一个 DAO，只写 SQL
│   ├── models.h           实体结构体
│   ├── employeedao.*      员工
│   ├── processdao.*       工序（含 bag_process / order_detail 引用查询）
│   ├── bagdao.*           书包（含「已配单价工序数」聚合）
│   ├── bagprocessdao.*    书包×工序单价（按书包整组替换、按书包+工序查价）
│   ├── orderdao.*         订单（列表带明细数/合计金额聚合）
│   ├── orderdetaildao.*   订单明细（含员工名/工序名/单价/小计的视图查询）
│   └── wagedao.*          工资核算取数（按月拉明细，带书包单价与是否定价）
├── db/
│   ├── dbmanager.*        连接 + 建表 + 索引 + 轻量迁移
│   └── transaction.*      RAII 事务（析构自动回滚）
├── service/               业务：校验、引用检查、事务编排
│   ├── serviceresult.h    所有 Service 共用的返回类型
│   ├── employeeservice.*
│   ├── processservice.*
│   ├── bagservice.*       书包 + 单价表（同一个事务）
│   ├── orderservice.*     订单 + 批量明细（单价必须存在，否则拒绝）
│   ├── wageservice.*      月度工资（按分累加、未定价单独暴露）
│   ├── monthcatalog.*     月份下拉候选（有记录的 ∪ 最近 12 个月）
│   └── chartworker.*      图表聚合子线程（自建 SQLite 连接）
└── ui/                    界面：只收集输入、弹窗、刷新表格
    ├── mainwindow.*
    ├── employeedialog.*
    ├── processdialog.*
    ├── bagdialog.*        书包名 + 单价表
    ├── orderdialog.*      订单头 + 明细表格
    ├── chartpage.*        柱状图 + 饼图
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

**自检跑在临时库上**，跑完自动删除，绝不会往正式 `bagwage.db` 里插数据。
默认用 `%TEMP%\bagwage_selftest_<pid>.db`；**如果临时目录里 SQLite 打不开库**
（本机实测会报 `out of memory Error opening database`），会自动回退到程序目录建库、跑完删掉。
当前共 139 项检查，期望最后一行 `===== 结果：PASS 139 / FAIL 0 =====`，退出码 0。

`build.bat test` 会**先 qmake + mingw32-make，再跑自检**，不需要先手工 clean/build。

想手工指定库文件路径（比如拿一份测试数据练手）：

```powershell
$env:BAGWAGE_DB = "D:\test\demo.db"
.\bagwage.exe
```

---

## 三、⚠️ 几个必须知道的坑

### 1. PATH 里的 MSYS2 会抢 Qt 的 DLL

本机 `PATH` 里有 `C:\msys64\mingw64\bin`，里面有 **Qt 6.12** 的 DLL。直接运行 `bagwage.exe` 可能加载到 Qt 6 的 DLL，报 `0xC000007B` 闪退。

**解决**：运行前把 Qt 5.9.1 的 bin 放到 PATH 最前面（上面的命令和 `build.bat` 都这么做了），或用 Qt Creator 运行。

### 2. `.pro` 必须是 UTF-8 **无 BOM**

qmake 读到带 BOM 的 `.pro` 会直接报 `Unexpected UTF-8 BOM`。
PowerShell 的 `Set-Content -Encoding utf8` 在 5.1 会加 BOM，改 `.pro` 请用编辑器另存为「UTF-8 无 BOM」。

### 3. 32 位 + C++11

- 生成 32 位程序，不能链接 64 位库；
- g++ 5.3 只到 C++14，**代码按 C++11 写**，别用 `std::optional`、`std::filesystem`、结构化绑定。

### 4. QMessageBox 按钮中文要自己补一层

Qt 自带的 `qt_zh_CN.qm` **不含** `QPlatformTheme` 上下文的标准按钮文案，
只加载它的话确认框里还是英文 Yes/No。
`main.cpp` 里额外装了一个 `StandardButtonTranslator` 覆盖「是/否/确定/取消/关闭」。
**打包时别加 `--no-translations`**，否则连 `QMessageBox` 自己的中文也会丢。

### 5. 界面上「删除选中行」为什么不用表格的 currentRow

单价表 / 明细表的每个单元格都被 `QComboBox`、`QDoubleSpinBox` 占满，
点击落在控件上，`QTableWidget::currentRow()` 不会跟着变——曾导致「删除选中行」永远删最后一行。
现在的做法是给每行控件装 `eventFilter`，控件获得焦点/被点击时同步选中行。
**以后再加类似的「带控件表格」时记得照抄这个模式**。

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
| 书包 增/改/删/查 + 界面 | ✅ 删除时校验订单引用 | `service/bagservice.cpp` |
| 书包 × 工序 单价表 | ✅ 事务内整组替换，删掉的单价不会残留 | `service/bagservice.cpp` |
| 订单 + 批量明细 | ✅ 事务写入；明细单价必须已配置 | `service/orderservice.cpp` |
| 订单列表（含合计金额） | ✅ 合计取 `bag_process` 单价 | `dao/orderdao.cpp` |
| **订单维度分页 + 员工筛选** | ✅ 一页 10 个订单，换页/筛选不重复不漏 | `dao/orderdao.cpp` |
| **月度工资核算** | ✅ 按分累加；缺单价单独列出、不计入总额 | `service/wageservice.cpp` |
| **月份下拉 + 空月份提示** | ✅ 有记录的月份 ∪ 最近 12 个月；空月份不显示「合计 0.00」 | `ui/mainwindow.cpp` |
| **月度统计图表** | ✅ 子线程聚合 + 柱状图/饼图，过期结果丢弃 | `service/chartworker.cpp`、`ui/chartpage.cpp` |
| 自检跑临时库 | ✅ 139 项（含界面与线程回归） | `selftest.cpp` |

**工序删除的引用校验（阶段①重点）**：

- 查 `bag_process`（哪些书包给它配了单价）——会列出书包名
- 查 `order_detail`（多少条计件明细用了它）
- 任意一处有引用就拒绝，提示里写清楚「被 2 款书包配了单价（书包A、书包B），且被 3 条计件明细使用」

只查一处就会漏拦，或者只看到数据库的英文报错。

**书包单价表的三个关键点（阶段②重点）**：

1. **整组替换**：保存时先 `DELETE FROM bag_process WHERE bag_id = ?`，再逐条插入。
   如果只用 `INSERT OR REPLACE`，用户删掉某道工序的单价后旧行会留在表里，继续参与工资计算。
2. **一个事务**：书包主表和单价子表在同一个 `Transaction` 里，子表写失败时书包本身也不会留下
   （自检里用触发器强制失败验证过）。
3. **删书包**：有订单引用就拦；没有引用则连带删除它的单价行（单价是书包自己的子数据）。

**订单分页的关键点（阶段④）**：

1. **分页粒度是订单，不是明细**：先 `SELECT DISTINCT order_id ... ORDER BY order_id DESC LIMIT ? OFFSET ?`
   拿到当页的 10 个订单 id，再用 `WHERE o.id IN (...)` 回查完整行。
   直接对 `order_detail` 分页会把一页变成 10 条明细。
2. **总数用 `COUNT(DISTINCT order_id)`**，按员工筛时同一订单多条明细只算一次。
3. **页码越界自动回退**：删掉最后一页的唯一订单、或筛选后页数变少时，
   界面会自动退到有效页，不会停在空页。
4. 分页状态（页码、每页 10、筛选员工）只存在 UI；Service 只返回 `{列表, 总数}`。

**月份下拉与空月份（阶段⑤补丁）**：

- 下拉 = 库里有记录的月份 ∪ 最近 12 个月（含本月），时间倒序。
  只列有数据的月份，「本月无计件记录」就永远看不到；
- 首次打开优先落在「最近一个真有数据的月份」，一个都没有才落到本月；
- 汇总为空时**清空合计标签**，不显示「合计 0.00 元 / 0 人」——
  「没有记录」和「工资是 0」是两件事。

**图表的四个关键点（阶段⑥）**：

1. **子线程自建连接**：`ChartWorker` 在 `compute()` 里 `addDatabase("chart_conn_N")`，
   先让 `QSqlQuery`、`QSqlDatabase` 依次离开作用域，再 `removeDatabase`，绝不借用主线程的 `"main"`。
2. **过期结果直接丢弃**：每次请求自增 `requestId`，`dataReady` 回来先比对，
   连点月份时只画最后一次。
3. **空数据不画图**：两张图都为空时显示「该月没有计件记录」，不去碰空容器。
4. **QChartView 的尺寸**：它继承 QGraphicsView，`sizeHint` 来自图表场景，会把页面撑得比窗口还宽/高。
   处理办法是设 `QSizePolicy::Ignored` + 显式的最大宽高，两张图上下堆叠。

> 注意：图表按**产量**统计，未配置单价的明细也算（活儿干了就是干了）；
> 工资页才是按「已定价」口径算钱，两者不一致是预期的。

---

## 五、下一步（阶段⑦：打包）

1. Release 构建 → `windeployqt --release bagwage.exe`（**别加 `--no-translations`**）。
2. 检查目录里必须有：`platforms\qwindows.dll`、`sqldrivers\qsqlite.dll`、
   `Qt5Core/Gui/Widgets/Sql/Charts.dll`、MinGW 运行时（`libgcc_s_dw2-1.dll`、`libstdc++-6.dll`、`libwinpthread-1.dll`）。
   少了 `sqldrivers\qsqlite.dll` 就是「打开数据库失败」。
3. `.pro` 里的 `CONFIG += console` 会让 GUI 带黑框；打包前做一份去掉 console 的 release 配置。
4. 数据库写在 exe 同目录，拷到只读目录（Program Files）会写不进去；交作业「整个文件夹拷走」没问题。
5. 在没装 Qt 的电脑/虚拟机上双击验证；32 位程序不能混进 64 位 DLL。

> 提示：本机 `PATH` 里的 msys64 带 64 位 Qt6，打包验证时不要把 msys64 的 bin 加进 PATH。
