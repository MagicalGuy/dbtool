# DbGuiTool —— C++/Qt 桌面数据库 GUI 连接工具（类 Navicat 初始版本）

一个用 **C++ + Qt 5** 写的桌面数据库图形客户端，定位是“简化版 Navicat”。
初始版本围绕**连接管理、表数据增删改查、SQL 执行与结果展示**这些最常用功能实现，
全部用 Qt 自带的 `QtSql` 模块完成，**SQLite 开箱即用，无需安装任何数据库服务器**。

---

## 一、功能一览（已实现）

| 模块 | 功能 |
|------|------|
| 连接管理 | 新建 / 编辑 / 删除连接、测试连接、连接配置持久化（`connections.ini`） |
| 数据库浏览 | 左侧三级树（**连接 → 数据库 → 表/视图**）；MySQL 一个实例下所有可访问的库全部列出，库内表/视图展开时按需加载，双击打开 |
| 表数据编辑 | 表格里**直接双击编辑**、新增行、删除多行、统一保存 / 撤销、刷新 |
| 数据过滤 | 选择列 + 关键字模糊查询（`LIKE`），表头点击排序 |
| SQL 编辑器 | 多语句脚本依次执行（自动跳过引号/注释中的分号），F5 / Ctrl+Enter 运行，仅执行选中部分 |
| 结果展示 | 最后一条查询语句的结果集展示，非查询语句统计影响行数与耗时 |
| 导出 | 表数据 / 查询结果导出为 **CSV（带 UTF-8 BOM，Excel 直接打开不乱码）** |
| 示例数据 | 新建连接对话框里一键生成含 `departments`、`employees` 两张表的示例 SQLite 库 |
| 多数据库 | 原生支持 SQLite、**MySQL（QMYSQL，已编译接入）**、PostgreSQL、ODBC（可经 ODBC 连 SQL Server 等） |
| 安全提示 | 关闭标签页 / 断开 / 退出时，对未保存的表数据弹保存确认 |

---

## 二、运行环境

- Qt **5.13.0**，套件 **Desktop Qt 5.13.0 MSVC2015 64bit**（与你现有示例一致）
- 编译器：Microsoft Visual C++ 2015（vcvarsall.bat）
- 构建系统：qmake（`.pro` 工程，**纯代码构建 UI，不依赖 `.ui` 文件**）
- 依赖模块：`core gui widgets sql`

> 数据库驱动（`plugins/sqldrivers`）：官方自带 `qsqlite`（SQLite）、`qsqlodbc`（ODBC）、
> `qsqlpsql`（PostgreSQL）；**本次已额外用 MySQL 8.0 客户端库编译并安装了原生
> `qsqlmysql`（QMYSQL）驱动**，编译与部署的完整过程见第九节。

---

## 三、目录与文件说明

项目目录：`E:\QT course\C++QT5\01\003dbtool`

```
003dbtool/
├── dbtool.pro              qmake 工程文件
├── main.cpp                程序入口（含 --selftest 无界面自检）
├── connection.h/.cpp       连接配置结构体 + INI 持久化（ConnectionStore）
├── dbmanager.h/.cpp        连接管理单例：根连接/每库独立连接、枚举数据库(SHOW DATABASES)、列出表与视图
├── connectiondialog.h/.cpp 新建/编辑连接对话框（测试连接、选文件、建示例库）
├── tabletab.h/.cpp         表数据浏览编辑页（QSqlTableModel，增删改查/过滤/导出）
├── querytab.h/.cpp         SQL 查询页（多语句执行、结果集、导出）
├── mainwindow.h/.cpp       主窗口：菜单栏/工具栏、连接树、多标签页
├── build_msvc2015_64.bat          命令行一键编译（Debug，影子构建）
├── build_msvc2015_64_release.bat  命令行一键编译（Release，影子构建）
├── build_qsqlmysql_driver.bat     编译原生 QMYSQL 驱动插件（见第九节）
└── run_selftest.bat               命令行跑无界面自检（offscreen）
```

构建产物目录（影子构建，可执行文件在其下 `debug` / `release` 子目录）：

- Debug：`E:\QT course\C++QT5\01\build-dbtool-Desktop_Qt_5_13_0_MSVC2015_64bit-Debug\debug\dbtool.exe`
- Release：`E:\QT course\C++QT5\01\build-dbtool-Desktop_Qt_5_13_0_MSVC2015_64bit-Release\release\dbtool.exe`
  （该 Release 目录已用 **windeployqt** 部署为可直接双击运行的绿色版，并已内置 QMYSQL 运行时）

---

## 四、在 Qt Creator 中运行（推荐）

1. 打开 Qt Creator，菜单 **文件 → 打开文件或项目**，选择
   `E:\QT course\C++QT5\01\003dbtool\dbtool.pro`。
2. 选择套件 **Desktop Qt 5.13.0 MSVC2015 64bit**，点 **Configure Project**。
3. 点击左下角绿色三角 **运行（Ctrl+R）** 即可。要用 Release，在左侧 **项目(Projects)**
   模式里把"构建配置(Build)"从 `Debug` 切到 `Release`，或直接在左下角套件选择器切换。
4. 首次运行左侧为空，按第五节步骤上手。

> 命令行方式（可选）：`build_msvc2015_64.bat` 编译 Debug、
> `build_msvc2015_64_release.bat` 编译 Release；`run_selftest.bat` 用于在不弹窗的情况下
> 验证数据库增删改查与各 SQL 驱动能否加载（返回码 0 表示通过，并在 exe 同目录生成
> `selftest_report.txt`）。

---

## 五、快速上手（3 分钟）

1. **新建连接**：点工具栏“新建连接” → 数据库类型保持默认 `QSQLITE`
   → 点 **“创建示例库...”**，选一个位置保存 `sample.db`，连接名称会自动填“示例SQLite”
   → 可先点“测试连接” → 确定。
2. **连接数据库**：在左侧**双击“示例SQLite”**（或选中后点“连接”），展开“表”，
   能看到 `departments`、`employees`。
3. **浏览/编辑数据**：**双击表 `employees`**，右侧打开表格页。
   - 双击任意单元格直接改；
   - 点“新增行”录入，选中若干行点“删除选中行”；
   - 改动后点 **“保存修改”** 才真正写库（也可“撤销”）；
   - 右上“查找”区域可选列、输关键字做模糊过滤；点表头排序；“导出CSV”保存数据。
4. **执行 SQL**：选中表后点 **“新建查询”**，会自动带入 `SELECT * FROM ...;`，
   按 **F5** 执行。也可一次粘贴多条语句，例如：
   ```sql
   INSERT INTO departments(name, location) VALUES ('法务部','深圳');
   UPDATE employees SET salary = salary + 500 WHERE department_id = 1;
   SELECT d.name AS 部门, COUNT(*) AS 人数, AVG(e.salary) AS 平均薪资
   FROM employees e JOIN departments d ON e.department_id = d.id
   GROUP BY d.name;
   ```
   结果集显示在下方表格，状态栏给出语句条数、影响行数 / 返回行数与耗时。

---

## 六、连接不同数据库

在“新建连接”对话框的**数据库类型**下拉里，列出的是当前 Qt 实际可用的驱动：

- **QSQLITE（推荐入门）**：数据库就是一个文件，选文件即可，不存在会自动创建。
- **QPSQL（PostgreSQL）**：填写主机、端口（默认 5432）、用户名、密码、数据库名。
  需能访问到 PostgreSQL 服务端。
- **QMYSQL（MySQL / MariaDB，已可用）**：填写主机（本机填 `127.0.0.1` 或 `localhost`）、
  端口（默认 `3306`）、用户名、密码即可，点“测试连接”。需要一台可访问的 MySQL
  服务端（本机安装或远程均可）。下拉里同时出现的 `QMYSQL3` 是旧版兼容别名，选 `QMYSQL` 即可。
  - **“数据库”栏对 MySQL 可留空**：留空也能连上实例，连接后左侧会列出该账号**所有可访问的
    数据库**（执行 `SHOW DATABASES`，包含 `information_schema`、`mysql`、`sys` 等系统库）；
    若填写了库名，连接后会自动展开并定位到该库。
  - 左侧为 **连接（实例）→ 数据库 → 表/视图** 三级树：双击某个数据库（或点它前面的展开箭头）
    才会真正打开该库并加载其中的表/视图（懒加载）。每个打开的数据库使用**一条独立的
    QSqlDatabase 连接**，因此多个库的数据页 / 查询页互不干扰，表编辑也能在各自默认库内正确
    识别主键。
  - 右键表选“新建查询”时会自动生成**带库名限定**的语句，例如
    <code>SELECT * FROM `m13_en_urs`.`cdb_user_coin`;</code>（反引号是 MySQL 标准标识符引号，
    等价于 `m13_en_urs.cdb_user_coin`，并能兼容含特殊字符或保留字的名字）；
    在数据库节点上“新建查询”则会带入 `SHOW TABLES;`。
  - 说明：PostgreSQL 一个连接绑定一个数据库、SQLite 是单文件、ODBC 语法各异，因此这三类仍保持
    “连接即单库”的两级树，只有 MySQL/MariaDB 使用上面的多数据库三级树。
- **QODBC（ODBC）**：在"数据库"栏填 **ODBC DSN 名称**，或完整连接串，例如
  `DRIVER={MySQL ODBC 8.0 Driver};SERVER=127.0.0.1;DATABASE=test;USER=root;PASSWORD=xxx;OPTION=3;`
  （需先在 Windows"ODBC 数据源管理器"装好对应驱动并配置 DSN）。
  SQL Server 等也可经此方式连接。

> **QMYSQL 是怎么来的？** Qt 5.13 官方包默认不带 `qsqlmysql.dll`，本次已用 MySQL 8.0
> 客户端库自行编译并安装（完整步骤见第九节）。**程序代码无需任何改动**——下拉项由 Qt 运行时
> 从 `plugins/sqldrivers` 动态枚举，装好驱动、重启程序即自动出现。

---

## 七、常见问题（FAQ）

- **中文会乱码吗？**
  不会。所有源码保存为 **UTF-8 with BOM**，界面文字统一用 `QStringLiteral(...)`
  （编译期生成 UTF-16），在 MSVC 下也不依赖系统代码页；CSV 导出带 UTF-8 BOM。
- **连接信息保存在哪？密码安全吗？**
  保存在**可执行文件同目录**的 `connections.ini`。初始版本密码为**明文**，仅适合本机学习；
  正式使用应改为系统凭据库或加密存储（见第八节扩展建议）。
- **视图能改吗？**
  视图以只读方式打开（不可编辑），表可编辑。
- **大表会不会卡？**
  当前表数据页一次性 `SELECT` 全部数据，适合万级以内数据学习使用；
  超大数据集应增加分页（LIMIT/OFFSET 或主键翻页）。
- **执行脚本报某条语句错？**
  状态栏会提示“第 N / 总条数 条语句执行失败”及数据库返回的具体错误，定位到该条即可。
- **自增主键新增时要填吗？**
  不用，留空保存后由数据库生成，保存成功后表格会自动刷新取回。

---

## 八、后续可扩展方向（学习路线）

1. 表结构可视化：查看/新建/修改/删除表（DDL 生成器）、主键与索引管理。
2. 大表分页加载、结果集分页、SQL 语法高亮与自动补全。
3. 显式事务（开始/提交/回滚）、批量导入、SQL 历史记录、查询脚本保存为 `.sql`。
4. 连接密码加密存储（如 Windows DPAPI / QKeychain）、连接分组与颜色标记。
5. 多结果集、存储过程、ER 关系图、数据比对与同步。
6. 用 Qt Installer Framework / windeployqt 打包成可分发的安装包。

> 打包提示：发布 Release 时在 Qt 命令行对 `dbtool.exe` 运行 `windeployqt --release dbtool.exe`，
> 它会自动拷贝 Qt 运行库与 `sqldrivers`、`platforms` 插件（含 qsqlmysql.dll）；
> 但 **`libmysql.dll`、`libssl-3-x64.dll`、`libcrypto-3-x64.dll` 需手动拷到 exe 同目录**
> （windeployqt 不识别这些第三方库），详见第九节 9.5。

---

## 九、为 Qt 5.13 编译原生 QMYSQL 驱动（MySQL 8.0，Windows / MSVC2015 64 位）

本节记录把 `QMYSQL` 接入本机 Qt 的完整、可复现过程（本次已按此完成并验证）。
核心思路：Qt 自带 SQL 驱动**源码**，但没编 MySQL 插件；用官方 MySQL 客户端开发库
（头文件 + 导入库）编译 Qt 的 `qsqlmysql` 插件，再把插件和运行时 dll 放到 Qt 能找到的位置。

### 9.0 环境与关键路径

| 项 | 路径 |
|----|------|
| qmake | `D:\Qt\Qt5.13.0\5.13.0\msvc2015_64\bin\qmake.exe` |
| jom | `D:\Qt\Qt5.13.0\Tools\QtCreator\bin\jom.exe` |
| MSVC 环境 | `C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat x64` |
| SQL 驱动源码 | `D:\Qt\Qt5.13.0\5.13.0\Src\qtbase\src\plugins\sqldrivers` |
| 插件安装目录 | `D:\Qt\Qt5.13.0\5.13.0\msvc2015_64\plugins\sqldrivers` |
| MySQL 开发库 | `D:\mysql-dev\mysql-8.0.46-winx64` |

### 9.1 下载并解压 64 位 MySQL 开发文件

下载 MySQL Community Server 的 **Windows x86-64 ZIP**（内含客户端开发库，无需安装服务器）：

- 直链：`https://cdn.mysql.com//Downloads/MySQL-8.0/mysql-8.0.46-winx64.zip`（约 236 MiB）
- 解压到 `D:\mysql-dev\`，得到 `D:\mysql-dev\mysql-8.0.46-winx64`。

编译 / 运行用到的文件（8.0 目录布局）：

- 头文件：`include\mysql.h`
- 导入库：`lib\libmysql.lib`
- 客户端运行库：`lib\libmysql.dll`（注意 8.0 在 **lib** 目录，不是旧版的 bin）
- OpenSSL 3 运行库：`bin\libcrypto-3-x64.dll`、`bin\libssl-3-x64.dll`

### 9.2 编译 qsqlmysql 插件

脚本（本机位于 `D:\mysql-dev\build_qsqlmysql.bat`，源码目录也自带一份
`build_qsqlmysql_driver.bat`，内容相同）的关键步骤：先用 `MYSQL_INCDIR / MYSQL_LIBDIR`
告诉 qmake 头文件和库的位置，再编译 mysql 子项目：

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64
cd /d "D:\Qt\Qt5.13.0\5.13.0\Src\qtbase\src\plugins\sqldrivers"
"D:\Qt\Qt5.13.0\5.13.0\msvc2015_64\bin\qmake.exe" -- "MYSQL_INCDIR=D:/mysql-dev/mysql-8.0.46-winx64/include" "MYSQL_LIBDIR=D:/mysql-dev/mysql-8.0.46-winx64/lib"
"D:\Qt\Qt5.13.0\Tools\QtCreator\bin\jom.exe" sub-mysql
```

配置阶段应打印 `Checking for MySQL... yes`，编译零错误。产物（in-source 构建，路径较深）在

`D:\Qt\Qt5.13.0\5.13.0\Src\qtbase\src\plugins\sqldrivers\plugins\sqldrivers\`

下的 `qsqlmysql.dll`（Release）与 `qsqlmysqld.dll`（Debug）。

> 说明：MSVC2015 编译 Qt 5.13 的该源码、对接 MySQL 8.0 头文件可正常通过；
> `libmysql.dll` 虽由更高版本 VS 构建，但其为标准 C ABI 导入库，MSVC2015 可正常链接。

### 9.3 部署（让 Qt 与本程序都能加载）

1. 插件拷入 Qt 插件目录：`qsqlmysql.dll`（Release，必需）、`qsqlmysqld.dll`（Debug，可选）
   → `D:\Qt\Qt5.13.0\5.13.0\msvc2015_64\plugins\sqldrivers\`
2. 运行时依赖拷入 Qt 的 bin 目录（供 Qt Creator 运行时经 PATH 加载）：
   `libmysql.dll`、`libssl-3-x64.dll`、`libcrypto-3-x64.dll`
   → `D:\Qt\Qt5.13.0\5.13.0\msvc2015_64\bin\`
3. 重启程序，"新建连接"的数据库类型下拉即出现 `QMYSQL`、`QMYSQL3`。

### 9.4 验证

- 程序自检：`dbtool.exe --selftest` 返回码 0，并在 exe 同目录生成 `selftest_report.txt`，
  其中包含 `QMYSQL = YES`。`YES` 表示插件及其 libmysql / OpenSSL 依赖全部加载成功
  （只实例化驱动，不需要真实 MySQL 服务器）。
- 实测驱动列表：`QSQLITE,QMYSQL,QMYSQL3,QODBC,QODBC3,QPSQL,QPSQL7`。
- 真正连通还需一台 MySQL 服务端：在连接对话框填主机 / 端口 / 账号 / 库名后点"测试连接"。

### 9.5 分发到没有 Qt / MySQL 的电脑

对 Release 版 `dbtool.exe` 运行 `windeployqt --release dbtool.exe` 后，**额外手动**：

1. 把 `libmysql.dll`、`libssl-3-x64.dll`、`libcrypto-3-x64.dll` 拷到 `dbtool.exe` 同目录；
2. 确认目标机装有 **Visual C++ 2015-2022 x64 运行库**（Qt 与 libmysql 均依赖）；
3. 若要在目标机跑无界面自检，再带上 `plugins\platforms\qoffscreen.dll`
   （正常图形运行只需要 qwindows.dll）。

本次已按此把 Release 目录做成绿色版，并在"PATH 仅含 System32"的干净环境下验证
`QMYSQL = YES`、GUI 可正常启动与关闭。
