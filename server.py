#!/usr/bin/env python3
"""
TinyURL – Lightweight HTTP Server & Bridge to C Backend
Data Structures and Algorithms (DSA) Project

This server acts SOLELY as an HTTP/REST communication bridge between the
frontend interface and the compiled C executable (tinyurl). All DSA operations
(hash table, separate chaining, linked lists, Base-62 encoding, searching,
deletion, click tracking, and collision statistics) are performed by the C program.
"""

import sys
import os
import json
import subprocess
import urllib.parse
from http.server import HTTPServer, SimpleHTTPRequestHandler
from socketserver import ThreadingMixIn

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
FRONTEND_DIR = os.path.join(BASE_DIR, "frontend")
DATA_DIR = os.path.join(BASE_DIR, "data")
DATA_FILE = os.path.join(DATA_DIR, "urls.dat")
SRC_DIR = os.path.join(BASE_DIR, "src")

EXE_NAME = "tinyurl.exe" if sys.platform == "win32" else "tinyurl"
EXE_PATH = os.path.join(BASE_DIR, EXE_NAME)

PORT = 8000


def ensure_c_executable():
    """Compiles the C backend if executable is missing or sources are newer."""
    os.makedirs(DATA_DIR, exist_ok=True)

    src_files = [
        os.path.join(SRC_DIR, f)
        for f in ["main.c", "url.c", "hashmap.c", "url_service.c", "storage.c"]
    ]
    header_files = [
        os.path.join(SRC_DIR, f)
        for f in ["url.h", "hashmap.h", "url_service.h", "storage.h"]
    ]

    all_deps = src_files + header_files
    needs_compile = False

    if not os.path.exists(EXE_PATH):
        needs_compile = True
    else:
        exe_mtime = os.path.getmtime(EXE_PATH)
        for dep in all_deps:
            if os.path.exists(dep) and os.path.getmtime(dep) > exe_mtime:
                needs_compile = True
                break

    if needs_compile:
        print("[TinyURL] Compiling C backend using GCC...")
        cmd = [
            "gcc",
            "-Wall",
            "-Wextra",
            "-pedantic",
            "-std=c99",
            "-I" + SRC_DIR,
            *src_files,
            "-o",
            EXE_PATH,
        ]
        result = subprocess.run(cmd, cwd=BASE_DIR, capture_output=True, text=True)
        if result.returncode != 0:
            print("[TinyURL] ERROR: GCC compilation failed!", file=sys.stderr)
            print(result.stderr, file=sys.stderr)
            sys.exit(1)
        print("[TinyURL] C backend compiled successfully ->", EXE_PATH)
    else:
        print("[TinyURL] C backend executable is up to date ->", EXE_PATH)


def run_c_backend(action, *args):
    """Executes the C executable with --api and returns parsed JSON output."""
    cmd = [EXE_PATH, "--api", "--data-file", DATA_FILE, action, *args]
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

    # Extract JSON object from stdout (in case of any stray prefix)
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


class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    daemon_threads = True


class TinyURLHandler(SimpleHTTPRequestHandler):
    def send_json(self, status_code, payload):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status_code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header(
            "Access-Control-Allow-Headers", "Content-Type, X-Requested-With"
        )
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
      font-family: system-ui, -apple-system, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
      background: #f8fafc;
      color: #0f172a;
      display: flex;
      align-items: center;
      justify-content: center;
      min-height: 100vh;
      margin: 0;
      padding: 1.5rem;
      box-sizing: border-box;
    }}
    .card {{
      background: #ffffff;
      border: 1px solid #e2e8f0;
      border-radius: 12px;
      padding: 2.5rem;
      max-width: 480px;
      width: 100%;
      text-align: center;
      box-shadow: 0 4px 6px -1px rgba(0,0,0,0.05), 0 2px 4px -2px rgba(0,0,0,0.05);
    }}
    .code-badge {{
      display: inline-block;
      background: #fee2e2;
      color: #991b1b;
      padding: 0.25rem 0.75rem;
      border-radius: 9999px;
      font-weight: 600;
      font-size: 0.875rem;
      margin-bottom: 1rem;
    }}
    h1 {{
      margin: 0 0 0.5rem 0;
      font-size: 1.5rem;
      color: #0f172a;
    }}
    p {{
      color: #64748b;
      line-height: 1.5;
      margin: 0 0 1.5rem 0;
    }}
    .code-term {{
      font-family: monospace;
      background: #f1f5f9;
      padding: 0.15rem 0.4rem;
      border-radius: 4px;
      color: #0f172a;
    }}
    a.btn {{
      display: inline-block;
      background: #4f46e5;
      color: #ffffff;
      text-decoration: none;
      padding: 0.75rem 1.5rem;
      border-radius: 8px;
      font-weight: 500;
      transition: background 0.15s ease;
    }}
    a.btn:hover {{
      background: #4338ca;
    }}
  </style>
</head>
<body>
  <div class="card">
    <div class="code-badge">HTTP 404</div>
    <h1>Short URL Not Found</h1>
    <p>The short code <span class="code-term">{short_code}</span> was not found in the C Hash Table database. It may have expired or been deleted.</p>
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
        self.send_header(
            "Access-Control-Allow-Headers", "Content-Type, X-Requested-With"
        )
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
                self.send_header(
                    "Cache-Control", "no-cache, no-store, must-revalidate"
                )
                self.send_header("Pragma", "no-cache")
                self.send_header("Expires", "0")
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

        # Serve static frontend files
        if path in ["/", "/index.html"]:
            file_path = os.path.join(FRONTEND_DIR, "index.html")
        else:
            # Strip leading slash and prevent directory traversal
            clean_rel = os.path.normpath(path.lstrip("/")).replace("\\", "/")
            if clean_rel.startswith(".."):
                self.send_error(403, "Forbidden")
                return
            file_path = os.path.join(FRONTEND_DIR, clean_rel)

        if os.path.isfile(file_path):
            content_type = "text/plain"
            if file_path.endswith(".html"):
                content_type = "text/html; charset=utf-8"
            elif file_path.endswith(".css"):
                content_type = "text/css; charset=utf-8"
            elif file_path.endswith(".js"):
                content_type = "application/javascript; charset=utf-8"
            elif file_path.endswith(".svg"):
                content_type = "image/svg+xml"
            elif file_path.endswith(".ico"):
                content_type = "image/x-icon"

            with open(file_path, "rb") as f:
                content = f.read()

            self.send_response(200)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(content)))
            self.end_headers()
            self.wfile.write(content)
        else:
            self.send_error(404, "File Not Found")

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
                self.send_json(
                    400,
                    {
                        "success": False,
                        "error": "URL cannot be empty. Please enter a valid URL.",
                    },
                )
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
                self.send_json(
                    400, {"success": False, "error": "Search query cannot be empty"}
                )
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
                self.send_json(
                    400, {"success": False, "error": "Missing short code to delete"}
                )
                return

            res = run_c_backend("delete", short_code)
            status = 200 if res.get("success") else 404
            self.send_json(status, res)
            return

        # Unknown POST endpoint
        self.send_json(404, {"success": False, "error": "Endpoint not found"})


def main():
    ensure_c_executable()

    server_address = ("", PORT)
    httpd = ThreadedHTTPServer(server_address, TinyURLHandler)
    url = f"http://localhost:{PORT}"

    print("==============================================================")
    print("      TinyURL – DSA URL Shortener Web Application (in C)      ")
    print("==============================================================")
    print(f"[*] Local Website Address : {url}")
    print(f"[*] C Backend Binary      : {EXE_PATH}")
    print(f"[*] Persistence Data File : {DATA_FILE}")
    print("==============================================================")
    print("[*] Server is running. Press Ctrl+C to terminate.")

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n[TinyURL] Shutting down server gracefully...")
        httpd.server_close()
        print("[TinyURL] Server stopped.")


if __name__ == "__main__":
    main()
