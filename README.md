# linux-zen1th

自用的最尖端內核：**linux-next 尖端 ⊕ BORE(CachyOS) ⊕ zen-kernel ⊕ xanmod ⊕ cjktty**，
編譯目標 **x86-64-v3**，模組按本機硬件精簡（localmodconfig + keep.fragment）。

與 `Capricornus007/linux-cachyos-cjktty`（7.x stable、CachyOS 腳本式補丁）**互不相干、各自存在**：
那位是 cachyos+cjktty，這裡是 next 全疊加棧。

## 結構

| 分支/檔案 | 用途 |
|---|---|
| `master` | 中樞：PKGBUILD、config、keep.fragment、lsmod.list、workflows |
| `zen1th` | **整合分支**（merge-bot 自動維護）：next ⊕ zen ⊕ xanmod ⊕ BORE ⊕ cjktty 的實際源码 |
| `pkgbuild/PKGBUILD` | 直接 `makepkg` 整合分支即可出包（Arch 包或手動 install 均可） |
| `config/zen1th-x86_64.baseconfig` | 底 config（＝現機 7.3.1-cachyos-cjktty 實測可用版） |
| `config/keep.fragment` | localmodconfig 裁剪後強制補回的硬需求（NVME=y/EXT4=y/vfat/BTF/dae 16 項/RTL8852AE…） |
| `ci/lsmod.list` | 本機 `lsmod` 實錄，給 localmodconfig 當白名單 |

## 流程（全自動）

1. `merge-bot.yml` 每日 09:00（UTC+8）：整合分支 fetch linux-next 尖端做 `git merge`；
   首次/重建時依序合 zen-kernel master、xanmod 7.3 分支、CachyOS BORE 補丁、cjktty 補丁。
   **任何衝突 = workflow 紅 + 衝突檔案清單留檔**，絕不假裝成功。
2. 合併成功 → repository_dispatch 觸發 `build.yml`：archlinux 容器 `makepkg`，
   產物自檢（vmlinuz/amdgpu.ko/rtw89_8852ae.ko/config NVME=y 全在包內，缺一即紅），
   發佈到 `current-kernel` release + SHA256SUMS。
3. 本機 `pacman -U linux-zen1th-*.pkg.tar.zst`（MLFS feed 化留後續）。

## 版本

`pkgver()` 用 `git describe`（如 `7.4.rc1.g<sha>`），本地手動構建即可复現。
本地構建：`git clone -b zen1th … && makepkg -fs`。

## 已知代價（誠實記錄）

- BORE/zen/xanmod 合併區（kernel/sched、Kconfig、mm/）有衝突風險：merge-bot 會紅，
  修復=在整合分支上手動解一次（bot 之後只跟 next 新提交走，不會再撞舊衝突）。
- xanmod 的 7.3 分支若停更，重建時改用其當前 HEAD 分支（改 merge-bot 一行）。
- cjktty 補丁按主版本（v7.x）：next 大版本跳動時需新補丁，衝突即紅提醒。
