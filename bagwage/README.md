# bagwage — 书包生产记账系统

Qt 5.9.1 / MinGW 5.3（32 位）/ qmake / SQLite。

> **当前进度：阶段①（工序）、阶段②（书包 + 单价表）已完成。**
> 已完成：数据库层、公共件（ServiceResult / Transaction / 临时库自检）、员工、工序、书包 + 单价表。
> 未完成：订单+明细、订单分页、工资核算、图表、打包。

---

## 一、目录结构

```
bagwage/
├── bagwage.pro            qmake 工程（QT += sql charts, CONFIG += c++11）
├── main.cpp               入口：--selftest 走自检，否则开库建表 → 主窗口
├── selftest.cpp/.h        无界面自检（跑在临时库上，不碰正式库）
├── build.bat              一键编译 / 运行 / 自检 / 清理
├── dao/                   一张表一个 DAO，只写 SQL
│   ├── models.h           实体结构体
│   ├── employeedao.*      员工
│   ├── processdao.*       工序（含 bag_process / order_detail 引用查询）
│   ├── bagdao.*           书包（含「已配单价工序数」聚合）
│   └── bagprocessdao.*    书包×工序单价（按书包整组替换）
├── db/
│   ├── dbmanager.*        连接 + 建表 + 索引 + 轻量迁移
│   └── transaction.*      RAII 事务（析构自动回滚）
├── service/               业务：校验、引用检查、事务编排
│   ├── serviceresult.h    所有 Service 共用的返回类型
│   ├── employeeservice.*
│   ├── processservice.*
│   └── bagservice.*       书包 + 单价表（同一个事务）
└── ui/                    界面：只收集输入、弹窗、刷新表格
    ├── mainwindow.*
    ├── employeedialog.*
    ├── processdialog.*
    ├── bagdialog.*        书包名 + 单价表（工序下拉 + 单价）
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
当前共 56 项检查（阶段① 24 项 + 阶段② 32 项），期望最后一行 `===== 结果：PASS 56 / FAIL 0 =====`，退出码 0。

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
| 书包 增/改/删/查 + 界面 | ✅ 删除时校验订单引用 | `service/bagservice.cpp` |
| 书包 × 工序 单价表 | ✅ 事务内整组替换，删掉的单价不会残留 | `service/bagservice.cpp` |
| 自检跑临时库 | ✅ 56 项 | `selftest.cpp` |
| 订单 / 工资 / 图表 | ⬜ 占位页 | `ui/placeholderpage.cpp` |

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

---

## 五、下一步（阶段③：订单 + 批量明细）

1. `dao/orderdao.*`：`insert`（拿 `lastInsertId()`）、`update`、`remove`、`findById`、
   `countByBag`/`countByEmployee`（分页要用的总数）。
2. `dao/orderdetaildao.*`：按订单查明细、按订单删除、批量插入。
3. `service/orderservice.*`：
   - `createOrder(order, details)` 用 `Transaction` 包住：插 `orders` → 拿 id → 循环插明细 → commit
   - **录明细时就校验单价存在**：`bag_process` 里没有 `(bag_id, process_id)` 就当场拒绝，
     别等到算工资时才发现是 0 元
   - 明细里的 `emp_id`/`process_id` 必须存在
4. `ui/orderdialog.*`：选书包 + 楼层 + 数量 + 日期，明细表格（员工 / 工序 / 数量 / 日期），可增删行。
5. 主窗口「订单」页：列表 + 新建/编辑/删除，删除前二次确认（明细会被级联删掉）。
6. `selftest.cpp` 加用例：事务内插 1 单 + 2 条明细，第 2 条故意用不存在的 emp_id，
   断言失败后 `orders` 和 `order_detail` 都是 0 行。

> 提示：`order_detail` 对 `orders` 是 `ON DELETE CASCADE`，删订单时明细会被静默带走，
> 界面必须二次确认；`orders.order_date` 列已经在阶段①建好了。
