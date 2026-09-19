import contextlib
import importlib.util
import io
import unittest
from pathlib import Path
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location(
    "firmware_runner", Path(__file__).resolve().parents[1] / "test-all.py"
)
RUNNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUNNER)


class RunnerTests(unittest.TestCase):
    def test_ctest_summary_formats(self):
        for text in [
            "100% tests passed out of 2",
            "100% tests passed, 0 tests failed out of 2",
        ]:
            self.assertEqual(RUNNER.parse_gtest_summary(text)["passed"], 2)
        counts = RUNNER.parse_gtest_summary("50% tests passed, 1 tests failed out of 2")
        self.assertEqual((counts["passed"], counts["failed"]), (1, 1))

    def test_host_only_skips_hardware_and_coverage(self):
        with (
            patch.object(
                RUNNER.sys, "argv", ["test-all.py", "--host-only", "--no-coverage"]
            ),
            patch.object(
                RUNNER, "run", return_value=(0, "100% tests passed out of 2")
            ) as run,
            patch.object(RUNNER, "run_coverage") as coverage,
            contextlib.redirect_stdout(io.StringIO()),
        ):
            self.assertEqual(RUNNER.main(), 0)
            self.assertTrue(
                all("pytest" not in call.args[0] for call in run.call_args_list)
            )
            coverage.assert_not_called()


if __name__ == "__main__":
    unittest.main()
