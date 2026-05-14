"""List vergiliusproject Windows kernel versions per OS family."""
import re
import sys
import requests

UA = "MyPCHunter64 offset scraper (personal use)"
BASE = "https://www.vergiliusproject.com"

OS_SLUGS = [
    "windows-11",
    "windows-10",
    "server-2022",
    "windows-8.1",
    "windows-8",
    "windows-7",
]


def main() -> int:
    s = requests.Session()
    s.headers["User-Agent"] = UA
    for osname in OS_SLUGS:
        url = f"{BASE}/kernels/x64/{osname}"
        try:
            r = s.get(url, timeout=20)
        except requests.RequestException as e:
            print(f"{osname}: error {e}")
            continue
        if r.status_code != 200:
            print(f"{osname}: HTTP {r.status_code}")
            continue
        prefix = f"/kernels/x64/{osname}/"
        seen = set()
        for href in re.findall(r'href="([^"]+)"', r.text):
            if href.startswith(prefix):
                v = href[len(prefix):]
                if v and "/" not in v and "#" not in v and not v.startswith("_"):
                    seen.add(v)
        print(f"{osname}: {sorted(seen)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
