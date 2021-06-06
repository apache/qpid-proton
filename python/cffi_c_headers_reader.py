import os
import argparse


def read_files_lines(file_path):
    lines = []
    if not os.path.basename(file_path).endswith(".h"):    raise Exception("Not a header file")
    # if not os.path.isfile(file_path):   raise Exception("Not a file")
    with open(file_path) as f:
        lines = f.readlines()
    return lines


def write_file(filtered_lines, file_name):
    lines = "".join(filtered_lines).encode()
    with open(file_name,"wb") as f:
        f.write(lines)


def filter_with_define(lines):
    pass


def filter_with_pn_extern(lines):
    return [ line.replace("PN_EXTERN", "") for line in lines if line.startswith("PN_EXTERN")]


def filter_with_typedef(lines):
    # filtered_lines = []
    # for line in lines:
    #     if line.startswith("typedef") and line.endswith("{\n"):
    pass


if __name__ == "__main__":
    argparse = argparse.ArgumentParser(description="Reads c header files")
    argparse.add_argument(
        "--file_path",
        type=str,
        help="c header file"
    )
    args = argparse.parse_args()

    output_file_name = f"{os.path.basename(args.file_path.split('.')[0])}_headers.txt"
    lines = read_files_lines(args.file_path)
    write_file(
        filter_with_pn_extern(lines),
        output_file_name
    )