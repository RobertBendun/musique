#!/usr/bin/env python3
from dataclasses import dataclass
from glob import glob
from sys import argv
from sys import exit
import os.path
import shlex
import subprocess
import json
from unittest import case

Interpreter = "bin/musique"

def directories_in_path(path: str):
    dirs = []
    while True:
        dirname, _ = os.path.split(path)
        if not dirname:
            break
        dirs.append(dirname)
        path = dirname
        if path == "/":
            break
    dirs.reverse()
    return dirs

def mkdir_recursive(path: str):
    for directory in directories_in_path(path):
        try:
            os.mkdir(directory)
        except FileExistsError:
            continue

@dataclass
class Test_Case:
    returncode = 0

    @staticmethod
    def from_file(fname : str):
        with open(fname) as f:
            content = json.load(f)
            tc = Test_Case()
            for name in content:
                # assert hasattr(tc, name), "Test_Case does not have attribute %s" % (name,)
                setattr(tc, name, content[name])
            return tc

    @staticmethod
    def from_run(run, flags=[]):
        tc = Test_Case()
        for attr in ["returncode", "stdout", "stderr"]: ### TODO FLAGS
            try:
                run_attr = getattr(run, attr).decode()
            except (UnicodeDecodeError, AttributeError):
                run_attr = getattr(run, attr)

            setattr(tc, attr, run_attr)
        setattr(tc, "flags", flags)
        return tc

    def save(self, fname : str):
        j = {}
        for attr in ["returncode", "stdout", "stderr", "flags"]:
            j[attr] = getattr(self, attr)

        mkdir_recursive(fname)
        with open(fname, 'w') as f:
            json.dump(j, f, indent=4)

def cmd_run_echoed(cmd, **kwargs):
    print("[CMD] %s" % " ".join(map(shlex.quote, cmd)))
    return subprocess.run(cmd, **kwargs)

def find_path_for_test_case(path: str) -> str:
    directory, filename = os.path.split(path)
    return (directory if directory else ".") + "/.tests_cache/" + filename + ".json"

def run_tests(file_paths: list):
    return_code = 0
    for program_file in file_paths:
        test_case_file = find_path_for_test_case(program_file)
        if os.path.exists(test_case_file):
            tc = Test_Case.from_file(test_case_file)
        else:
            continue

        flags_list = [Interpreter]
        if hasattr(tc, "flags"):
            flags_list.extend(tc.flags)
        flags_list.append(program_file)

        res = cmd_run_echoed(flags_list, capture_output=True)

        for attr in [a for a in dir(tc) if a in ["returncode", "stdout", "stderr"]]:
            tc_attr = getattr(tc, attr)
            res_attr = getattr(res, attr)
            try:
                res_attr = res_attr.decode()
            except (UnicodeDecodeError, AttributeError):
                pass

            if tc_attr != res_attr:
                print(f"[ERROR] Failed test {program_file}")
                print(f"Expected {attr} = ")
                print(tc_attr)
                print(f"Received {attr} = ")
                print(res_attr)
                return_code = 1
    exit(return_code)

def record_tests(file_paths: list):
    for program_file in file_paths:
        test_case_file = find_path_for_test_case(program_file)

        res = cmd_run_echoed([Interpreter, program_file], capture_output=True)
        tc = Test_Case.from_run(res, [])
        tc.save(test_case_file)

# list of files to test
def main():
    file_paths, mode = [], run_tests

    if len(argv) < 2:
        print("[ERROR] Expected mode argument (either 'record' or 'test')")
        exit(1)

    if argv[1] == "test":
        mode = run_tests
    elif argv[1] == "record":
        mode = record_tests
    else:
        print(f"[ERROR] Unrecognized mode '{argv[1]}'")
        exit(1)

    if len(argv) < 3:
        print("[ERROR] Expected test case")
        exit(1)

    for path in argv[2:]:
        if os.path.exists(path):
            if os.path.isdir(path):
                file_paths.extend(glob(f"{path}/*.mq"))
            else:
                file_paths.append(path)

    mode(file_paths)

if __name__ == "__main__":
    main()
