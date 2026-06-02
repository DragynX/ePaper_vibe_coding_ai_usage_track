from pathlib import Path

from .common import aggregate_jsonl, day_bounds, env_or_default


def collect():
    today_start, today_end = day_bounds()
    root = env_or_default("copilot", str(Path.home() / ".copilot"))
    return aggregate_jsonl(root, today_start - 86400, today_start, today_end)
