import json
import os
from collections import Counter
from datetime import datetime, timedelta
from pathlib import Path


EMPTY = {
    "available": False,
    "status": "no data",
    "today_tokens": 0,
    "input_tokens": 0,
    "output_tokens": 0,
    "cache_tokens": 0,
    "session_count": 0,
    "latest_at": 0,
    "top_models": [],
}


def day_bounds(now=None):
    current = now or datetime.now().astimezone()
    start = current.replace(hour=0, minute=0, second=0, microsecond=0)
    end = start + timedelta(days=1)
    return start.timestamp(), end.timestamp()


def parse_timestamp(value):
    if not value:
        return None
    if isinstance(value, (int, float)):
        return float(value) / 1000 if value > 100000000000 else float(value)
    if not isinstance(value, str):
        return None
    text = value.replace("Z", "+00:00")
    try:
        return datetime.fromisoformat(text).timestamp()
    except ValueError:
        return None


def recent_jsonl_files(root, since_epoch):
    path = Path(root).expanduser()
    if not path.exists():
        return []
    files = []
    for item in path.rglob("*.jsonl"):
        try:
            if item.stat().st_mtime >= since_epoch:
                files.append(item)
        except OSError:
            continue
    return files


def nested_get(data, path):
    cur = data
    for key in path:
        if not isinstance(cur, dict) or key not in cur:
            return None
        cur = cur[key]
    return cur


def read_int(data, paths):
    for path in paths:
        value = nested_get(data, path)
        if isinstance(value, bool):
            continue
        if isinstance(value, int):
            return value
        if isinstance(value, float):
            return int(value)
    return 0


def extract_record(obj):
    message = obj.get("message") if isinstance(obj.get("message"), dict) else obj
    usage = message.get("usage") if isinstance(message.get("usage"), dict) else obj.get("usage")
    if not isinstance(usage, dict):
        return None
    model = message.get("model") or obj.get("model") or usage.get("model") or "unknown"
    ts = parse_timestamp(obj.get("timestamp") or message.get("timestamp") or obj.get("created_at"))
    if ts is None:
        return None
    input_tokens = read_int(usage, [["input_tokens"], ["prompt_tokens"]])
    output_tokens = read_int(usage, [["output_tokens"], ["completion_tokens"]])
    cache_creation = read_int(usage, [["cache_creation_input_tokens"]])
    cache_read = read_int(usage, [["cache_read_input_tokens"], ["cached_tokens"]])
    total = read_int(usage, [["total_tokens"]])
    if total > 0 and input_tokens + output_tokens == 0:
        input_tokens = total
    if input_tokens + output_tokens + cache_creation + cache_read == 0:
        return None
    return {
        "model": str(model),
        "timestamp": ts,
        "input_tokens": input_tokens,
        "output_tokens": output_tokens,
        "cache_tokens": cache_creation + cache_read,
    }


def aggregate_jsonl(root, since_epoch, today_start, today_end):
    records = []
    for file_path in recent_jsonl_files(root, since_epoch):
        try:
            with file_path.open("r", encoding="utf-8") as handle:
                for line in handle:
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        obj = json.loads(line)
                    except json.JSONDecodeError:
                        continue
                    record = extract_record(obj)
                    if record and today_start <= record["timestamp"] < today_end:
                        records.append(record)
        except OSError:
            continue

    if not records:
        return dict(EMPTY)

    input_tokens = sum(r["input_tokens"] for r in records)
    output_tokens = sum(r["output_tokens"] for r in records)
    cache_tokens = sum(r["cache_tokens"] for r in records)
    model_tokens = Counter()
    model_counts = Counter()
    for record in records:
        total = record["input_tokens"] + record["output_tokens"] + record["cache_tokens"]
        model_tokens[record["model"]] += total
        model_counts[record["model"]] += 1

    top_models = [
        {"name": name[:40], "tokens": tokens, "count": model_counts[name]}
        for name, tokens in model_tokens.most_common(3)
    ]
    return {
        "available": True,
        "status": "ok",
        "today_tokens": input_tokens + output_tokens + cache_tokens,
        "input_tokens": input_tokens,
        "output_tokens": output_tokens,
        "cache_tokens": cache_tokens,
        "session_count": len({int(r["timestamp"] // 1800) for r in records}),
        "latest_at": int(max(r["timestamp"] for r in records)),
        "top_models": top_models,
    }


def env_or_default(provider, default_path):
    key = "UM_LOCAL_%s_LOG_DIR" % provider.upper()
    return os.environ.get(key) or default_path
