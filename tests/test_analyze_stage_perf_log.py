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

    def test_profiler_breakdown_and_phase_numbers(self):
        log = """[0000001000 ms] MEM cdram=123456
[0000001400 ms] PERF  stage fps=42.0 logic_hz=20.0 enemies=40 phase=13/21 logic_us_per_tu=40000.0 draw_us_per_frame=15000.0 ai_us_per_tu=3000.0 objects_us_per_tu=5000.0 hits_us_per_tu=28000.0
[0000002000 ms] MEM cdram=222222
[0000007000 ms] PERF  stage fps=60.0 logic_hz=30.0 enemies=1 phase=14/21 logic_us_per_tu=2000.0 draw_us_per_frame=14000.0 ai_us_per_tu=1000.0 objects_us_per_tu=1000.0 hits_us_per_tu=500.0
"""
        parsed = analyzer.parse_log(log)
        early, late = parsed["samples"]
        self.assertEqual((early["phase_index"], early["phase_total"]), (13, 21))
        self.assertEqual(early["other_us_per_tu"], 4000.0)
        self.assertTrue(early["measured_logic_over_budget"])
        self.assertTrue(early["logic_under_25_hz"])
        self.assertEqual(early["last_reported_cdram_bytes"], 123456)
        self.assertEqual(late["last_reported_cdram_bytes"], 222222)
        self.assertFalse(late["measured_logic_over_budget"])
        self.assertNotIn("other_us_per_tu", late)

    def test_slow_interval_with_little_measured_work(self):
        log = "[0000009000 ms] PERF  stage fps=38 logic_hz=19 enemies=1 logic_us_per_tu=1000 draw_us_per_frame=15000"
        parsed = analyzer.parse_log(log)
        row = parsed["samples"][0]
        self.assertTrue(row["slow_with_low_measured_logic_cost"])
        self.assertTrue(row["clean_window"])
        self.assertIsNone(row["last_reported_cdram_bytes"])
        stats = analyzer.report(parsed)
        self.assertEqual(stats["slow_with_low_measured_logic_cost"]["count"], 1)
        self.assertEqual(stats["clean_windows_by_enemy_count"]["0-10"]["count"], 1)
        self.assertEqual(len(stats["slowest_clean_logic_windows"]), 1)

    def test_future_events_do_not_mark_previous_samples(self):
        log = """[0000001000 ms] PERF stage fps=60 logic_hz=30 enemies=0
[0000003000 ms] STAGE stage:wave_advance phase=2/21
[0000003200 ms] INFO loading textures char=Bandit sheets=4
[0000003500 ms] PERF stage fps=40 logic_hz=22 enemies=40
[0000009000 ms] PERF stage fps=60 logic_hz=30 enemies=0
"""
        first, second, third = analyzer.parse_log(log, nearby_ms=2000)["samples"]
        self.assertTrue(first["clean_window"])
        self.assertIsNone(first["ms_since_texture_load"])
        self.assertTrue(second["stage_transition_nearby"])
        self.assertTrue(second["texture_load_nearby"])
        self.assertEqual(second["ms_since_texture_load"], 300)
        self.assertTrue(third["clean_window"])


    def test_baseline_comparison_restricts_to_clean_equal_enemy_bins(self):
        older = """[0000001000 ms] PERF stage fps=45 logic_hz=22 enemies=40 logic_us_per_tu=20000
[0000009000 ms] PERF stage fps=55 logic_hz=28 enemies=40 logic_us_per_tu=10000
[0000015000 ms] PERF stage fps=60 logic_hz=30 enemies=1 logic_us_per_tu=1000
"""
        newer = """[0000001000 ms] PERF stage fps=50 logic_hz=25 enemies=40 logic_us_per_tu=15000
[0000009000 ms] PERF stage fps=60 logic_hz=30 enemies=40 logic_us_per_tu=9000
[0000015000 ms] PERF stage fps=59 logic_hz=30 enemies=1 logic_us_per_tu=1000
"""
        before = analyzer.parse_log(older)
        after = analyzer.parse_log(newer)
        comparison = analyzer.compare_runs(before, after, min_samples=2)
        heavy = comparison["bins"]["31-40"]
        self.assertEqual(heavy["status"], "exploratory_only")
        self.assertEqual(heavy["metrics"]["fps"]["baseline_mean"], 50)
        self.assertEqual(heavy["metrics"]["fps"]["candidate_mean"], 55)
        self.assertEqual(heavy["metrics"]["fps"]["delta_percent"], 10)
        self.assertEqual(comparison["bins"]["0-10"]["status"], "insufficient_samples")
        self.assertFalse(comparison["controlled_benchmark"])
        with self.assertRaises(ValueError):
            analyzer.compare_runs(before, after, min_samples=0)

    def test_baseline_comparison_excludes_transition_intervals(self):
        older = """[0000001000 ms] INFO loading textures char=Bandit sheets=4
[0000001100 ms] PERF stage fps=10 logic_hz=10 enemies=40
[0000010000 ms] PERF stage fps=50 logic_hz=26 enemies=40
"""
        newer = """[0000001000 ms] PERF stage fps=60 logic_hz=30 enemies=40
[0000009000 ms] PERF stage fps=60 logic_hz=30 enemies=40
"""
        comparison = analyzer.compare_runs(analyzer.parse_log(older),
                                           analyzer.parse_log(newer), min_samples=1)
        heavy = comparison["bins"]["31-40"]
        self.assertEqual(heavy["baseline_samples"], 1)
        self.assertEqual(heavy["candidate_samples"], 2)
        self.assertEqual(heavy["metrics"]["fps"]["baseline_mean"], 50)



if __name__ == "__main__":
    unittest.main()
