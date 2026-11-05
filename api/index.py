#!/usr/bin/env python3
"""
Vercel Serverless Function entry point for TinyURL
Bridges HTTP requests to the compiled C backend.
"""

import os
import sys
import json
import subprocess
import urllib.parse
from http.server import BaseHTTPRequestHandler

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_DIR = os.path.join(BASE_DIR, "src")
IS_VERCEL = bool(os.environ.get("VERCEL") or os.environ.get("AWS_LAMBDA_FUNCTION_NAME"))

# On Vercel Lambda, root filesystem is read-only; use /tmp for compilation output and persistence
if IS_VERCEL:
    BIN_DIR = "/tmp"
    DATA_DIR = "/tmp"
else:
    BIN_DIR = BASE_DIR
    DATA_DIR = os.path.join(BASE_DIR, "data")

EXE_NAME = "tinyurl.exe" if sys.platform == "win32" else "tinyurl"
TMP_EXE_PATH = os.path.join(BIN_DIR, EXE_NAME)
PREBUILT_EXE_PATH = os.path.join(BASE_DIR, EXE_NAME)
DATA_FILE = os.path.join(DATA_DIR, "urls.dat")


def get_c_executable():
    """Returns executable path, compiling to /tmp if necessary on Linux/Vercel."""
    os.makedirs(DATA_DIR, exist_ok=True)

    # 1. Check if executable exists in BIN_DIR
    if os.path.exists(TMP_EXE_PATH) and os.access(TMP_EXE_PATH, os.X_OK):
        return TMP_EXE_PATH

    # 2. Check if prebuilt executable exists in BASE_DIR (e.g. from buildCommand)
    if os.path.exists(PREBUILT_EXE_PATH) and os.access(PREBUILT_EXE_PATH, os.X_OK):
        return PREBUILT_EXE_PATH

    # 3. Compile using gcc on the fly
    src_files = [
        os.path.join(SRC_DIR, f)
        for f in ["main.c", "url.c", "hashmap.c", "url_service.c", "storage.c"]
    ]
    cmd = [
        "gcc",
        "-O2",
        "-Wall",
        "-Wextra",
        "-pedantic",
        "-std=c99",
        "-I" + SRC_DIR,
        *src_files,
        "-o",
        TMP_EXE_PATH,
    ]
    try:
        res = subprocess.run(cmd, cwd=BASE_DIR, capture_output=True, text=True, timeout=20)
        if res.returncode == 0:
            try:
                os.chmod(TMP_EXE_PATH, 0o755)
            except Exception:
                pass
            return TMP_EXE_PATH
    except Exception as exc:
        print(f"[TinyURL Vercel] Compilation failed: {exc}", file=sys.stderr)

    return TMP_EXE_PATH


def run_c_backend(action, *args):
    """Executes the C executable with --api and returns parsed JSON output."""
    exe_path = get_c_executable()
    cmd = [exe_path, "--api", "--data-file", DATA_FILE, action, *args]
    try:
        proc = subprocess.run(
            cmd,
            cwd=BASE_DIR,
            capture_output=True,
            text=True,
            timeout=10,
        )
    except Exception as exc:
        return {"success": False, "error": f"Failed to execute C backend: {str(exc)}"}

    output = proc.stdout.strip()
    if not output:
        err = proc.stderr.strip() or "Empty output from C backend"
        return {"success": False, "error": err}

    json_start = output.find("{")
    if json_start != -1:
        json_str = output[json_start:]
        try:
            return json.loads(json_str)
        except json.JSONDecodeError:
            pass

    return {
        "success": False,
        "error": f"Invalid JSON received from C backend: {output}",
    }


class handler(BaseHTTPRequestHandler):
    def send_json(self, status_code, payload):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status_code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, X-Requested-With")
        self.end_headers()
        self.wfile.write(body)

    def send_404_html(self, short_code):
        html = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>404 - Short URL Not Found | TinyURL</title>
  <style>
    body {{
      font-family: system-ui, -apple-system, sans-serif;
      background: #f8fafc; color: #0f172a;
      display: flex; align-items: center; justify-content: center;
      min-height: 100vh; margin: 0; padding: 1.5rem;
    }}
    .card {{
      background: #ffffff; border: 1px solid #e2e8f0; border-radius: 12px;
      padding: 2.5rem; max-width: 480px; width: 100%; text-align: center;
      box-shadow: 0 4px 6px -1px rgba(0,0,0,0.05);
    }}
    .badge {{
      display: inline-block; background: #fee2e2; color: #991b1b;
      padding: 0.25rem 0.75rem; border-radius: 9999px; font-weight: 600; font-size: 0.875rem; margin-bottom: 1rem;
    }}
    h1 {{ margin: 0 0 0.5rem 0; font-size: 1.5rem; }}
    p {{ color: #64748b; line-height: 1.5; margin: 0 0 1.5rem 0; }}
    code {{ background: #f1f5f9; padding: 0.15rem 0.4rem; border-radius: 4px; }}
    a.btn {{
      display: inline-block; background: #4f46e5; color: #ffffff;
      text-decoration: none; padding: 0.75rem 1.5rem; border-radius: 8px; font-weight: 500;
    }}
  </style>
</head>
<body>
  <div class="card">
    <div class="badge">HTTP 404</div>
    <h1>Short URL Not Found</h1>
    <p>The short code <code>{short_code}</code> was not found in the C Hash Table database.</p>
    <a href="/" class="btn">Return to TinyURL Home</a>
  </div>
</body>
</html>"""
        body = html.encode("utf-8")
        self.send_response(404)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, X-Requested-With")
        self.end_headers()

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path

        # URL Redirection route: /r/<shortCode>
        if path.startswith("/r/"):
            short_code = path[3:].strip()
            if not short_code:
                self.send_404_html("(empty)")
                return

            result = run_c_backend("resolve", short_code)
            if result.get("success") and result.get("originalURL"):
                target_url = result["originalURL"]
                self.send_response(302)
                self.send_header("Location", target_url)
                self.send_header("Cache-Control", "no-cache, no-store, must-revalidate")
                self.end_headers()
                return
            else:
                self.send_404_html(short_code)
                return

        # API: List all URLs -> GET /api/urls
        if path == "/api/urls":
            res = run_c_backend("list")
            status = 200 if res.get("success") else 500
            self.send_json(status, res)
            return

        # API: Statistics -> GET /api/stats
        if path == "/api/stats":
            res = run_c_backend("stats")
            status = 200 if res.get("success") else 500
            self.send_json(status, res)
            return

        # API: Search via query params -> GET /api/search?type=code&q=1
        if path == "/api/search":
            qs = urllib.parse.parse_qs(parsed.query)
            stype = qs.get("type", ["code"])[0]
            query = qs.get("q", [""])[0].strip()

            if not query:
                self.send_json(400, {"success": False, "error": "Query cannot be empty"})
                return

            if stype == "url":
                res = run_c_backend("search-url", query)
            else:
                res = run_c_backend("search-code", query)

            status = 200 if res.get("success") else 404
            self.send_json(status, res)
            return

        self.send_json(404, {"success": False, "error": "Endpoint not found"})

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path

        content_len = int(self.headers.get("Content-Length", 0))
        body_bytes = self.rfile.read(content_len) if content_len > 0 else b"{}"

        try:
            data = json.loads(body_bytes.decode("utf-8")) if body_bytes else {}
        except json.JSONDecodeError:
            self.send_json(400, {"success": False, "error": "Malformed JSON payload"})
            return

        # API: Shorten URL -> POST /api/shorten
        if path == "/api/shorten":
            url = data.get("url", "").strip()
            if not url:
                self.send_json(400, {"success": False, "error": "URL cannot be empty."})
                return

            res = run_c_backend("shorten", url)
            status = 200 if res.get("success") else 400
            self.send_json(status, res)
            return

        # API: Search -> POST /api/search
        if path == "/api/search":
            stype = data.get("type", "code")
            query = data.get("query", "").strip()

            if not query:
                self.send_json(400, {"success": False, "error": "Search query cannot be empty"})
                return

            if stype == "url":
                res = run_c_backend("search-url", query)
            else:
                res = run_c_backend("search-code", query)

            status = 200 if res.get("success") else 404
            self.send_json(status, res)
            return

        # API: Delete -> POST /api/delete
        if path == "/api/delete":
            short_code = data.get("shortCode", "").strip()
            if not short_code:
                self.send_json(400, {"success": False, "error": "Missing short code to delete"})
                return

            res = run_c_backend("delete", short_code)
            status = 200 if res.get("success") else 404
            self.send_json(status, res)
            return

        self.send_json(404, {"success": False, "error": "Endpoint not found"})
