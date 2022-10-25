import argparse
import dataclasses
import json
import os
import subprocess

TEST_DB = "test_db.json"
INTERPRETER = "bin/linux/debug/musique"

@dataclasses.dataclass
class Result:
    exit_code:    int       = 0
    stdin_lines:  list[str] = dataclasses.field(default_factory=list)
    stdout_lines: list[str] = dataclasses.field(default_factory=list)
    stderr_lines: list[str] = dataclasses.field(default_factory=list)

@dataclasses.dataclass
class TestCase:
    name:         str
    exit_code:    int       = 0
    stdin_lines:  list[str] = dataclasses.field(default_factory=list)
    stdout_lines: list[str] = dataclasses.field(default_factory=list)
    stderr_lines: list[str] = dataclasses.field(default_factory=list)

    def run(self, interpreter: str, source: str, cwd: str):
        result = subprocess.run(
            args=[interpreter, source, "-q"],
            capture_output=True,
            cwd=cwd,
            text=True
        )
        return Result(
            exit_code=result.returncode,
            stdout_lines=result.stdout.splitlines(keepends=False),
            stderr_lines=result.stderr.splitlines(keepends=False)
        )

    def record(self, interpreter: str, source: str, cwd: str):
        print(f"Recording case {self.name}")
        result = self.run(interpreter, source, cwd)

        changes = []
        if self.exit_code    != result.exit_code:    changes.append("exit code")
        if self.stderr_lines != result.stderr_lines: changes.append("stderr")
        if self.stdout_lines != result.stdout_lines: changes.append("stdout")
        if changes:
            print(f"  changed: {', '.join(changes)}")

        self.exit_code, self.stderr_lines, self.stdout_lines = result.exit_code, result.stderr_lines, result.stdout_lines

    def test(self, interpreter: str, source: str, cwd: str):
        print(f"  Testing case {self.name}  ", end="")
        result = self.run(interpreter, source, cwd)
        if self.exit_code == result.exit_code and self.stdout_lines == result.stdout_lines and self.stderr_lines == result.stderr_lines:
            print("ok")
            return True

        print(f"FAILED")
        print(f"File: {source}")

        if self.exit_code != result.exit_code:
            print(f"Different exit code - expected {self.exit_code}, got {result.exit_code}")

        for name, expected, actual in [
            ("standard output", self.stdout_lines, result.stdout_lines),
            ("standard error",  self.stderr_lines, result.stderr_lines)
        ]:
            if expected == actual:
                continue

            diff_line = None
            for i, (exp_line, got_line) in enumerate(zip(expected, actual)):
                if exp_line != got_line:
                    diff_line = i
                    break

            if diff_line is not None:
                print(f"First difference at line {diff_line+1} in {name}:")
                print(f"  Expected: {expected[diff_line]}")
                print(f"       Got: {actual[diff_line]}")
            elif len(expected) > len(actual):
                print(f"Expected {name} is {len(expected) - len(actual)} lines longer then actual")
            else:
                print(f"Actual {name} is {len(actual) - len(expected)} lines longer then expected")

        return False


@dataclasses.dataclass
class TestSuite:
    name:  str
    cases: list[TestCase] = dataclasses.field(default_factory=list)

suites = list[TestSuite]()

def traverse(discover: bool, update: bool):
    to_record = list[tuple[TestSuite, TestCase]]()
    if discover:
        for suite_name in os.listdir(testing_dir):
            if os.path.isdir(os.path.join(testing_dir, suite_name)) and suite_name not in (suite.name for suite in suites):
                print(f"Discovered new test suite: {suite_name}")
                suites.append(TestSuite(name=suite_name))

        for suite in suites:
            suite_path = os.path.join(testing_dir, suite.name)
            for case_name in os.listdir(suite_path):
                if os.path.isfile(os.path.join(suite_path, case_name)) and case_name not in (case.name for case in suite.cases):
                    print(f"In suite '{suite.name}' discovered new test case: {case_name}")
                    case = TestCase(name=case_name)
                    suite.cases.append(case)
                    to_record.append((suite, case))

    if update:
        to_record.extend(((suite, case) for suite in suites for case in suite.cases))

    for (suite, case) in to_record:
        case.record(
            interpreter=os.path.join(root, INTERPRETER),
            source=os.path.join(testing_dir, suite.name, case.name),
            cwd=root
        )

    with open(test_db_path, "w") as f:
        json_suites = [dataclasses.asdict(suite) for suite in suites]
        json.dump(json_suites, f, indent=2)

def test():
    successful, total = 0, 0
    for suite in suites:
        print(f"Testing suite {suite.name}")
        for case in suite.cases:
            successful += int(case.test(
                interpreter=os.path.join(root, INTERPRETER),
                source=os.path.join(testing_dir, suite.name, case.name),
                cwd=root
            ))
            total += 1

    print(f"Passed {successful} out of {total} ({100 * successful // total}%)")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Regression test runner for Musique programming language")
    parser.add_argument("-d", "--discover", action="store_true", help="Discover all tests that are not in testing database")
    parser.add_argument("-u", "--update", action="store_true", help="Update all tests")

    args = parser.parse_args()

    root = os.path.dirname(os.path.dirname(__file__))
    testing_dir = os.path.join(root, "regression-tests")
    test_db_path = os.path.join(testing_dir, TEST_DB)

    with open(test_db_path, "r") as f:
        for src in json.load(f):
            src["cases"] = [TestCase(**case) for case in src["cases"]]
            suites.append(TestSuite(**src))

    if args.discover or args.update:
        traverse(discover=args.discover, update=args.update)
    else:
        test()
