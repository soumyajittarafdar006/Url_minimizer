#!/usr/bin/env python3
"""
TinyURL – End-to-End Web and C Backend Integration Test Suite
Verifies all 12 test workflow requirements against the running server.
"""

import sys
import os
import json
import time
import urllib.request
import urllib.error
import subprocess

PORT = 8000
BASE_URL = f"http://127.0.0.1:{PORT}"

def run_tests():
    print("=" * 66)
    print("  TinyURL Web & DSA C Backend End-to-End Verification Suite")
    print("=" * 66)

    # 1. Test Static Frontend Assets
    print("\n[Test 1] Testing static frontend routes...")
    req = urllib.request.urlopen(f"{BASE_URL}/")
    assert req.status == 200, f"Expected 200, got {req.status}"
    html = req.read().decode("utf-8")
    assert "Shorten Your Long URLs" in html, "HTML missing hero title"
    assert "TinyURL" in html, "HTML missing brand text"
    print("  -> GET / [200 OK] Frontend HTML loaded successfully.")

    req_css = urllib.request.urlopen(f"{BASE_URL}/style.css")
    assert req_css.status == 200, f"Expected 200, got {req_css.status}"
    print("  -> GET /style.css [200 OK] Stylesheet loaded.")

    req_js = urllib.request.urlopen(f"{BASE_URL}/script.js")
    assert req_js.status == 200, f"Expected 200, got {req_js.status}"
    print("  -> GET /script.js [200 OK] Client JavaScript loaded.")

    # 2. Test Shortening URL (Requirement 1 & 2)
    print("\n[Test 2] Testing URL Shortening via C Backend (POST /api/shorten)...")
    test_url = "https://en.wikipedia.org/wiki/Hash_table"
    post_data = json.dumps({"url": test_url}).encode("utf-8")
    req = urllib.request.Request(
        f"{BASE_URL}/api/shorten",
        data=post_data,
        headers={"Content-Type": "application/json"}
    )
    res = urllib.request.urlopen(req)
    assert res.status == 200, f"Expected 200, got {res.status}"
    shorten_res = json.loads(res.read().decode("utf-8"))
    assert shorten_res["success"] is True, f"Shorten failed: {shorten_res}"
    short_code = shorten_res["shortCode"]
    print(f"  -> Generated Short Code: '{short_code}' for '{test_url}'")
    assert shorten_res["originalURL"] == test_url, "Original URL mismatch"
    assert shorten_res["clickCount"] == 0, "Initial clickCount must be 0"

    # 3. Test Invalid URL Rejection by C Backend
    print("\n[Test 3] Testing Invalid URL rejection...")
    invalid_data = json.dumps({"url": "invalid-url-without-scheme"}).encode("utf-8")
    req_inv = urllib.request.Request(
        f"{BASE_URL}/api/shorten",
        data=invalid_data,
        headers={"Content-Type": "application/json"}
    )
    try:
        urllib.request.urlopen(req_inv)
        assert False, "Should have returned HTTP 400 for invalid URL"
    except urllib.error.HTTPError as e:
        assert e.code == 400
        err_res = json.loads(e.read().decode("utf-8"))
        print(f"  -> Correctly rejected by C backend: '{err_res.get('error')}'")

    # 4. Test List All URLs (Requirement 4)
    print("\n[Test 4] Testing Listing Stored URLs (GET /api/urls)...")
    res = urllib.request.urlopen(f"{BASE_URL}/api/urls")
    list_res = json.loads(res.read().decode("utf-8"))
    assert list_res["success"] is True, "List failed"
    records = list_res["records"]
    matching = [r for r in records if r["shortCode"] == short_code]
    assert len(matching) == 1, f"Record '{short_code}' not found in list"
    assert matching[0]["originalURL"] == test_url
    print(f"  -> Successfully listed {len(records)} records from C hash table.")

    # 5. Test Search by Short Code (Requirement 7)
    print("\n[Test 5] Testing Search by Short Code (POST /api/search)...")
    search_code_data = json.dumps({"type": "code", "query": short_code}).encode("utf-8")
    req_sc = urllib.request.Request(
        f"{BASE_URL}/api/search",
        data=search_code_data,
        headers={"Content-Type": "application/json"}
    )
    res_sc = urllib.request.urlopen(req_sc)
    search_res = json.loads(res_sc.read().decode("utf-8"))
    assert search_res["success"] is True
    assert search_res["record"]["shortCode"] == short_code
    assert search_res["record"]["clickCount"] == 0, "Search must NOT increment click count!"
    print(f"  -> Found record by short code: '{short_code}'. Click count verified unchanged at 0.")

    # 6. Test Search by Original URL (Requirement 8)
    print("\n[Test 6] Testing Search by Original URL (POST /api/search)...")
    search_url_data = json.dumps({"type": "url", "query": test_url}).encode("utf-8")
    req_su = urllib.request.Request(
        f"{BASE_URL}/api/search",
        data=search_url_data,
        headers={"Content-Type": "application/json"}
    )
    res_su = urllib.request.urlopen(req_su)
    search_url_res = json.loads(res_su.read().decode("utf-8"))
    assert search_url_res["success"] is True
    assert search_url_res["record"]["shortCode"] == short_code
    assert search_url_res["record"]["clickCount"] == 0, "Search must NOT increment click count!"
    print(f"  -> Found record by URL scan: '{test_url}'. Click count verified unchanged at 0.")

    # 7. Test URL Resolution & HTTP Redirection (Requirement 4, 5, 6)
    print("\n[Test 7] Testing HTTP 302 Redirection & Click Counting (GET /r/<shortCode>)...")
    # Custom opener to prevent automatic redirect following so we can inspect the 302 response
    class NoRedirectHandler(urllib.request.HTTPRedirectHandler):
        def redirect_request(self, req, fp, code, msg, headers, newurl):
            return None

    opener = urllib.request.build_opener(NoRedirectHandler)
    try:
        res_redir = opener.open(f"{BASE_URL}/r/{short_code}")
        assert False, "Should have received 302 redirect response"
    except urllib.error.HTTPError as e:
        assert e.code == 302, f"Expected 302 redirect, got {e.code}"
        loc = e.headers.get("Location")
        assert loc == test_url, f"Expected redirect to '{test_url}', got '{loc}'"
        print(f"  -> HTTP 302 Redirect verified: Location: {loc}")

    # Verify Click Count Incremented (Requirement 6)
    res_check = urllib.request.urlopen(f"{BASE_URL}/api/urls")
    list_check = json.loads(res_check.read().decode("utf-8"))
    updated_rec = next(r for r in list_check["records"] if r["shortCode"] == short_code)
    assert updated_rec["clickCount"] == 1, f"Expected clickCount 1, got {updated_rec['clickCount']}"
    print(f"  -> Click count correctly incremented to {updated_rec['clickCount']}!")

    # 8. Test 404 on Nonexistent Short Code
    print("\n[Test 8] Testing 404 on nonexistent short code...")
    try:
        opener.open(f"{BASE_URL}/r/nonexistentXYZ")
        assert False, "Should have returned 404"
    except urllib.error.HTTPError as e:
        assert e.code == 404
        content_404 = e.read().decode("utf-8")
        assert "Short URL Not Found" in content_404
        print("  -> HTTP 404 response verified with friendly error page.")

    # 9. Test Statistics Dashboard (Requirement 11)
    print("\n[Test 9] Testing Hash Table Statistics (GET /api/stats)...")
    res_stats = urllib.request.urlopen(f"{BASE_URL}/api/stats")
    stats = json.loads(res_stats.read().decode("utf-8"))
    assert stats["success"] is True
    assert stats["tableSize"] == 101, "tableSize must be 101"
    assert stats["totalRecords"] >= 1
    assert stats["usedBuckets"] >= 1
    assert len(stats["buckets"]) == 101, "Must contain all 101 bucket distribution records"
    print(f"  -> Hash table statistics: Total Records: {stats['totalRecords']}, Used Buckets: {stats['usedBuckets']}, Max Chain: {stats['maxChainLength']}, Load Factor: {stats['loadFactor']:.4f}")

    # 10. Test Deletion (Requirement 9 & 10)
    print("\n[Test 10] Testing Record Deletion (POST /api/delete)...")
    del_data = json.dumps({"shortCode": short_code}).encode("utf-8")
    req_del = urllib.request.Request(
        f"{BASE_URL}/api/delete",
        data=del_data,
        headers={"Content-Type": "application/json"}
    )
    res_del = urllib.request.urlopen(req_del)
    del_res = json.loads(res_del.read().decode("utf-8"))
    assert del_res["success"] is True, "Delete failed"
    print(f"  -> Successfully deleted record '{short_code}'.")

    # Verify deleted record is gone (Requirement 10)
    res_after_del = urllib.request.urlopen(f"{BASE_URL}/api/urls")
    list_after = json.loads(res_after_del.read().decode("utf-8"))
    assert not any(r["shortCode"] == short_code for r in list_after["records"]), f"Record '{short_code}' still exists in list!"
    print(f"  -> Verified record '{short_code}' no longer exists in hash table.")

    # 11. Test Persistence Across Server / Service Reload (Requirement 12)
    print("\n[Test 11] Testing Persistence Across Reload...")
    # Add a fresh record to persist
    persist_url = "https://deepmind.google/technologies/gemini/"
    post_data2 = json.dumps({"url": persist_url}).encode("utf-8")
    res2 = urllib.request.urlopen(urllib.request.Request(
        f"{BASE_URL}/api/shorten",
        data=post_data2,
        headers={"Content-Type": "application/json"}
    ))
    code2 = json.loads(res2.read().decode("utf-8"))["shortCode"]
    print(f"  -> Created persistent record '{code2}' -> '{persist_url}'.")

    # Resolve it once to set clicks = 1
    try:
        opener.open(f"{BASE_URL}/r/{code2}")
    except urllib.error.HTTPError as e:
        assert e.code == 302

    # Check via direct C command (independent process)
    exe_name = "tinyurl.exe" if sys.platform == "win32" else "tinyurl"
    direct_proc = subprocess.run(
        [os.path.join(".", exe_name), "--api", "search-code", code2],
        capture_output=True,
        text=True
    )
    direct_res = json.loads(direct_proc.stdout.strip())
    assert direct_res["success"] is True
    assert direct_res["record"]["shortCode"] == code2
    assert direct_res["record"]["clickCount"] == 1
    print(f"  -> Direct C process verified data persisted to 'data/urls.dat' with clickCount=1.")

    print("\n" + "=" * 66)
    print("  ALL 11 INTEGRATION TESTS PASSED SUCCESSFULLY!")
    print("=" * 66)

if __name__ == "__main__":
    run_tests()
