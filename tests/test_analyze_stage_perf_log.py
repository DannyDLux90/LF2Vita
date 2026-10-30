"""Host-only regression coverage for tools/analyze_stage_perf_log.py."""
import importlib.util
import unittest
from pathlib import Path

MODULE_PATH = Path(__file__).resolve().parents[1] / "tools" / "analyze_stage_perf_log.py"
SPEC = importlib.util.spec_from_file_location("analyze_stage_perf_log", MODULE_PATH)
assert SPEC and SPEC.loader
analyzer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(analyzer)

LOG = """[0000001000 ms] STAGE stage:wave_advance | phase=2/21
[0000001020 ms] INFO  loading textures char=Bandit sheets=4
[0000001050 ms] MEM   after texture user=229638144 cdram=0 phycont=123456
[0000001400 ms] PERF  stage fps=50.0 logic_hz=25.0 enemies=40 phase=13/21 backlog_us=2 logic_us_per_tu=40000.0 draw_us_per_frame=15000.0
[0000008000 ms] PERF  stage fps=60.0 logic_hz=30.0 enemies=1 phase=14/21 backlog_us=0 logic_us_per_tu=1000.0 draw_us_per_frame=15000.0
"""


class PerfLogTest(unittest.TestCase):
    def test_transition_and_clean_window(self):
        parsed = analyzer.parse_log(LOG, nearby_ms=5000)
        self.assertEqual(len(parsed["samples"]), 2)
        self.assertEqual(parsed["cdram_min_bytes"], 0)
        self.assertEqual(parsed["event_count"], 2)
        first, second = parsed["samples"]
        self.assertTrue(first["texture_load_nearby"])
        self.assertTrue(first["stage_transition_nearby"])
        self.assertFalse(first["clean_window"])
        self.assertTrue(second["clean_window"])
        self.assertEqual(first["enemies"], 40)
        self.assertEqual(second["logic_hz"], 30)

    def test_summary_and_bins(self):
        data = analyzer.report(analyzer.parse_log(LOG))
        self.assertEqual(data["sample_count"], 2)
        self.assertEqual(data["by_enemy_count"]["31-40"]["count"], 1)
        self.assertEqual(data["by_enemy_count"]["0-10"]["fps"]["mean"], 60)
        self.assertEqual(data["windows_with_load_or_transition"]["count"], 1)
        self.assertEqual(data["windows_without_load_or_transition"]["count"], 1)

    def test_empty_and_invalid(self):
        self.assertEqual(analyzer.report(analyzer.parse_log("not a log"))["sample_count"], 0)
        parsed = analyzer.parse_log("[0000001000 ms] PERF  stage fps=55 logic_hz=25 enemies=10", nearby_ms=0)
        self.assertEqual(len(parsed["samples"]), 1)
        self.assertTrue(parsed["samples"][0]["clean_window"])


if __name__ == "__main__":
    unittest.main()
