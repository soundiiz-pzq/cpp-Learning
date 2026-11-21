# bagwage — 书包生产记账系统（骨架）

Qt 5.9.1 / MinGW 5.3（32 位）/ qmake / SQLite。

> 这份工程目前是**骨架**：数据库层、分层结构、员工模块已经完整可用；
> 其余模块是占位页。照着员工模块的写法往下补即可。

---

## 一、目录结构

```
bagwage/
├── bagwage.pro           qmake 工程文件（QT += sql charts, CONFIG += c++11）
├── main.cpp              入口：开库 → 建表 → 自检或开主窗口
├── dao/
│   ├── models.h          6 个实体结构体（纯数据）
│   └── employeedao.*     员工表 SQL（已实现，作为其余 DAO 的模板）
├── db/
│   └── dbmanager.*       连接管理 + 建表（6 张表 + 索引，事务内执行）
├── service/
│   └── employeeservice.* 业务校验 + 引用校验（已实现）
└── ui/
    ├── mainwindow.*      主窗口 + 6 个功能页
    ├── employeedialog.*  员工录入/编辑对话框（已实现）
    └── placeholderpage.* 占位页（实现某模块时替换掉）
```

分层规矩（需求文档第六节）：**ui 不碰 dao、不开事务**；SQL 只在 dao；事务和校验只在 service。

---

## 二、构建与运行

### 用 Qt Creator（推荐）

1. 打开 `bagwage.pro`，Kit 选 `Desktop Qt 5.9.1 MinGW 32bit`。
2. `Ctrl+B` 编译，`Ctrl+R` 运行。

### 用命令行

```powershell
$env:Path = "D:\Qt5.9.1\5.9.1\mingw53_32\bin;D:\Qt5.9.1\Tools\mingw530_32\bin;" + $env:Path
cd C:\Users\PYH\Desktop\cpp\bagwage
qmake bagwage.pro
mingw32-make -j4
.\bagwage.exe
```

### 无界面自检（确认建库/建表/外键都对）

```powershell
.\bagwage.exe --selftest
```

期望输出 8 行 `[PASS]`，最后一行 `===== 结果：PASS =====`。程序会在**同目录**生成 `bagwage.db`。

---

## 三、⚠️ 两个必须知道的坑（实测踩到过）

### 1. PATH 里的 MSYS2 会抢 Qt 的 DLL

本机 `PATH` 里有 `C:\msys64\mingw64\bin`，里面有 **Qt 6.12** 的 DLL。
如果直接从命令行运行 `bagwage.exe`，可能加载到 Qt 6 的 DLL，直接报
`0xC000007B`（`-1073741701`）闪退。

**解决**：运行前把 Qt 5.9.1 的 bin 放到 PATH 最前面（上面的命令已经这么写了），
或者用 Qt Creator 运行（Kit 会自己处理）。

### 2. 32 位 + C++11

- 生成的是 32 位程序，不能链接 64 位第三方库；
- g++ 5.3 只支持到 C++14，**代码按 C++11 写**，别用 `std::optional`、`std::filesystem`、结构化绑定。

---

## 四、已经能跑通的东西

| 功能 | 状态 | 位置 |
|---|---|---|
| 建库 + 6 张表 + 索引 | ✅ 事务内创建，可重复执行 | `db/dbmanager.cpp` |
| 外键约束 | ✅ 每个连接单独 `PRAGMA foreign_keys = ON` | `db/dbmanager.cpp` |
| 员工 增 / 改 / 删 / 查 | ✅ 含输入规范化、引用校验 | `service/employeeservice.cpp` |
| 员工界面 | ✅ 表格 + 新增/编辑/删除/刷新 | `ui/mainwindow.cpp`、`ui/employeedialog.cpp` |
| 其余 5 个模块 | ⬜ 占位页，页面上写了待办清单 | `ui/placeholderpage.cpp` |

**删员工会被拦**：如果该员工已有计件明细（`order_detail`），会弹窗提示条数并拒绝删除。
这是需求里「引用校验」的示范实现。

---

## 五、下一步怎么加模块（照员工模块抄）

以「工序」模块为例，四步：

1. **dao/procesdao.h/.cpp** —— 照 `dao/employeedao.*` 写 `listAll / insert / update / remove`，
   再加引用计数：`countBagProcess(processId)`、`countOrderDetails(processId)`。
2. **service/processservice.h/.cpp** —— 照 `service/employeeservice.*` 写校验和删除拦截；
   引用计数 > 0 就返回失败，message 里写清楚被谁引用。
3. **ui/processdialog.h/.cpp** —— 照 `ui/employeedialog.*` 写录入对话框。
4. **ui/mainwindow.cpp** —— 把占位页换成真页面，把新文件加进 `bagwage.pro` 的
   `SOURCES` / `HEADERS`，重新 `qmake`。

**每次加完文件都要重新跑 `qmake`**，否则 Makefile 里没有新文件，会报找不到目标。

---

## 六、各模块的实现提示（来自需求文档和自检结论）

- **书包-工序单价**：保存时开事务，先写 `bag` 拿到 id，再循环
  `INSERT OR REPLACE INTO bag_process(bag_id, process_id, price)`。
  表上已有 `UNIQUE(bag_id, process_id)`，重复插入会冲突，用 `OR REPLACE`。
- **订单 + 批量明细**：一个事务内先插 `orders`，`q.lastInsertId()` 拿订单 id，
  再循环插 `order_detail`；任何一条失败就 `rollback()`。
- **订单维度分页**：先 `SELECT DISTINCT order_id ... LIMIT ? OFFSET ?` 拿当页订单 id，
  再回查 `orders`。直接对明细分页会把粒度变成明细行数。
- **工资核算**：单价取 `bag_process`，不是 `process.price`：
  ```sql
  SELECT e.id, e.name, SUM(d.qty * bp.price) AS total
  FROM order_detail d
  JOIN employee e ON e.id = d.emp_id
  JOIN orders o ON o.id = d.order_id
  JOIN bag_process bp ON bp.bag_id = o.bag_id AND bp.process_id = d.process_id
  WHERE substr(d.work_date, 1, 7) = ?
  GROUP BY e.id ORDER BY total DESC
  ```
- **子线程图表**：子线程里 `QSqlDatabase::addDatabase("QSQLITE", "unique_name")`，
  用完先让 `QSqlQuery`、`QSqlDatabase` 离开作用域，最后 `removeDatabase`，顺序错了会警告
  `connection is still in use`。
- **图表空数据**：画之前先判空，别对空容器取最大值。
