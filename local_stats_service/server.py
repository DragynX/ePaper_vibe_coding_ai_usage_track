#!/usr/bin/env python3
import argparse
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

from providers import COLLECTORS
from providers.common import EMPTY


class StatsHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path != "/v1/snapshot":
            self.send_error(404)
            return

        query = parse_qs(parsed.query)
        raw_providers = query.get("providers", [""])[0]
        requested = [p.strip().lower() for p in raw_providers.split(",") if p.strip()]
        if not requested:
            requested = sorted(COLLECTORS.keys())

        body = {"providers": {}}
        for provider in requested:
            collector = COLLECTORS.get(provider)
            if not collector:
                item = dict(EMPTY)
                item["status"] = "unknown provider"
            else:
                try:
                    item = collector()
                except Exception as exc:
                    item = dict(EMPTY)
                    item["status"] = "error"
                    item["error"] = str(exc)
            body["providers"][provider] = item

        payload = json.dumps(body, separators=(",", ":")).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, fmt, *args):
        print("[local-stats] " + fmt % args)


def main():
    parser = argparse.ArgumentParser(description="Usage Monitor local stats service")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8787)
    args = parser.parse_args()

    server = ThreadingHTTPServer((args.host, args.port), StatsHandler)
    print("Usage Monitor local stats service listening on %s:%d" % (args.host, args.port))
    server.serve_forever()


if __name__ == "__main__":
    main()
