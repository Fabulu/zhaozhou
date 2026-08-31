from pathlib import Path
import hashlib
import time
import urllib.error
import urllib.request
import uuid

PUBLIC = Path(r"C:/programmieren/zencrifice/zhaozhou/runs/CLAUDE-RUNS/RUN-20260829-2226-zixxtrixx-v10-lighting-spring-repair/work/Upheaval-v10/website/public")
ROOTS = [
    ("deployment", "https://606f25e1.upheaval.pages.dev"),
    ("production", "https://upheaval.pages.dev"),
]
PATHS = [
    "index.html",
    "renders/zixxtrixx-knockdown.webm",
    "renders/zixxtrixx-knockdown.png",
    "renders/zixxtrixx-jump-one.webm",
    "renders/zixxtrixx-jump-one.png",
    "renders/archive-2026-08-29-v9-cel-main-knockdown.webm",
    "renders/archive-2026-08-29-v9-cel-main-knockdown.png",
]
ROBOTS = '<meta name="robots" content="noindex, nofollow">'
V10_MARKER = "V10&#x27;s lighting transforms and blends the actual skinned normal"
V9_MARKER = "v9 cel-main · 2026-08-29"


def fetch(url):
    request = urllib.request.Request(
        url + ("&" if "?" in url else "?") + "cachebust=" + uuid.uuid4().hex,
        headers={
            "Cache-Control": "no-cache, no-store, max-age=0",
            "Pragma": "no-cache",
            "User-Agent": "zixxtrixx-v10-production-verifier/1",
        },
    )
    with urllib.request.urlopen(request, timeout=60) as response:
        return response.status, response.geturl(), response.headers.get("Content-Type", ""), response.read()


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    local = {path: (PUBLIC / path).read_bytes() for path in PATHS}
    all_ok = True
    for label, root in ROOTS:
        fetched = None
        error = None
        for attempt in range(1, 13):
            try:
                status, final_url, content_type, data = fetch(root + "/index.html")
                if data == local["index.html"]:
                    fetched = (status, final_url, content_type, data, attempt)
                    break
                error = f"index mismatch local={sha(local['index.html'])} remote={sha(data)}"
            except Exception as exc:
                error = repr(exc)
            time.sleep(3)
        if fetched is None:
            print(f"{label}: FAIL after propagation retries: {error}")
            all_ok = False
            continue

        status, final_url, content_type, index, attempt = fetched
        text = index.decode("utf-8")
        video_count = text.count("<video ")
        image_count = text.count('<img src="renders/')
        index_ok = (
            status == 200
            and index == local["index.html"]
            and text.count(ROBOTS) == 1
            and V10_MARKER in text
            and V9_MARKER in text
            and video_count == 115
            and image_count == 2
        )
        print(
            f"{label}: index status={status} attempt={attempt} bytes={len(index)} "
            f"sha256={sha(index)} exact_local={index == local['index.html']} "
            f"robots_exact_count={text.count(ROBOTS)} v10_marker={V10_MARKER in text} "
            f"v9_marker={V9_MARKER in text} video_count={video_count} "
            f"image_count={image_count} render_entry_count={video_count + image_count} "
            f"content_type={content_type!r} final_url={final_url!r}"
        )
        all_ok &= index_ok

        for path in PATHS[1:]:
            try:
                status, final_url, content_type, data = fetch(root + "/" + path)
                equal = data == local[path]
                ok = status == 200 and equal
                all_ok &= ok
                print(
                    f"{label}: {path} status={status} bytes={len(data)} "
                    f"sha256={sha(data)} exact_local={equal} "
                    f"content_type={content_type!r} final_url={final_url!r}"
                )
            except Exception as exc:
                all_ok = False
                print(f"{label}: {path} FAIL {exc!r}")

    print(
        "PRODUCTION VERIFY: "
        + ("PASS" if all_ok else "FAIL")
        + " — deployment alias and production index/media are cache-bypassed, byte-identical to local; exact noindex/nofollow and v10/v9 declarations present"
    )
    raise SystemExit(0 if all_ok else 1)


if __name__ == "__main__":
    main()
