#!/usr/bin/env python3
"""Stand-in for the Anthropic Messages API, used inside the test VM so the
real assistant GUI can be exercised without a key or Internet access.

Listens on 127.0.0.1:8765, logs every request (headers + JSON body) to
/tmp/mock-provider.jsonl and streams a fixed Hebrew/English Markdown reply.
"""
import json
import sys
import time
from http.server import BaseHTTPRequestHandler, HTTPServer

REPLY = [
    "שלום! כדי לעדכן את רשימת החבילות ב-ZionOS הריצו את הפקודה הבאה ",
    "בטרמינל. הפקודה `apt update` רק מרעננת את הרשימה ואינה משנה דבר במערכת.\n\n",
    "```sh\nsudo apt update\n```\n\n",
    "This second paragraph is in English and must stay left-to-right. ",
    "<script>alert('x')</script> <b>not bold</b>\n",
]
LOG = "/tmp/mock-provider.jsonl"


def sse(event, obj):
    return ("event: %s\ndata: %s\n\n" % (event, json.dumps(obj, ensure_ascii=False))).encode()


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("content-length", "0")))
        try:
            parsed = json.loads(body)
        except ValueError:
            parsed = None
        with open(LOG, "a", encoding="utf-8") as f:
            f.write(json.dumps({"path": self.path, "headers": {k.lower(): v for k, v in self.headers.items()},
                                "body": parsed}, ensure_ascii=False) + "\n")
        if self.headers.get("x-api-key") != "sk-ant-test-vm-0000000000000000":
            data = json.dumps({"type": "error", "error": {"type": "authentication_error",
                                                          "message": "invalid x-api-key"}}).encode()
            self.send_response(401)
            self.send_header("content-type", "application/json")
            self.send_header("content-length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
            return
        self.send_response(200)
        self.send_header("content-type", "text/event-stream")
        self.send_header("connection", "close")
        self.end_headers()
        w = self.wfile
        w.write(sse("message_start", {"type": "message_start", "message": {
            "id": "msg_test", "model": parsed.get("model", "?"), "usage": {"input_tokens": 42, "output_tokens": 1}}}))
        w.write(sse("content_block_start", {"type": "content_block_start", "index": 0,
                                            "content_block": {"type": "text", "text": ""}}))
        for part in REPLY:
            w.write(sse("content_block_delta", {"type": "content_block_delta", "index": 0,
                                                "delta": {"type": "text_delta", "text": part}}))
            w.flush()
            time.sleep(0.3)
        w.write(sse("content_block_stop", {"type": "content_block_stop", "index": 0}))
        w.write(sse("message_delta", {"type": "message_delta", "delta": {"stop_reason": "end_turn"},
                                      "usage": {"output_tokens": 57}}))
        w.write(sse("message_stop", {"type": "message_stop"}))
        w.flush()
        self.close_connection = True


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    HTTPServer(("127.0.0.1", port), Handler).serve_forever()
