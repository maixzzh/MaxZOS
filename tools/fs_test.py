#!/usr/bin/env python3
"""MaxZOS 多级目录文件系统冒烟测试
启动无头 QEMU（-kernel，-name maxzos-fs-test 标记），通过 QMP 的
human-monitor-command 包装 sendkey 注入按键、pmemsave 导出 0xB8000
的 VGA 文本内存，比对屏幕内容验证命令行为。
用法：make test  或  python3 tools/fs_test.py
"""
import socket, subprocess, time, os, sys, shutil, json

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MARK = "maxzos-fs-test"
MON = f"/tmp/maxzos-mon-{os.getpid()}.sock"
VGA = "/tmp/maxzos-vga.bin"

# 清理残留测试进程（按 -name 标记匹配，避免误杀其它 QEMU）
subprocess.run(["pkill", "-f", f"-name {MARK}"], capture_output=True)
time.sleep(0.3)
if os.path.exists(VGA):
    os.unlink(VGA)

qemu = shutil.which("qemu-system-i386")
proc = subprocess.Popen([qemu, "-kernel", "bin/kernel.elf",
                         "-name", MARK,
                         "-display", "none",
                         "-qmp", f"unix:{MON},server=on,wait=off"],
                        cwd=REPO, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

for _ in range(100):
    if os.path.exists(MON):
        break
    if proc.poll() is not None:
        print("QEMU 提前退出！"); sys.exit(1)
    time.sleep(0.1)
else:
    print("monitor socket 超时"); proc.kill(); sys.exit(1)

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(MON)
s.settimeout(5)

def qmp_recv():
    buf = b""
    while True:
        buf += s.recv(4096)
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            line = line.strip()
            if line:
                return json.loads(line)

def qmp_send(obj):
    s.sendall((json.dumps(obj) + "\n").encode())

def hmp(cmd):
    qmp_send({"execute": "human-monitor-command",
              "arguments": {"command-line": cmd}})
    return qmp_recv()

qmp_recv()                          # 握手问候
qmp_send({"execute": "qmp_capabilities"})
assert qmp_recv().get("return") == {}, "qmp_capabilities 失败"

def dump_screen():
    r = hmp(f'pmemsave 753664 4000 "{VGA}"')
    if "error" in r:
        return []
    with open(VGA, "rb") as fp:
        data = fp.read(4000)
    lines = []
    for r in range(25):
        row = ""
        for c in range(80):
            ch = data[(r * 80 + c) * 2]
            row += chr(ch) if 32 <= ch < 127 else " "
        lines.append(row.rstrip())
    return lines

def wait_for_banner(timeout=20):
    """轮询屏幕直到内核画完启动横幅（避免启动竞态）"""
    t0 = time.time()
    while time.time() - t0 < timeout:
        if any("Welcome to MaxZOS" in l for l in dump_screen()):
            time.sleep(0.5)          # 再等一瞬，让提示符落定
            return True
        time.sleep(0.2)
    return False

if not wait_for_banner():
    print("内核未在预期时间内显示启动横幅"); proc.kill(); sys.exit(1)

KEYMAP = {
    " ": "spc", "/": "slash", ".": "dot", '"': "shift-apostrophe",  # 美式键盘上档引号
    "-": "minus", "_": "shift-minus", ":": "shift-semicolon",
}

def type_line(text):
    for ch in text:
        hmp(f"sendkey {KEYMAP.get(ch, ch)}")
        time.sleep(0.008)
    hmp("sendkey ret")
    time.sleep(0.25)

def check(tag, expect):
    """整屏搜索期望子串（调用前已 clear 清屏，屏幕只剩当前命令输出）"""
    all_lines = dump_screen()
    scr = "\n".join(all_lines)
    missing = [e for e in expect if e not in scr]
    if missing:
        print(f"[FAIL] {tag!r}: 未找到 {missing}")
        print("       屏幕:\n" + "\n".join("       |" + l for l in all_lines))
        return False
    print(f"[PASS] {tag!r}: {expect}")
    return True

# 测试清单（命令，期望出现在屏幕的子串；每条命令前先 clear 清屏）
tests = [
    # A. 保留行为回归
    ("ls",                     ["Name", "Size(Byte)"]),
    ("create c",               ["usage: create <path> [content]"]),
    ("echo \"a b\"",           ["a b"]),
    ("create \"\" x",          ["empty name"]),
    # B. 目录基本操作
    ("mkdir sub",              []),
    ("ls",                     ["sub/"]),
    ("create sub/f hello",     []),
    ("ls sub",                 ["f", "5"]),
    ("cat sub/f",              ["hello"]),
    ("cd sub",                 ["MaxZOS/sub$"]),
    ("cd ..",                  ["MaxZOS/$"]),
    ("cd /sub",                ["MaxZOS/sub$"]),
    ("cd /",                   ["MaxZOS/$"]),
    ("mkdir /sub/deep",        []),
    ("cd sub/deep",            ["MaxZOS/sub/deep$"]),
    ("cat ../f",               ["hello"]),
    ("ls ..",                  ["f", "5", "deep/"]),
    ("cd /",                   []),
    # C. 错误路径
    ("cat /nonexistent",       ["file not found"]),
    ("cat sub",                ["is a directory"]),
    ("create a x",             []),
    ("cd a",                   ["not a directory"]),
    ("mkdir a/b",              ["not a directory"]),
    ("rm /sub/f",              []),
    ("rm /sub",                ["directory not empty"]),
    ("rm /sub/deep",           []),
    ("rm /sub",                []),
    ("rm /",                   ["cannot remove root"]),
    ("rm .",                   ["cannot remove root"]),   # 在根目录：保护根优先于保护 cwd
    ("rm nonexistent",         ["file not found"]),
    ("mkdir tmp",              []),
    ("cd tmp",                 ["MaxZOS/tmp$"]),
    ("rm .",                   ["cannot remove current directory"]),
    ("rm ..",                  ["cannot remove root"]),   # 在 /tmp 中 rm .. = 删根
    ("cd /",                   []),
    ("mkdir /tmp/deep2",       []),
    ("cd /tmp/deep2",          ["MaxZOS/tmp/deep2$"]),
    ("rm .",                   ["cannot remove current directory"]),
    ("rm ..",                  ["directory not empty"]),   # 祖先 /tmp 非空，非空检查优先
    ("cd /",                   []),
    ("rm /tmp/deep2",          []),
    ("rm /tmp",                []),
    # D. 边界
    ("create / x",             ["bad path"]),
    ("mkdir .",                ["bad path"]),
    ("mkdir a//b",             ["not a directory"]),   # 连续斜杠容忍，a 是文件
    ("delete a",               []),
    ("ls",                     ["Name", "Size(Byte)"]),
    # E. 全局路径（addpath / listpath / cat 回退查找）
    ("mkdir g1",               []),
    ("mkdir g2",               []),
    ("create g1/note data1",   []),
    ("create g2/note data2",   []),
    ("create g2/only2 data2b", []),
    ("addpath /g1",            []),
    ("addpath /g2",            []),
    ("addpath /g1",            ["already in path list"]),
    ("addpath /nonexist",      ["file not found"]),
    ("addpath /g1/note",       ["not a directory"]),
    ("addpath",                ["usage: addpath"]),
    ("listpath",               ["/g1", "/g2"]),
    ("cat note",               ["data1"]),      # 当前目录没有 → 第 1 个全局路径
    ("cat only2",              ["data2b"]),     # 只有第 2 个全局路径有 → 也能找到
    ("create note local",      []),
    ("cat note",               ["local"]),      # 当前目录优先
    ("mkdir /g2/dir",          []),
    ("cat dir",                ["is a directory"]),  # 命中目录条目立即报错，不继续遍历
    ("cat /g2/note",           ["data2"]),      # 绝对路径不受全局路径影响
    # F. 全局路径上限（已有 /g1 /g2 两个，还剩 6 个名额）
    ("mkdir /p1",              []),
    ("mkdir /p2",              []),
    ("mkdir /p3",              []),
    ("mkdir /p4",              []),
    ("mkdir /p5",              []),
    ("mkdir /p6",              []),
    ("mkdir /p7",              []),
    ("addpath /p1",            []),
    ("addpath /p2",            []),
    ("addpath /p3",            []),
    ("addpath /p4",            []),
    ("addpath /p5",            []),
    ("addpath /p6",            []),
    ("addpath /p7",            ["too many paths"]),
]

failed = 0
for cmd, expect in tests:
    type_line("clear")          # 清屏，避免旧输出干扰整屏搜索
    type_line(cmd)
    if not check(cmd, expect):
        failed += 1
        if failed >= 5:
            print("失败过多，终止"); break

proc.kill()                     # 确保进程退出（quit 可能挂起）
proc.wait(timeout=5)
print(f"\n=== 结果: {len(tests) - failed}/{len(tests)} 通过 ===")
sys.exit(1 if failed else 0)
