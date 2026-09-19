import importlib.util
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location(
    "formatter", Path(__file__).resolve().parents[1] / "format-all.py"
)
FORMAT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FORMAT)


class FormatterTests(unittest.TestCase):
    def test_excludes_generated_sources(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for name in [
                "main/app.c",
                "scripts/tool.py",
                "tests/gtest/CMakeLists.txt",
                "tests/gtest/build-coverage/generated.cpp",
                "managed_components/vendor/CMakeLists.txt",
                ".venv/tool.py",
            ]:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()
            groups = FORMAT.source_files(root)
            self.assertEqual([p.name for p in groups["cpp"]], ["app.c"])
            self.assertEqual([p.name for p in groups["python"]], ["tool.py"])
            self.assertEqual([p.name for p in groups["cmake"]], ["CMakeLists.txt"])

    def test_check_mode_does_not_request_writes(self):
        commands = list(
            FORMAT.commands(
                {
                    "cpp": [Path("a.c")],
                    "python": [Path("a.py")],
                    "cmake": [Path("CMakeLists.txt")],
                },
                check=True,
            )
        )
        self.assertIn("--dry-run", commands[0])
        self.assertIn("--Werror", commands[0])
        self.assertIn("--check", commands[1])
        self.assertIn("--check", commands[2])
        self.assertTrue(all("-i" not in command for command in commands))


if __name__ == "__main__":
    unittest.main()
