# 第 4 课：生存三分钟

这是 `lesson-4` 学生起点分支。下载 ZIP 时先在仓库选择 `lesson-4`，再用 **Code → Download ZIP**；已有复刻的同学先更新远端引用，再从 `upstream/lesson-4` 建立自己的 `my-lesson-4` 练习分支。不要用旧课整份代码覆盖新起点。

打开本分支的 [student-guide.html](student-guide.html) 阅读任务与逐级提示；在线课程主页为 <https://xianyudd.github.io/first-frame/>。下载前请确认远端确实已有这个分支；本地准备不等于发布完成。

## 先认识三个模块

- `src/main.cpp`：窗口、输入与主循环。
- `src/game.cpp` / `src/game.h`：游戏数据与规则。
- `src/drawing.cpp` / `src/drawing.h`：画面与血条。

六项任务共 **13 个 TODO 定位标记**，分布在 `game.cpp`、`drawing.cpp`、`my_experiment.h`。先定位，再读附近输入与用途；不要一次写完整份答案。

## 运行与验证

在已配置 raylib 的终端、课包根目录运行（Windows 使用 MSYS2 UCRT64）：

```sh
make todo
make run
make test
make test VERBOSE=1
```

`make` / `make game` 编译三个模块，产物为 `build/game-l4`（Windows 为 `build/game-l4.exe`）。`make run` 编译后启动。`make test` 真正链接多个翻译单元，默认只显示失败细节、各组结果与总分；`VERBOSE=1` 另外显示通过项。未完成的起点出现红分与非零退出码是预期，不能把它改成假通过。

01–05 是五个自动组；06 单数值实验要记录改动、预测、观察与解释，人工验收，不计入自动全绿。自动测试也不能替代实际画面和声音检查。

## 操作

默认进入 **练习模式**，不自动刷怪。

- WASD：移动。
- **持续按住方向键**：射击，不是只响应按下瞬间。
- TAB：切换练习 / 战斗，并重置本局。
- Enter：重开，保留当前模式与调试开关。
- G：显示 / 隐藏调试网格，只改变视觉。
- 1 / 2 / 3：出现三选一时选择对应候选。

每做一点：先预测 → 修改 → `make test` 检查对应组 → `make run` 观察 → 用自己的话解释。卡住时回到指南对应任务，按需打开局部提示。
