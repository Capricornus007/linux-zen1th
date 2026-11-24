#!/usr/bin/env python3
"""linux-zen1th release 管理：current 只留最新一对，历史按版本 tag 归档。

用法（env: GITHUB_TOKEN / GITHUB_REPOSITORY / GH_API）：
  release-manage.py clean-current          # 删除 current-kernel release 的所有 assets
  release-manage.py archive <tag> <files...>  # 建/更新 <tag> release 并上传 files

用 curl + Python 标准库，不依赖 gh / jq（容器是 archlinux/base）。
"""
import os, sys, json, subprocess, urllib.parse

TOKEN = os.environ.get("GITHUB_TOKEN", "")
REPO = os.environ.get("GITHUB_REPOSITORY", "Capricornus007/linux-zen1th")
API = os.environ.get("GH_API", "https://api.github.com")


def api(method, path, data=None):
    """调 GitHub REST API；返回 (status, parsed_body)。"""
    cmd = ["curl", "-sS", "-o", "/tmp/_gh_body", "-w", "%{http_code}",
           "-X", method, "-H", "Authorization: Bearer " + TOKEN,
           "-H", "Accept: application/vnd.github+json",
           API + path]
    if data is not None:
        cmd += ["-H", "Content-Type: application/json", "-d", json.dumps(data)]
    code = subprocess.run(cmd, capture_output=True, text=True).stdout.strip()
    try:
        body = json.load(open("/tmp/_gh_body"))
    except Exception:
        body = open("/tmp/_gh_body").read()
    return int(code or 0), body


def get_release(tag):
    c, b = api("GET", f"/repos/{REPO}/releases/tags/{urllib.parse.quote(tag)}")
    return b if c == 200 else None


def delete_asset(asset_id):
    c, _ = api("DELETE", f"/repos/{REPO}/releases/assets/{asset_id}")
    return c in (200, 204)


def upload_asset(release_id, path):
    name = os.path.basename(path)
    url = f"https://uploads.github.com/repos/{REPO}/releases/{release_id}/assets?name={urllib.parse.quote(name)}"
    cmd = ["curl", "-sS", "-o", "/tmp/_gh_up", "-w", "%{http_code}",
           "-X", "POST", "-H", "Authorization: Bearer " + TOKEN,
           "-H", "Content-Type: application/octet-stream",
           "--data-binary", "@" + path, url]
    code = subprocess.run(cmd, capture_output=True, text=True).stdout.strip()
    return int(code or 0) in (200, 201), open("/tmp/_gh_up").read()[:200]


def main():
    if len(sys.argv) < 2:
        print("usage: release-manage.py clean-current | archive <tag> <files...>")
        sys.exit(2)
    action = sys.argv[1]

    if action == "clean-current":
        rel = get_release("current-kernel")
        if not rel:
            print("current-kernel release 不存在，无需清理")
            return
        assets = rel.get("assets", [])
        print(f"current-kernel 现有 {len(assets)} 个资源，开始清理…")
        ok = 0
        for a in assets:
            if delete_asset(a["id"]):
                ok += 1
                print(f"  删除 {a['name']}")
            else:
                print(f"  ::error::删除失败 {a['name']}")
        print(f"清理完成：{ok}/{len(assets)}")
        if ok != len(assets):
            sys.exit(1)

    elif action == "archive":
        if len(sys.argv) < 4:
            print("archive 需要 tag 和至少一个文件")
            sys.exit(2)
        tag = sys.argv[2]
        files = sys.argv[3:]
        for f in files:
            if not os.path.isfile(f):
                print(f"::error::文件不存在：{f}")
                sys.exit(1)
        rel = get_release(tag)
        if not rel:
            c, b = api("POST", f"/repos/{REPO}/releases",
                       {"tag_name": tag, "name": f"linux-zen1th {tag}",
                        "body": f"历史归档：{tag}。current 只保留最新一对。",
                        "make_latest": "false"})
            if c != 201:
                print(f"::error::建 release {tag} 失败（{c}）：{b}")
                sys.exit(1)
            rel = b
            print(f"新建归档 release：{tag}")
        else:
            print(f"归档 release {tag} 已存在，追加文件")
        rid = rel["id"]
        for f in files:
            name = os.path.basename(f)
            # 2026-11-25 修幂等：重跑同一版本時 archive tag 已存在、同名 asset 也在，
            # 直接上傳會 422 already_exists 讓整個 build 紅（run 45475856476 實測）。
            # 先刪同名舊 asset 再傳，與 clean-current 的「只留最新」語意一致。
            for a in rel.get("assets", []):
                if a.get("name") == name:
                    delete_asset(a["id"])
                    print(f"  刪同名舊 asset：{name}")
            ok, msg = upload_asset(rid, f)
            print(f"  上传 {name}: {'OK' if ok else 'FAIL ' + msg}")
            if not ok:
                sys.exit(1)
            # 上傳成功後同步本地 assets 清單，避免同名多份殘留判斷失效。
            rel = get_release(tag)
    else:
        print(f"未知动作：{action}")
        sys.exit(2)


if __name__ == "__main__":
    main()
