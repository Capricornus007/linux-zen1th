# Maintainer: Capricornus007 <sihaogang at gmail dot com>
# linux-zen1th —— 一切去尖端：
#   基座 = linux-next（torvalds next 尖端，merge-bot 每日合入；補丁全部已合入整合分支）
#   ⊕ BORE（CachyOS sched 補丁）⊕ zen-kernel ⊕ xanmod ⊕ cjktty
#   編譯目標 znver3（用戶 CPU：Ryzen 5 5600U / Zen 3，2026-11-09 定案；
#                     lfs-pacman 的 91 包 64/32 位也是同一個目標）
#   模組精簡 = localmodconfig（ci/lsmod.list）＋ config/keep.fragment 硬需求補回
# 來源 = 本倉 zen1th 整合分支（merge-bot.yml 維護，衝突即紅、絕不假裝成功）
_pkgbase=linux-zen1th
pkgbase=$_pkgbase
pkgname=("$_pkgbase" "$_pkgbase-headers")
_RepoOwner=Capricornus007
_integration_branch=zen1th
pkgver=0.r0.g0000000
pkgrel=1
pkgdesc="The Linux -next kernel with BORE, Zen, XanMod and cjktty optimizations; znver3, hw-trimmed"
arch=(x86_64)
url="https://github.com/$_RepoOwner/linux-zen1th"
license=(GPL2)
makedepends=(git bc flex bison openssl python perl rsync cpio kmod zstd)
options=(!debug)
source=("git+https://github.com/$_RepoOwner/linux-zen1th.git#branch=$_integration_branch")
sha256sums=(SKIP)
_srcname=linux-zen1th

pkgver() {
  cd "$srcdir/$_srcname"
  # zen1th 是無上游歷史的快照分支（見 merge-bot.yml：整包推 next 史會 500），
  # describe/rev-list --count 都失去意義。版本用「核心版本.r<UTC日期>.g<快照sha>」：
  # 每日構建自然遞增、跨快照鏈重開也單調，pacman 升級排序安全。
  # .scm_version 存整合時的真實 next commit（追溯用，不進版本號）。
  local kv
  kv=$(make -s kernelversion | tr -d '-')
  printf '%s.r%s.g%s' "$kv" "$(date -u +%Y%m%d)" "$(git rev-parse --short HEAD)"
}

prepare() {
  cd "$_srcname"
  echo "=== zen1th prepare @ $(git rev-parse --short HEAD) ==="
  echo "-zen1th" > localversion.20-pkgname

  echo "--- config：用戶機底 + localmodconfig 精簡 + KEEP 補回 + znver3 ---"
  cp config/zen1th-x86_64.baseconfig .config
  make LSMOD=ci/lsmod.list localmodconfig
  local opt line
  while IFS= read -r line; do
    case "$line" in
      CONFIG_*=y)  opt=${line#CONFIG_}; opt=${opt%%=y}; scripts/config -e "$opt";;
      CONFIG_*=m)  opt=${line#CONFIG_}; opt=${opt%%=m}; scripts/config -m "$opt";;
      '# CONFIG_'*' is not set') opt=${line#'# CONFIG_'}; opt=${opt%% is not set}; scripts/config -d "$opt";;
    esac
  done < config/keep.fragment
  # znver3（2026-11-09 用戶定案：內核與 lfs-pacman 的 91 包一起對準本機 Zen 3）。
  # 官方 linux-next arch/x86/Kconfig.cpu 實查：CPU 選項只有 X86_NATIVE_CPU，
  # 沒有 MZEN3，也沒有 X86_64_VERSION（後者是 CachyOS 補丁；scripts/config 對
  # 不存在的選項靜默丟行，run 42441427280 已實錘）。唯一真路＝改
  # arch/x86/Makefile 的 else（非 native）分支，把 generic 的 march/tune 與
  # rust target-cpu/tune-cpu 換成 znver3。
  # X86_NATIVE_CPU 必關：CI runner 不是本機，開 native 會編出 runner 專用指令集
  # （可能是 AVX-512），裝到 Zen 3 上直接非法指令、開不了機。
  # kernel 自帶 -mno-sse/-mno-mmx/-mno-sse2/-mno-avx/-mno-sse4a，
  # -march=znver3 只影響調度與整數指令，不會讓內核用到向量暫存器。
  scripts/config -d MZEN4 -d X86_NATIVE_CPU
  sed -i 's/-march=x86-64 -mtune=generic/-march=znver3 -mtune=znver3/; s/-Ctarget-cpu=x86-64 /-Ctarget-cpu=znver3 /; s/-Ztune-cpu=generic/-Ztune-cpu=znver3/' arch/x86/Makefile
  scripts/config -e SCHED_BORE -e CACHY -e IKCONFIG -e IKCONFIG_PROC -e DEBUG_INFO_BTF
  make olddefconfig

  echo "--- 開機硬項自檢（任一不符即停，絕不產出開不了機的包）---"
  local o
  for o in CONFIG_BLK_DEV_NVME=y CONFIG_EXT4_FS=y CONFIG_VFAT_FS=y CONFIG_EXFAT_FS=y \
           CONFIG_BPF_SYSCALL=y CONFIG_DEBUG_INFO_BTF=y "CONFIG_TUN=m" \
           "# CONFIG_X86_NATIVE_CPU is not set" CONFIG_SCHED_BORE=y CONFIG_DRM_AMDGPU=m \
           CONFIG_RTW89_8852AE=m CONFIG_IKCONFIG=y \
           CONFIG_USB_NET_DRIVERS=m CONFIG_USB_USBNET=m CONFIG_USB_NET_CDCETHER=m \
           CONFIG_USB_NET_CDC_NCM=m CONFIG_USB_NET_CDC_MBIM=m CONFIG_USB_NET_RNDIS_HOST=m \
           "CONFIG_DEFAULT_TCP_CONG=\"bbr\"" CONFIG_FONT_CJK_16x16=y CONFIG_FONT_CJK_32x32=y; do
    grep -qx "$o" .config || { echo "config 自檢失敗：$o"; exit 1; }
  done
  # znver3 落地自檢（Kconfig 無對應選項，只能查 Makefile 本身）
  grep -q '\-march=znver3 -mtune=znver3' arch/x86/Makefile || { echo "自檢失敗：arch/x86/Makefile 無 znver3 march"; exit 1; }
}

build() {
  cd "$_srcname"
  make -j"${JOBS:-4}"
}

package_linux-zen1th() {
  depends=(coreutils kmod)
  optdepends=('linux-zen1th-headers: headers for external modules')
  provides=("linux" "kernel")
  conflicts=(linux)
  cd "$_srcname"
  local release; release=$(make -s kernelrelease)
  make INSTALL_MOD_PATH="$pkgdir" INSTALL_MOD_STRIP=1 modules_install
  # 不用 make install：7.4 的 scripts/install.sh 走 arch/x86/boot/install.sh，
  # 產出「無版本後綴」的 boot/vmlinuz 且結尾試圖跑 LILO（run 42443660615 實測，
  # 自检找 vmlinuz-* 找不到直接紅）。bzImage 直接擺放、帶版本名，GRUB 路徑固定。
  install -D -m644 arch/x86/boot/bzImage "$pkgdir/boot/vmlinuz-$release"
  install -D -m644 System.map "$pkgdir/boot/System.map-$release"
  install -D -m600 .config "$pkgdir/boot/config-$release"
}

package_linux-zen1th-headers() {
  pkgdesc="Headers and scripts for $pkgbase kernel"
  depends=(elfutils)
  provides=("linux-headers")
  cd "$_srcname"
  local release; release=$(make -s kernelrelease)
  make headers_install INSTALL_HDR_PATH="$pkgdir/usr"
  # DKMS／外部模組需要的是「已 configure 的內核編譯樹」，不是只有 UAPI headers。
  # 之前只裝 usr/include，NVIDIA DKMS 找不到 include/generated、Module.symvers、
  # scripts 與 arch/x86，必然編不過。這裡照 Arch linux-headers 的佈局安裝：
  #   /usr/src/linux-headers-<release>/
  #   /usr/lib/modules/<release>/build -> 上述目錄
  install -d "$pkgdir/usr/src/linux-headers-$release"
  tar -cf - \
    --exclude='.git' --exclude='*.o' --exclude='*.ko' --exclude='*.ko.*' \
    --exclude='*.cmd' --exclude='*.a' --exclude='*.mod' --exclude='*.mod.c' \
    --exclude='modules.order' --exclude='.tmp_*' \
    --exclude='vmlinux' --exclude='vmlinux.o' --exclude='System.map' \
    --exclude='arch/x86/boot/bzImage' \
    Makefile Kconfig .config Module.symvers arch block certs crypto drivers fs \
    init io_uring kernel lib mm net rust samples scripts security sound tools usr virt \
    | tar -xf - -C "$pkgdir/usr/src/linux-headers-$release"
  install -d "$pkgdir/usr/lib/modules/$release"
  ln -sfn "/usr/src/linux-headers-$release" "$pkgdir/usr/lib/modules/$release/build"
  ln -sfn "/usr/src/linux-headers-$release" "$pkgdir/usr/lib/modules/$release/source"
}
# vim:set ts=2 sw=2 et:
