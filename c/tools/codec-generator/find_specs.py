import argparse
import json
import os
import re
import sys

from pycparser import c_ast, parse_file

from typing import Any, Dict, List, Set, Tuple, Union


def strip_quotes(arg: str) -> str:
    if arg[0] != '"' or arg[-1:] != '"':
            raise Exception(arg)
    return arg[1:-1]


def find_function_calls(file: str, name: str, includes: List[str], defines: List[str]) -> List[List[Any]]:
    class FillFinder(c_ast.NodeVisitor):
        def __init__(self):
            self._name = name
            self.result = []

        def visit_FuncCall(self, node):
            if node.name.name == self._name:
                r = []
                for e in node.args.exprs:
                    r.append(e)
                self.result.append(r)

    include_args = [f'-I{d}' for d in includes]
    define_args = [f'-D{d}' for d in defines]
    ast = parse_file(file, use_cpp=True,
                     cpp_args=[
                         *include_args,
                         *define_args
                     ])
    ff = FillFinder()
    ff.visit(ast)
    return ff.result


c_defines = ['GENERATE_CODEC_CODE',
             'NDEBUG',
             '__attribute__(X)=',
             '__asm__(X)=',
             '__inline=',
             '__extension__=',
             '__restrict=',
             '__builtin_va_list=int']


def find_fill_specs(c_includes, pn_source):
    amqp_calls = find_function_calls(os.path.join(pn_source, 'c/src/core/transport.c'), 'pn_fill_performative',
                                     c_includes, c_defines)
    sasl_calls = find_function_calls(os.path.join(pn_source, 'c/src/sasl/sasl.c'), 'pn_fill_performative',
                                     c_includes, c_defines)
    message_calls = find_function_calls(os.path.join(pn_source, 'c/src/core/message.c'), 'pn_data_fill',
                                        c_includes, c_defines)
    fill_spec_args = [c[1] for c in amqp_calls]
    fill_spec_args += [c[1] for c in sasl_calls]
    fill_spec_args += [c[1] for c in message_calls]
    fill_specs: Set[str] = \
        {strip_quotes(e.value) for e in fill_spec_args if type(e) is c_ast.Constant and e.type == 'string'}
    return fill_specs


def find_scan_specs(c_includes, pn_source):
    amqp_calls = find_function_calls(os.path.join(pn_source, 'c/src/core/transport.c'), 'pn_data_scan',
                                     c_includes, c_defines)
    message_calls = find_function_calls(os.path.join(pn_source, 'c/src/core/message.c'),'pn_data_scan',
                                        c_includes, c_defines)
    scan_spec_args = [c[1] for c in amqp_calls]
    scan_spec_args += [c[1] for c in message_calls]
    # Only try to generate code for constant strings
    scan_specs: Set[str] = \
        {strip_quotes(e.value) for e in scan_spec_args if type(e) is c_ast.Constant and e.type == 'string'}
    return scan_specs


def main():
    argparser = argparse.ArgumentParser(description='Find scan/fill specs in proton code for code generation')
    argparser.add_argument('-o', '--output', help='output json file', type=str, required=True)
    argparser.add_argument('-s', '--source', help='proton source directory', type=str, required=True)
    argparser.add_argument('-b', '--build', help='proton build directory', type=str, required=True)

    args = argparser.parse_args()

    pn_source = args.source
    pn_build = args.build
    json_filename = args.output

    c_includes = [f'{os.path.join(pn_build, "c/include")}',
                  f'{os.path.join(pn_build, "c/src")}']
    fill_specs = find_fill_specs(c_includes, pn_source)
    scan_specs = find_scan_specs(c_includes, pn_source)
    with open(json_filename, 'w') as file:
        json.dump({'fill_specs': sorted(list(fill_specs)), 'scan_specs': sorted(list(scan_specs))}, file, indent=2)


if __name__ == '__main__':
    sys.argv[0] = re.sub(r'(-script\.pyw?|\.exe)?$', '', sys.argv[0])
    sys.exit(main())
