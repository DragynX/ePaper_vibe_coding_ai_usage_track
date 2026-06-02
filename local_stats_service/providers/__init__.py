from .claude import collect as collect_claude
from .codex import collect as collect_codex
from .copilot import collect as collect_copilot
from .kimi import collect as collect_kimi
from .minimax import collect as collect_minimax
from .zai import collect as collect_zai


COLLECTORS = {
    "claude": collect_claude,
    "codex": collect_codex,
    "copilot": collect_copilot,
    "kimi": collect_kimi,
    "minimax": collect_minimax,
    "zai": collect_zai,
}
