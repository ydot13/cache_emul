import argparse
import difflib
import os
import subprocess

import six

import exts.fs
import devtools.ya.test.system.process
import devtools.ya.test.common
import devtools.ya.test.test_types.common
import devtools.ya.test.const
import devtools.ya.test.util.shared
import devtools.ya.test.filter as test_filter
from devtools.ya.test import facility
import library.python.cores as cores


GEN_COMMENT_PREFIX = '// Code generated '
GEN_COMMENT_SUFFIX = ' DO NOT EDIT.'
SWIG_COMMENT = 'This file was automatically generated by SWIG (http://www.swig.org).'


def is_generated(lines):
    for line in lines:
        line.strip()
        if line.find(SWIG_COMMENT) >= 0:
            return True
        if line.startswith(GEN_COMMENT_PREFIX) and line.endswith(GEN_COMMENT_SUFFIX):
            return True
    return False


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--gofmt", required=True)
    parser.add_argument('--source-root', required=False, default='arcadia/')
    parser.add_argument("--out-path", help="Path to the output test_cases")
    parser.add_argument("--trace-path", help="Path to the output trace log")
    parser.add_argument("--tests-filters", required=False, action="append")
    parser.add_argument('files', nargs='*')
    args = parser.parse_args()

    tests = []

    test_cases = args.files
    if test_cases and args.tests_filters:
        filter_func = test_filter.make_testname_filter(args.tests_filters)
        test_cases = [tc for tc in test_cases if filter_func("{}::gofmt".format(os.path.basename(tc)))]

    for path in sorted(test_cases):
        go_relative_path = os.path.relpath(path, args.source_root)
        test_path = os.path.dirname(go_relative_path)
        out_lines = []
        err_lines = []
        failed = False
        cmd = [args.gofmt, path]
        elapsed = 0.0
        try:
            res = devtools.ya.test.system.process.execute(cmd, check_exit_code=True)
            elapsed = res.elapsed
            formatted = six.ensure_str(res.std_out)
            with open(path) as f:
                original = f.read()
            ok = True
            if formatted != original:
                original_lines = original.splitlines()
                if not is_generated(original_lines):
                    differ = difflib.Differ()
                    diff = "\n".join(differ.compare(formatted.splitlines(), original_lines))

                    out_lines.append(diff)
                    err_lines.append(
                        "[[imp]]Code needs to be formatted.[[rst]] Run [[alt1]]ya tool go fmt {}[[rst]]".format(
                            go_relative_path
                        )
                    )
                    failed = True
                    ok = False
            if ok:
                err_lines.append("Validated [[imp]]{}[[rst]]: [[good]]OK[[rst]]".format(go_relative_path))
        except subprocess.CalledProcessError as e:
            err_lines.append("[[imp]]Error while running [[alt1]]'{}':\n[[bad]]{}[[rst]]".format(' '.join(cmd), e))
            failed = True
        finally:
            test_name = os.path.basename(path)
            logs_dir = args.out_path
            std_out = '\n'.join(out_lines) + '\n'
            std_err = '\n'.join(err_lines) + '\n'

            logs = {'logsdir': logs_dir}

            if out_lines:
                out_path = devtools.ya.test.common.get_unique_file_path(logs_dir, "{}.gofmt.out".format(test_name))
                exts.fs.write_file(out_path, std_out, binary=False)
                logs["stdout"] = out_path
            if err_lines:
                err_path = devtools.ya.test.common.get_unique_file_path(logs_dir, "{}.gofmt.err".format(test_name))
                exts.fs.write_file(err_path, std_err, binary=False)
                logs["stderr"] = err_path

            snippet = std_err if failed else ""

            test_case = facility.TestCase(
                "{}::gofmt".format(test_name),
                devtools.ya.test.const.Status.FAIL if failed else devtools.ya.test.const.Status.GOOD,
                cores.colorize_backtrace(snippet),
                elapsed,
                logs=logs,
                path=test_path,
            )
            tests.append(test_case)

    suite = devtools.ya.test.test_types.common.PerformedTestSuite(None, None, None)
    suite.set_work_dir(os.getcwd())
    suite.register_chunk()
    suite.chunk.tests = tests
    suite.generate_trace_file(args.trace_path)

    return 0
