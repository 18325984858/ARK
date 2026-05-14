#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
scrape_vergilius.py

�ݹ�ץȡ vergiliusproject.com �µ� x64 �ں˽ṹ��ҳ�棬��ȡ�����ַ��ƫ�� / sizeof / ��Աƫ�ƣ�
����� gen_offset_defaults.py ���ܽ��յ� JSON ��ʽ��

�÷���
    python tools/scrape_vergilius.py --os windows-11 --version 21h2 --out ver_w11_21h2.json
    python tools/scrape_vergilius.py --os windows-11 --version 21h2 --structs _EPROCESS _ETHREAD _KPCR --out partial.json
    python tools/scrape_vergilius.py --all --out all.json     # ץȡ�����Ѻ�OS����汾(������������)

�����������Ƽ�����ʵ�ֵ�ָ�
    pip install requests beautifulsoup4

����Σ�
    1. ��ٴ�ץȡ�����ܱ� vergiliusproject �� ToS ��ֹ�����ÿ�����������������Ӧ��Ϊ private/personal��
    2. �ű�����Ĭ�����٣���һ�����л����� 1.0 �룬��Ƶ�ʸ� 503/HTTP �������� 2 ��󼴲������²�ѯ��
    3. ץȡ�����Զ�����ǰ build �ŵ�ӳ��� Win10 / Win11 / Server ת��Ϊ NT build dwBuildNumber ��
       Microsoft һ�¶�Ӧ��(��ע vergiliusproject ÿ�����ϱ仯)��
    4. Vergiliusproject �ṹ��ҳ��ʹ�� // 0xNN ע��ƫ�ƣ�ֱ�ӽ�����Ҫ�����ⲿ�����������ϡ�

�����ʽ��Ϊ�б����ÿ��ʵ�����¶����͵Ķ���
    [
      { "build": 22000, "module": "ntoskrnl", "struct": "_EPROCESS",
        "size": 4144,
        "fields": { "UniqueProcessId": 1088, "ActiveProcessLinks": 1096, ... } }
    ]
"""

import argparse
import json
import re
import sys
import time
from typing import Dict, Iterable, List, Optional, Tuple

try:
    import requests
except ImportError:
    print("ERROR: please pip install requests", file=sys.stderr)
    sys.exit(2)

BASE = "https://www.vergiliusproject.com"
UA = "Mozilla/5.0 (MyPCHunter64 offset scraper; personal use; +https://github.com/yourrepo)"

# vergiliusproject ��"ϵͳ��Ŀ¼��"�� NT build �ŵ�ӳ�䣬��Ҫʱ�ɱ��θ��»򲹳䡣
# ��һ�����˷�����ļ��� URL ��ʶ�����ǵ�ӳ�䱸�����������Ӧ�� Microsoft NT �ں�
# dwBuildNumber (���� FileVersion ֮��ֵ).
OS_VERSION_TO_BUILD: Dict[Tuple[str, str], int] = {
    # Windows 10
    ("windows-10", "1507"): 10240,
    ("windows-10", "1511"): 10586,
    ("windows-10", "1607"): 14393,
    ("windows-10", "1703"): 15063,
    ("windows-10", "1709"): 16299,
    ("windows-10", "1803"): 17134,
    ("windows-10", "1809"): 17763,
    ("windows-10", "1903"): 18362,
    ("windows-10", "1909"): 18363,
    ("windows-10", "2004"): 19041,
    ("windows-10", "20h2"): 19042,
    ("windows-10", "21h1"): 19043,
    ("windows-10", "21h2"): 19044,
    ("windows-10", "22h2"): 19045,
    # Windows 11
    ("windows-11", "insider-preview-jun-2021"): 21996,
    ("windows-11", "21h2"): 22000,
    ("windows-11", "22h2"): 22621,
    ("windows-11", "23h2"): 22631,
    ("windows-11", "24h2"): 26100,
    ("windows-11", "25h2"): 26200,
    # Server 2022
    ("server-2022", "21h2"): 20348,
    # Windows 8 / 8.1
    ("windows-8", "rtm"): 9200,
    ("windows-8.1", "rtm"): 9600,
    ("windows-8.1", "update-1"): 9600,
    # Windows 7
    ("windows-7", "rtm"): 7600,
    ("windows-7", "sp1"): 7601,
}

# ֧�ֵ� OS ��Ŀ��vergiliusproject ��ҳ��Ŀ¼��һ�£���
SUPPORTED_OS = [
    "windows-11",
    "windows-10",
    "server-2022",
    "windows-8.1",
    "windows-8",
    "windows-7",
    "windows-vista",
    "windows-xp",
]


def fetch(url: str, sess: requests.Session, retries: int = 2, throttle: float = 1.0) -> Optional[str]:
    """������ʡ�ʧ�ܷ��� None��"""
    for attempt in range(retries + 1):
        time.sleep(throttle)
        try:
            r = sess.get(url, timeout=20)
        except requests.RequestException as e:
            print(f"[warn] {url} request error: {e}", file=sys.stderr)
            continue
        if r.status_code == 200:
            return r.text
        if r.status_code in (429, 503) and attempt < retries:
            print(f"[warn] {url} HTTP {r.status_code}, backing off", file=sys.stderr)
            time.sleep(5.0 * (attempt + 1))
            continue
        print(f"[warn] {url} HTTP {r.status_code}", file=sys.stderr)
        return None
    return None


_HREF_RE = re.compile(r'href="([^"]+)"')

# ƥ�� vergiliusproject "// 0x10 bytes (sizeof)" ע��
_SIZEOF_RE = re.compile(r"//\s*0x([0-9a-fA-F]+)\s*bytes?\s*\(sizeof\)", re.IGNORECASE)

# ƥ�����: "    TYPE Name;        // 0x40"  ��ǰ�����Ϳ��ܸܺ��ӣ��Ŵ�����������������ƫ�ơ�
# ��ȡ�����һ��"��ʶ�� ��ѡ ��С����] ;  // 0xNN"��
# һ�����Ұ�ȫ��������ץ Name + offset��
_FIELD_RE = re.compile(
    r"^\s*(?:struct\s+|union\s+|enum\s+)?[\w_:<>\*\s,\(\)\[\]]+?"
    r"\b(?P<name>[A-Za-z_]\w*)\s*"
    r"(?:\[[^\]]*\])?\s*"
    r"(?::\s*\d+\s*)?"           # ��λ��(bitfield)
    r";\s*//\s*0x(?P<off>[0-9a-fA-F]+)",
    re.MULTILINE,
)


def parse_struct_page(html: str) -> Tuple[Optional[int], Dict[str, int]]:
    """�� vergiliusproject �ṹ��ҳ�滻ȡ sizeof �����Ա��ӳ�䡣"""
    size = None
    m = _SIZEOF_RE.search(html)
    if m:
        try:
            size = int(m.group(1), 16)
        except ValueError:
            size = None

    fields: Dict[str, int] = {}
    for fm in _FIELD_RE.finditer(html):
        name = fm.group("name")
        off = int(fm.group("off"), 16)
        if name in fields:
            # ͬ��ƫ�Ƴ�����Σ�union �ȣ�����������ǰ��һ�����ɣ�
            continue
        fields[name] = off
    return size, fields


def list_struct_names_on_version(os_slug: str, ver_slug: str, sess: requests.Session, throttle: float) -> List[str]:
    """ץȡ�汾������ҳ�е����� Structures/Enums/Unions ���õ�����ʵ�����ơ�"""
    url = f"{BASE}/kernels/x64/{os_slug}/{ver_slug}"
    html = fetch(url, sess, throttle=throttle)
    if not html:
        return []

    prefix = f"/kernels/x64/{os_slug}/{ver_slug}/"
    names = []
    seen = set()
    for href in _HREF_RE.findall(html):
        if href.startswith(prefix):
            n = href[len(prefix):]
            # ���ܻ�����ҳ�ڵ� URL/Ƭ��
            if "/" in n or "#" in n or not n:
                continue
            # URL ����
            try:
                import urllib.parse
                n = urllib.parse.unquote(n)
            except Exception:
                pass
            if n not in seen:
                seen.add(n)
                names.append(n)
    return names


def scrape_version(
    os_slug: str,
    ver_slug: str,
    structs_filter: Optional[List[str]],
    sess: requests.Session,
    throttle: float,
) -> List[dict]:
    build = OS_VERSION_TO_BUILD.get((os_slug, ver_slug))
    if build is None:
        print(f"[warn] no build number mapping for {os_slug}/{ver_slug}, using 0", file=sys.stderr)
        build = 0

    names = list_struct_names_on_version(os_slug, ver_slug, sess, throttle)
    if not names:
        print(f"[warn] no structures listed for {os_slug}/{ver_slug}", file=sys.stderr)
        return []

    if structs_filter:
        wanted = set(structs_filter)
        names = [n for n in names if n in wanted]

    print(f"[info] {os_slug}/{ver_slug}: {len(names)} entries to fetch", file=sys.stderr)

    out: List[dict] = []
    for i, name in enumerate(names, 1):
        url = f"{BASE}/kernels/x64/{os_slug}/{ver_slug}/{name}"
        html = fetch(url, sess, throttle=throttle)
        if not html:
            continue
        size, fields = parse_struct_page(html)
        if not fields and size is None:
            continue
        entry = {
            "build": build,
            "module": "ntoskrnl",
            "struct": name,
        }
        if size is not None:
            entry["size"] = size
        if fields:
            entry["fields"] = fields
        out.append(entry)
        if i % 25 == 0:
            print(f"  [{i}/{len(names)}] last={name}", file=sys.stderr)
    return out


def list_versions(os_slug: str, sess: requests.Session, throttle: float) -> List[str]:
    url = f"{BASE}/kernels/x64/{os_slug}"
    html = fetch(url, sess, throttle=throttle)
    if not html:
        return []
    prefix = f"/kernels/x64/{os_slug}/"
    versions = []
    seen = set()
    for href in _HREF_RE.findall(html):
        if href.startswith(prefix):
            v = href[len(prefix):]
            if "/" in v or "#" in v or not v:
                continue
            if v not in seen:
                seen.add(v)
                versions.append(v)
    return versions


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--os", help="OS slug, e.g. windows-11")
    ap.add_argument("--version", help="version slug, e.g. 21h2")
    ap.add_argument("--structs", nargs="*", help="optional whitelist of structure names")
    ap.add_argument("--all", action="store_true", help="scrape ALL supported OS / all versions (long & heavy)")
    ap.add_argument("--throttle", type=float, default=1.0, help="seconds between requests (default 1.0)")
    ap.add_argument("--out", required=True, help="output JSON path")
    args = ap.parse_args()

    sess = requests.Session()
    sess.headers["User-Agent"] = UA

    all_out: List[dict] = []
    if args.all:
        for osname in SUPPORTED_OS:
            print(f"[info] discovering versions for {osname}", file=sys.stderr)
            versions = list_versions(osname, sess, args.throttle)
            for v in versions:
                all_out.extend(scrape_version(osname, v, args.structs, sess, args.throttle))
    else:
        if not args.os or not args.version:
            ap.error("--os and --version are required unless --all is used")
        all_out.extend(scrape_version(args.os, args.version, args.structs, sess, args.throttle))

    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(all_out, f, ensure_ascii=False, indent=2)
    print(f"[done] wrote {len(all_out)} entries to {args.out}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
