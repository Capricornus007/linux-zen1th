#!/bin/bash
# xanmod 層的 next 適配步驟（merge-bot.yml 在套 generic xan.diff 前呼叫）。
#
# 背景：xanmod 7.3 跟 stable/linux-7.3.y 走，其差量對 linux-next 基座必然漂移。
# 2026-11-25 起 22 檔長期衝突（含 include/ 標頭）觸發「整層回退」，
# xanmod 優化（BBRv3、workingset protection、Steam Deck、fullcone 等）整層被丟。
#
# 本腳本：
#   1. 以 fuzz 0 套用 next-adapted.patch（22 檔衝突中 14 檔的人手三路合併結果，
#      對 next+zen 基座可乾淨套用）。任一 hunk 失敗即以 rc!=0 收場——
#      next 漂移時必須更新適配版，大聲失敗，不可靜默丟層。
#   2. 寫出 /tmp/xan-skip.txt（21 檔）：已由適配版處理或已無移植對象的檔，
#      merge-bot 的 generic 逐檔 3way 迴圈會跳過，避免「已套用→Reversed→又算衝突」。
#
# 維護：next 漂移導致本腳本失敗時，重新對照
#   base = merge-base(xanmod/7.3, stable/linux-7.3.y)
#   ours = next/master（+zen，zen 差量現為 0）
#   theirs = xanmod/7.3
# 重做三路合併並更新 next-adapted.patch 與 README.md。
set -e

cat > /tmp/xan-skip.txt <<'SKIP'
drivers/android/Kconfig
drivers/android/Makefile
drivers/android/binder.c
drivers/android/binder_alloc.c
drivers/android/binder_alloc.h
drivers/android/binder_internal.h
drivers/android/binderfs.c
drivers/hwmon/Makefile
drivers/leds/Kconfig
drivers/mfd/Kconfig
drivers/mfd/Makefile
include/linux/tcp.h
include/net/tcp.h
kernel/sysctl.c
mm/Kconfig
mm/vmscan.c
net/ipv4/bpf_tcp_ca.c
net/ipv4/tcp.c
net/ipv4/tcp_bbr.c
net/ipv4/tcp_input.c
net/ipv4/tcp_output.c
SKIP

patch -Np1 --fuzz=0 < /tmp/xan-adapted/next-adapted.patch
echo "xanmod next 適配版：14 檔已套用（fuzz 0）；generic 差量將排除 $(wc -l < /tmp/xan-skip.txt) 檔"
