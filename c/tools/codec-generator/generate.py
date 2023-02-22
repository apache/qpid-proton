import argparse
import itertools
import json
import os
import re
import sys

from typing import Any, Dict, List, Set, Tuple, Union


class ParseError(Exception):
    def __init__(self, error):
        super().__init__(error)


indent_size = 4


class ASTNode:
    def __init__(self, function_suffix: str, types: List[str], consume_types: Union[List[str], None] = None):
        self.function_suffix = function_suffix
        self.types = types
        self.consume_types = consume_types
        self.count_args = len(self.types)
        self.count_consume_args = len(self.consume_types) if self.consume_types else len(self.types)

    @staticmethod
    def mk_indent(indent: int):
        return " "*indent*indent_size

    @staticmethod
    def mk_funcall(name: str, args: List[str]):
        return f'{name}({", ".join(args)})'

    def gen_emit_code(self, prefix: List[str], first_arg: int, indent: int) -> List[str]:
        return [f'{self.mk_indent(indent)}{self.mk_funcall("emit_"+self.function_suffix, prefix + self.gen_args(first_arg))};']

    def gen_params(self, first_arg: int) -> List[Tuple[str, str]]:
        return [(f'arg{i+first_arg}', self.types[i]) for i in range(len(self.types))]

    def gen_args(self, first_arg: int) -> List[str]:
        return [f'arg{i+first_arg}' for i in range(len(self.types))]

    def gen_consume_code(self, prefix: List[str], first_arg: int, indent: int) -> List[str]:
        return [f'{self.mk_indent(indent)}{self.mk_funcall("consume_"+self.function_suffix, prefix + self.gen_consume_args(first_arg))};']

    @staticmethod
    def add_pointer(type: str) -> str:
        return f'{type}*'

    def gen_consume_params(self, first_arg: int) -> List[Tuple[str, str]]:
        if self.consume_types:
            return [(f'arg{i + first_arg}', self.consume_types[i]) for i in range(len(self.consume_types))]
        else:
            return [(f'arg{i + first_arg}', self.add_pointer(self.types[i])) for i in range(len(self.types))]

    def gen_consume_args(self, first_arg: int) -> List[str]:
        if self.consume_types:
            return [f'arg{i+first_arg}' for i in range(len(self.consume_types))]
        else:
            return [f'arg{i+first_arg}' for i in range(len(self.types))]

    def __repr__(self) -> str:
        return f'<{self.function_suffix}: {self.types}>'


class NullNode(ASTNode):
    def __init__(self, function_suffix: str):
        super().__init__(function_suffix, [])

    def gen_emit_code(self, prefix: List[str], first_arg: int, indent: int) -> List[str]:
        return [f'{self.mk_indent(indent)}{self.mk_funcall("emit_"+self.function_suffix, prefix)};']

    def gen_params(self, first_arg: int) -> List[Tuple[str, str]]:
        return []


class ListNode(ASTNode):
    def __init__(self, name: str, l: List[ASTNode], types: List[str]):
        self.list: List[ASTNode] = l
        super().__init__(name, types)
        self.count_args += sum([i.count_args for i in l])
        self.count_consume_args += sum([i.count_consume_args for i in l])

    def gen_params_list(self, first_arg: int) -> List[Tuple[str, str]]:
        params = []
        arg = first_arg
        for n in self.list:
            r = n.gen_params(arg)
            arg += len(r)
            params.append(r)
        return [i for i in itertools.chain(*params)]

    def gen_params(self, first_arg: int) -> List[Tuple[str, str]]:
        args = super().gen_params(first_arg)
        return [
            *args,
            *self.gen_params_list(first_arg + len(args))
        ]

    def gen_consume_params_list(self, first_arg: int) -> List[Tuple[str, str]]:
        params = []
        arg = first_arg
        for n in self.list:
            r = n.gen_consume_params(arg)
            arg += len(r)
            params.append(r)
        return [i for i in itertools.chain(*params)]

    def gen_consume_params(self, first_arg: int) -> List[Tuple[str, str]]:
        args = super().gen_consume_params(first_arg)
        return [
            *args,
            *self.gen_consume_params_list(first_arg + len(args))
        ]

    def gen_emit_list_code(self, prefix: List[str], first_arg: int, indent: int) -> List[str]:
        lines = []
        arg = first_arg
        for n in self.list:
            lines.append(n.gen_emit_code(prefix, arg, indent))
            arg += n.count_args
        return [i for i in itertools.chain(*lines)]

    def gen_emit_code(self, prefix: List[str], first_arg: int, indent: int) -> List[str]:
        return [
            f'{self.mk_indent(indent)}for (bool small_encoding = true; ; small_encoding = false) {{',
            f'{self.mk_indent(indent+1)}pni_compound_context c = '
            f'{self.mk_funcall("emit_list", prefix+["small_encoding", "true"])};',
            f'{self.mk_indent(indent+1)}pni_compound_context compound = c;',
            *(self.gen_emit_list_code(prefix, first_arg, indent + 1)),
            f'{self.mk_indent(indent+1)}{self.mk_funcall("emit_end_list", prefix+["small_encoding"])};',
            f'{self.mk_indent(indent+1)}if (encode_succeeded({", ".join(prefix)})) break;',
            f'{self.mk_indent(indent)}}}',
        ]

    def gen_consume_code(self, prefix: List[str], first_arg: int, indent: int) -> List[str]:
        lines = []
        arg = first_arg
        for n in self.list:
            lines.append(n.gen_consume_code(prefix, arg, indent+1))
            arg += n.count_consume_args
        return [
            f'{self.mk_indent(indent)}{{',
            f'{self.mk_indent(indent+1)}pni_consumer_t subconsumer;',
            f'{self.mk_indent(indent+1)}uint32_t count;',
            f'{self.mk_indent(indent+1)}{self.mk_funcall("consume_list", prefix+["&subconsumer", "&count"])};',
            f'{self.mk_indent(indent+1)}pni_consumer_t consumer = subconsumer;',
            *[i for i in itertools.chain(*lines)],
            f'{self.mk_indent(indent+1)}{self.mk_funcall("consume_end_list", prefix)};',
            f'{self.mk_indent(indent)}}}'
        ]

    def __repr__(self) -> str:
        nl = "\n    "
        return f'''<{self.function_suffix}: {self.types}
    {nl.join([repr(x) for x in self.list])}
>'''


class DescListNode(ListNode):
    def __init__(self, l: List[ASTNode]):
        super().__init__('described_list', l, ['uint64_t'])

    def gen_emit_code(self, prefix: List[str], first_arg: int, indent: int) -> List[str]:
        args = self.gen_args(first_arg)
        return [
            f'{self.mk_indent(indent)}emit_descriptor({", ".join(prefix+args)});',
            *super().gen_emit_code(prefix, first_arg+len(args), indent),
        ]


class DescListIgnoreTypeNode(ListNode):
    def __init__(self, l: List[ASTNode]):
        super().__init__('described_unknown_list', l, [])

    def gen_consume_code(self, prefix: List[str], first_arg: int, indent: int) -> List[str]:
        return [
            f'{self.mk_indent(indent)}{{',
            f'{self.mk_indent(indent+1)}pni_consumer_t subconsumer;',
            f'{self.mk_indent(indent+1)}{self.mk_funcall("consume_described", prefix+["&subconsumer"])};',
            f'{self.mk_indent(indent+1)}pni_consumer_t consumer = subconsumer;',
            *super().gen_consume_code(prefix, first_arg, indent + 1),
            f'{self.mk_indent(indent)}}}',
        ]


class ArrayNode(ListNode):
    def __init__(self, l: List[ASTNode]):
        self.list: List[ASTNode] = l
        super().__init__('array', l, ['pn_type_t'])

    def gen_emit_code(self, prefix: List[str], first_arg: int, indent: int) -> List[str]:
        args = self.gen_args(first_arg)
        return [
            f'{self.mk_indent(indent)}for (bool small_encoding = true; ; small_encoding = false) {{',
            f'{self.mk_indent(indent+1)}pni_compound_context c = '
            f'{self.mk_funcall("emit_array", prefix+["small_encoding"]+args)};',
            f'{self.mk_indent(indent+1)}pni_compound_context compound = c;',
            *super().gen_emit_list_code(prefix, first_arg+len(args), indent + 1),
            f'{self.mk_indent(indent+1)}{self.mk_funcall("emit_end_array", prefix+["small_encoding"])};',
            f'{self.mk_indent(indent+1)}if (encode_succeeded({", ".join(prefix)})) break;',
            f'{self.mk_indent(indent)}}}',
        ]


class OptionNode(ASTNode):
    def __init__(self, i: ASTNode):
        self.option: ASTNode = i
        super().__init__('option', ['bool'])
        self.count_args += i.count_args
        self.count_consume_args += i.count_consume_args

    def gen_emit_code(self, prefix: List[str], first_arg: int, indent: int) -> List[str]:
        arg = f'arg{first_arg}'
        return [
            f'{self.mk_indent(indent)}if ({arg}) {{',
            *self.option.gen_emit_code(prefix, first_arg+1, indent + 1),
            f'{self.mk_indent(indent)}}} else {{',
            *NullNode("null").gen_emit_code(prefix, 0, indent + 1),
            f'{self.mk_indent(indent)}}}'
        ]

    def gen_params(self, first_arg: int) -> List[Tuple[str, str]]:
        args = super().gen_params(first_arg)
        return [
            *args,
            *self.option.gen_params(first_arg + len(args))
        ]

    def gen_consume_code(self, prefix: List[str], first_arg: int, indent: int) -> List[str]:
        arg = f'arg{first_arg}'
        return [
            f'{self.mk_indent(indent)}*{arg} = {self.option.gen_consume_code(prefix, first_arg+1, 0)[0]};'
        ]

    def gen_consume_params(self, first_arg: int) -> List[Tuple[str, str]]:
        args = super().gen_consume_params(first_arg)
        return [
            *args,
            *self.option.gen_consume_params(first_arg + len(args))
        ]


class EmptyNode(ASTNode):
    def __init__(self):
        super().__init__('empty_frame', [])

    def gen_emit_code(self, prefix: List[str], first_arg: int, indent: int) -> List[str]:
        return []

    def gen_params(self, first_arg: int) -> List[Tuple[str, str]]:
        return []


def expect_char(format: str, char: str) -> Tuple[bool, str]:
    return format[0] == char, format[1:]


def parse_list(format: str) -> Tuple[List[ASTNode], str]:
    rest = format
    list = []
    while not rest[0] == ']':
        i, rest, = parse_item(rest)
        list.append(i)
    return list, rest


def parse_item(format: str) -> Tuple[ASTNode, str]:
    if len(format) == 0:
        return EmptyNode(), ''
    if format.startswith('DL['):
        l, rest = parse_list(format[3:])
        b, rest = expect_char(rest, ']')
        if not b:
            raise ParseError(format)
        return DescListNode(l), rest
    elif format.startswith('D.['):
        l, rest = parse_list(format[3:])
        b, rest = expect_char(rest, ']')
        if not b:
            raise ParseError(format)
        return DescListIgnoreTypeNode(l), rest
    elif format.startswith('['):
        l, rest = parse_list(format[1:])
        b, rest = expect_char(rest, ']')
        if not b:
            raise ParseError(format)
        return ListNode('list', l, []), rest
    elif format.startswith('D?LR'):
        return ASTNode('described_maybe_type_raw', ['bool', 'uint64_t', 'pn_bytes_t'], consume_types=['bool*', 'uint64_t*', 'pn_bytes_t*']), format[4:]
    elif format.startswith('D?L?.'):
        return ASTNode('described_maybe_type_maybe_anything', ['bool', 'uint64_t', 'bool'], consume_types=['bool*', 'uint64_t*', 'bool*']), format[4:]
    elif format.startswith('DLC'):
        return ASTNode('described_type_copy', ['uint64_t', 'pn_data_t*'], consume_types=['uint64_t*', 'pn_data_t*']), format[3:]
    elif format.startswith('DLR'):
        return ASTNode('described_type_raw', ['uint64_t', 'pn_bytes_t'], consume_types=['uint64_t*', 'pn_bytes_t*']), format[3:]
    elif format.startswith('DL.'):
        return ASTNode('described_type_anything', ['uint64_t']), format[3:]
    elif format.startswith('D?L.'):
        return ASTNode('described_maybe_type_anything', ['bool', 'uint64_t']), format[3:]
    elif format.startswith('D..'):
        return NullNode('described_anything'), format[3:]
    elif format.startswith('D.C'):
        return ASTNode('described_copy', ['pn_data_t*'], consume_types=['pn_data_t*']), format[3:]
    elif format.startswith('D.R'):
        return ASTNode('described_raw', ['pn_bytes_t'], consume_types=['pn_bytes_t*']), format[3:]
    elif format.startswith('@T['):
        l, rest = parse_list(format[3:])
        b, rest = expect_char(rest, ']')
        if not b:
            raise ParseError(format)
        return ArrayNode(l), rest
    elif format.startswith('?'):
        i, rest = parse_item(format[1:])
        return OptionNode(i), rest
    elif format.startswith('*s'):
        return ASTNode('counted_symbols', ['size_t', 'char**']), format[2:]
    elif format.startswith('.'):
        return NullNode('anything'), format[1:]
    elif format.startswith('s'):
        return ASTNode('symbol', ['pn_bytes_t'], consume_types=['pn_bytes_t*']), format[1:]
    elif format.startswith('S'):
        return ASTNode('string', ['pn_bytes_t'], consume_types=['pn_bytes_t*']), format[1:]
    elif format.startswith('c'):
        return ASTNode('condition', ['pn_condition_t*'], consume_types=['pn_condition_t*']), format[1:]
    elif format.startswith('d'):
        return ASTNode('disposition', ['pn_disposition_t*'], consume_types=['pn_disposition_t*']), format[1:]
    elif format.startswith('I'):
        return ASTNode('uint', ['uint32_t']), format[1:]
    elif format.startswith('H'):
        return ASTNode('ushort', ['uint16_t']), format[1:]
    elif format.startswith('n'):
        return NullNode('null'), format[1:]
    elif format.startswith('R'):
        return ASTNode('raw', ['pn_bytes_t'], consume_types=['pn_bytes_t*']), format[1:]
    elif format.startswith('a'):
        return ASTNode('atom', ['pn_atom_t*'], consume_types=['pn_atom_t*']), format[1:]
    elif format.startswith('M'):
        return ASTNode('multiple', ['pn_bytes_t']), format[1:]
    elif format.startswith('o'):
        return ASTNode('bool', ['bool']), format[1:]
    elif format.startswith('B'):
        return ASTNode('ubyte', ['uint8_t']), format[1:]
    elif format.startswith('L'):
        return ASTNode('ulong', ['uint64_t']), format[1:]
    elif format.startswith('t'):
        return ASTNode('timestamp', ['pn_timestamp_t']), format[1:]
    elif format.startswith('z'):
        return ASTNode('binaryornull', ['size_t', 'const char*'], consume_types=['pn_bytes_t*']), format[1:]
    elif format.startswith('Z'):
        return ASTNode('binarynonull', ['size_t', 'const char*'], consume_types=['pn_bytes_t*']), format[1:]
    else:
        raise ParseError(format)


# Need to translate '@[]*?.' to legal identifier characters
# These will be fairly arbitrary and just need to avoid already used codes
def make_legal_identifier(s: str) -> str:
    subs = {'@': 'A', '[': 'E', ']': 'e', '*': 'j', '.': 'q', '?': 'Q'}
    r = ''
    for c in s:
        if c in subs:
            r += subs[c]
        else:
            r += c
    return r


def consume_function(name_prefix: str, scan_spec: str, prefix_args: List[Tuple[str, str]]) -> Tuple[str, List[str]]:
    p, _ = parse_item(scan_spec)
    params = p.gen_consume_params(0)
    function_name = name_prefix + '_' + make_legal_identifier(scan_spec)
    param_list = ', '.join([t+' '+arg for arg, t in prefix_args+params])
    function_spec = f'size_t {function_name}({param_list})'

    function_decl = f'{function_spec};'

    prefix_params = [a for (a, _) in prefix_args]
    function_defn = [
        f'{function_spec}',
        '{',
        f'{p.mk_indent(1)}pni_consumer_t consumer = make_consumer_from_bytes({", ".join(prefix_params)});',
        *p.gen_consume_code(['&consumer'], 0, 1),
        f'{p.mk_indent(1)}return consumer.position;',
        '}',
        ''
    ]
    return function_decl, function_defn


def emit_function(name_prefix: str, fill_spec: str, prefix_args: List[Tuple[str, str]]) -> Tuple[str, List[str]]:
    p, _ = parse_item(fill_spec)
    params = p.gen_params(0)
    function_name = name_prefix + '_' + make_legal_identifier(fill_spec)
    param_list = ', '.join([t+' '+arg for arg, t in prefix_args+params])
    function_spec = f'pn_bytes_t {function_name}({param_list})'

    bytes_args = [('bytes', 'char*'), ('size', 'size_t')]
    bytes_function_name = name_prefix + '_bytes_' + make_legal_identifier(fill_spec)
    bytes_param_list = ', '.join([t+' '+arg for arg, t in bytes_args+params])
    bytes_function_spec = f'size_t {bytes_function_name}({bytes_param_list})'

    inner_function_name = name_prefix + '_inner_' + make_legal_identifier(fill_spec)
    inner_param_list = [('emitter', 'pni_emitter_t*')]+params
    inner_params = ', '.join([t+' '+arg for arg, t in inner_param_list])
    inner_function_spec = f'bool {inner_function_name}({inner_params})'

    function_decl = f'{function_spec};\n{bytes_function_spec};'

    prefix_params = [a for (a, _) in prefix_args]
    args = [a for (a, _) in params]
    if type(p) is EmptyNode:
        function_defn = [
            f'{function_spec}',
            '{',
            f'{p.mk_indent(1)}pni_emitter_t emitter = make_emitter({", ".join(prefix_params)});',
            f'{p.mk_indent(1)}return make_return(emitter);',
            '}',
            ''
        ]
    else:
        function_defn = [
            f'{inner_function_spec}',
            '{',
            f'{p.mk_indent(1)}pni_compound_context compound = make_compound();',
            *p.gen_emit_code(['emitter', '&compound'], 0, 1),
            f'{p.mk_indent(1)}return resize_required(emitter);',
            '}',
            '',
            f'{function_spec}',
            '{',
            f'{p.mk_indent(1)}do {{',
            f'{p.mk_indent(2)}pni_emitter_t emitter = make_emitter_from_rwbytes({", ".join(prefix_params)});',
            f'{p.mk_indent(2)}if ({p.mk_funcall(inner_function_name, ["&emitter", *args])}) {{',
            f'{p.mk_indent(3)}{p.mk_funcall("size_buffer_to_emitter", [*prefix_params, "&emitter"])};',
            f'{p.mk_indent(3)}continue;',
            f'{p.mk_indent(2)}}}',
            f'{p.mk_indent(2)}return make_bytes_from_emitter(emitter);',
            f'{p.mk_indent(1)}}} while (true);',
            f'{p.mk_indent(1)}/*Unreachable*/',
            '}',
            ''
            f'{bytes_function_spec}',
            '{',
            f'{p.mk_indent(1)}pni_emitter_t emitter = make_emitter_from_bytes((pn_rwbytes_t){{.size=size, .start=bytes}});',
            f'{p.mk_indent(1)}{p.mk_funcall(inner_function_name, ["&emitter", *args])};',
            f'{p.mk_indent(1)}return make_bytes_from_emitter(emitter).size;',
            '}',
            ''
        ]
    return function_decl, function_defn


prefix_emit_header = """
#include "proton/codec.h"
#include "proton/condition.h"
#include "proton/disposition.h"
#include "buffer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

"""

prefix_consume_header = """
#include "proton/codec.h"
#include "proton/condition.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

"""


def output_header(decls: Dict[str, str], prefix: str, file=None):
    # Output declarations
    print(prefix, file=file)
    for fill_spec, decl in sorted(decls.items()):
        print(
            f'/* {fill_spec} */\n'
            f'{decl}',
            file=file
        )


prefix_emit_implementation = """
#include "core/emitters.h"
"""

prefix_consume_implementation = """
#include "core/consumers.h"
"""


def output_implementation(defns: Dict[str, List[str]], prefix: str, header=None, file=None):
    # Output implementations
    if header:
        print(f'#include "{header}"', file=file)

    print(prefix, file=file)
    for fill_spec, defn in sorted(defns.items()):
        printable_defn = '\n'.join(defn)
        print(
            f'/* {fill_spec} */\n'
            f'{printable_defn}',
            file=file
        )


def emit(fill_specs, decl_filename, impl_filename):
    decls: Dict[str, str] = {}
    defns: Dict[str, List[str]] = {}
    for fill_spec in fill_specs:
        decl, defn = emit_function('pn_amqp_encode', fill_spec, [('buffer', 'pn_rwbytes_t*')])
        decls[fill_spec] = decl
        defns[fill_spec] = defn
    if decl_filename and impl_filename:
        with open(decl_filename, 'w') as dfile:
            output_header(decls, prefix_emit_header, file=dfile)
        with open(impl_filename, 'w') as ifile:
            output_implementation(defns, prefix_emit_implementation, header=os.path.basename(decl_filename), file=ifile)
    else:
        output_header(decls, prefix_emit_header)
        output_implementation(defns, prefix_emit_implementation)


def consume(scan_specs, decl_filename, impl_filename):

    decls: Dict[str, str] = {}
    defns: Dict[str, List[str]] = {}
    for scan_spec in scan_specs:
        decl, defn = consume_function('pn_amqp_decode', scan_spec, [('bytes', 'pn_bytes_t')])
        decls[scan_spec] = decl
        defns[scan_spec] = defn
    if decl_filename and impl_filename:
        with open(decl_filename, 'w') as dfile:
            output_header(decls, prefix_consume_header, file=dfile)
        with open(impl_filename, 'w') as ifile:
            output_implementation(defns, prefix_consume_implementation, header=os.path.basename(decl_filename), file=ifile)
    else:
        output_header(decls, prefix_consume_header)
        output_implementation(defns, prefix_consume_implementation)


def main():
    argparser = argparse.ArgumentParser(description='Generate AMQP codec in C for data scan/fill function calls')
    argparser.add_argument('-i', '--input_specs', help='json file with specs to generate codec output code', type=str, required=True)
    argparser.add_argument('-o', '--output-base', help='basename for output code', type=str)
    group = argparser.add_mutually_exclusive_group(required=True)
    group.add_argument('-e', '--emit', help='generate code to emit amqp', action='store_true')
    group.add_argument('-c', '--consume', help='generate code to consume amqp', action='store_true')

    args = argparser.parse_args()

    if args.output_base:
        decl_filename = args.output_base + '.h'
        impl_filename = args.output_base + '.c'
    else:
        decl_filename = None
        impl_filename = None

    with open(args.input_specs, 'r') as file:
        jsonfile = json.load(file)
        if args.emit:
            fill_specs = jsonfile['fill_specs']
            emit(fill_specs, decl_filename, impl_filename)
        elif args.consume:
            scan_specs = jsonfile['scan_specs']
            consume(scan_specs, decl_filename, impl_filename)


if __name__ == '__main__':
    sys.argv[0] = re.sub(r'(-script\.pyw?|\.exe)?$', '', sys.argv[0])
    sys.exit(main())
