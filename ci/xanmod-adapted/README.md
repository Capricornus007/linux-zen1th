# xanmod 層 next 適配版（next-adapted.patch）

日期：2026-11-26
基準：
- `base`  = merge-base(xanmod/7.3, stable/linux-7.3.y) = `1a0388846138`（xanmod 的出發點）
- `ours`  = next/master = `9437046953b2`（next-20261125；zen 差量現為 0，故 next+zen 基座等同 next）
- `theirs` = xanmod/7.3 差量（/tmp/xan.diff，96 檔）

## 運作方式

merge-bot 在套 generic xan.diff **之前**先跑 `apply.sh`：
1. `patch -Np1 --fuzz=0 < next-adapted.patch`——任一 hunk 失敗即紅（next 漂移要大聲）。
2. 寫出 `/tmp/xan-skip.txt`（21 檔），generic 逐檔 3way 迴圈跳過這些檔，
   避免「已套用→Reversed→又算衝突」。

其餘 ~75 個乾淨檔照舊走 generic 3way。`kernel/sched/fair.c` 不在適配版內：
對純 next 可乾淨 3way，交給 generic 迴圈即可（若未來 BORE 層先改了 fair.c
導致衝突，該檔會被回退並記 warning，屬 best-effort 行為）。

## 逐檔解法（14 檔入補丁）

| 檔案 | 解法 |
|---|---|
| drivers/hwmon/Makefile | 平凡衝突：next 新增 STARFIVE_PWM_FAN 與 xanmod 新增 SENSORS_STEAMDECK 同位置，兩條都保留 |
| drivers/leds/Kconfig | 同上：LEDS_LTC3208（next）與 LEDS_STEAMDECK（xanmod）都保留 |
| drivers/mfd/Kconfig | 同上：MFD_ZTE_ZX297520V3_CRM（next）與 MFD_STEAMDECK（xanmod）都保留 |
| drivers/mfd/Makefile | 同上：兩條 obj- 都保留 |
| mm/Kconfig | next 側為空，全取 xanmod：ARCH_WANT_HUGETLB_VMEMMAP_PREINIT + ANON/CLEAN_LOW/CLEAN_MIN_RATIO 三個保護比例選項 |
| kernel/sysctl.c | next 已自帶 sysctl_subsys_table/sysctl_init_bases（xanmod 的衝突區塊捨棄）；只移植 xanmod 意圖：開頭 `extern int unprivileged_userns_clone` + 表內 unprivileged_userns_clone 條目（變數定義由乾淨套用的 kernel/user_namespace.c 提供） |
| include/linux/tcp.h | 位元欄位衝突：next 的 bytes_acked_syn+BPF_TCP_OPS 區塊與 xanmod 的 fast_ack_mode 都保留（同 u8 群共 7 bits，相容） |
| include/net/tcp.h | tso_segs 簽名 u32 與 unsigned int 同型，取 next 版（含新註解）；xanmod 新增的 skb_marked_lost ops 鉤子保留（BBRv3 與 tcp_input.c 依賴） |
| net/ipv4/bpf_tcp_ca.c | tso_segs 簽名型別之爭（u32 vs unsigned int 同型），取 next；無實質 diff，不入補丁，僅入 skip 清單 |
| net/ipv4/tcp.c | tcp_connect_reset 兩側各加一行（next: rcv_collapse_credit、xanmod: fast_ack_mode），都保留 |
| net/ipv4/tcp_input.c | 保留 next 的 rcv_collapse_credit 崩塌閘門結構，其前插入 xanmod 的 tcp_collapse_max_bytes 前置檢查（goto do_not_collapse，標籤 next 仍有）；tcp_collapse 呼叫採 next 的 6 參簽名（next 已移除 end 參數）。其餘 xanmod hunk（tcp_ca_wants_ce_events、skb_marked_lost、tcp_stamp32_us_delta、tx.in_flight）3way 乾淨併入 |
| net/ipv4/tcp_output.c | xanmod 側僅舊式寫法（min_t 無下限、無 READ_ONCE），全取 next 的 clamp_t+READ_ONCE 版本；無實質 xanmod 意圖損失 |
| net/ipv4/tcp_bbr.c | 全取 xanmod BBRv3（2418 行，.tso_segs/kfunc 註冊與 next 新 ops 介面本就相容）；移植 next 的 mss_now==0 防護（bbr_min_tso_segs + bbr_min_tso_rate 常數），避免 BPF 呼叫路徑除零 |
| mm/vmscan.c + mm/vmscan.h | xanmod workingset protection 全套移植：scan_control 三個 bitfield（vmscan.h，next 已把 scan_control 搬進該標頭）、sysctl 變數、vm_swappiness 60→10、shrink_folio_list/get_scan_count 保護、shrink_one/shrink_folios 呼叫點。next 已把 MGLRU get_type_to_scan 重構為 lru_gen_balance_scan，原 hunk 7/8 無法直套，改以等價語意移植：低於水位的類型在 lru_gen_balance_scan 內歸零不掃（clean_below_min→保護 file、anon_below_min→保護 anon、clean_below_low→強掃 anon） |

## 不移植的 7 檔（binder）

next 已把整個 C binder（binder.c/binder_alloc.c/binder_alloc.h/binder_internal.h/
binderfs.c）刪除，改為 Rust binder（drivers/android/binder/，commit fd2bc059ed466
「binder: rm -f binder.c」）。xanmod 對這批檔的修改（C binder 模組化 tristate、
module_init/MODULE 宏、debug_mask 改名、IS_ENABLED 修正、ipc_ns API）全部以
C binder 為前提，無移植對象 → 這 7 檔入 skip 清單直接跳過，保留 next 的
Rust binder。Kconfig/Makefile 的 xanmod 條目同樣針對 C binder，一併不移植。

## 驗證

- next-adapted.patch 對乾淨 next 樹 `patch -Np1 --fuzz=0` 全 hunk 成功（rc=0）。
- 以 x86_64_defconfig+baseconfig 合併配置，mm/vmscan.o、mm/util.o、
  net/ipv4/tcp_bbr.o、tcp_input.o、tcp_output.o、tcp.o、bpf_tcp_ca.o、
  kernel/sysctl.o 編譯驗證（見 commit 訊息）。
