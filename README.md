# linux-zen1th

自用的最尖端內核：**linux-next 尖端 ⊕ BORE(CachyOS) ⊕ zen-kernel ⊕ xanmod ⊕ lily(archlinuxcn) ⊕ cjktty**，
（lily＝archlinuxcn linux-lily：無源碼補丁，config 層修改——BBR 默認＋CJK 字體，已併入 keep.fragment 並進 PKGBUILD 自檢）
編譯目標 **x86-64-v3**，模組按本機硬件精簡（localmodconfig + keep.fragment）。

與 `Capricornus007/linux-cachyos-cjktty`（7.x stable、CachyOS 腳本式補丁）**互不相干、各自存在**：
那位是 cachyos+cjktty，這裡是 next 全疊加棧。

## 結構

| 分支/檔案 | 用途 |
|---|---|
| `master` | 中樞：PKGBUILD、config、keep.fragment、lsmod.list、workflows |
| `zen1th` | **整合快照分支**（merge-bot 每日全量重組）：next ⊕ zen ⊕ xanmod ⊕ BORE ⊕ cjktty 的實際源码＋根目錄 PKGBUILD/config/ci。**無上游歷史**（單提交鏈、≤30 代）——270 萬 commit 的 next 史整包推 GitHub 必 500（run #16–#19 實證），真實 next commit 記在 `.scm_version` |
| `pkgbuild/PKGBUILD` | 直接 `makepkg` 整合分支即可出包（Arch 包或手動 install 均可） |
| `config/zen1th-x86_64.baseconfig` | 底 config（＝現機 7.3.1-cachyos-cjktty 實測可用版） |
| `config/keep.fragment` | localmodconfig 裁剪後強制補回的硬需求（NVME=y/EXT4=y/vfat/BTF/dae 16 項/RTL8852AE…） |
| `ci/lsmod.list` | 本機 `lsmod` 實錄，給 localmodconfig 當白名單 |

## 流程（全自動）

1. `merge-bot.yml` 每日 09:00（UTC+8）全量重組：取 next 尖端，依 merge-base 算
   zen/xanmod **淨差量**逐檔 3way 套用（xanmod 漂移屬常態：衝突檔回退保留基座版＋
   清單推 `zen1th-conflicts` 分支＋::warning::；zen 差量衝突則必紅），
   再打 BORE（required）與 cjktty（required，衝突必紅）；快照樹硬校驗
   PKGBUILD/.scm_version 在位後推單提交快照。
2. 合併成功 → repository_dispatch 觸發 `build.yml`：archlinux 容器 `makepkg`，
   產物自檢（vmlinuz/amdgpu.ko/rtw89_8852ae.ko/config NVME=y 全在包內，缺一即紅），
   發佈到 `current-kernel` release + SHA256SUMS。
3. 本機 `pacman -U linux-zen1th-*.pkg.tar.zst`（MLFS feed 化留後續）。

## 版本

`pkgver()`＝`核心版本.r<UTC日期>.g<快照sha>`（快照分支無 tag/無歷史，describe 不可用），
例如 `7.4.0rc1.r20261108.g83f0895`。每日自然遞增、pacman 升級排序安全。
本地構建：`git clone -b zen1th … && makepkg -fs`（倉是 private，帶 token）。

## 已知代價（誠實記錄）

- BORE/zen/xanmod 合併區（kernel/sched、Kconfig、mm/）有衝突風險：merge-bot 會紅，
  修復=在整合分支上手動解一次（bot 之後只跟 next 新提交走，不會再撞舊衝突）。
- xanmod 的 7.3 分支若停更，重建時改用其當前 HEAD 分支（改 merge-bot 一行）。
- cjktty 補丁按主版本（v7.x）：next 大版本跳動時需新補丁，衝突即紅提醒。
