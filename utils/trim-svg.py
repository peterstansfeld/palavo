# Optimises an svg by doing the following:
# - Removing comments
# - Removing unused defs

# Todo
# - Remove unused styles

import os
import sys

def show_help():
    print("Usage: python3 trim-svg.py SOURCE")

src_filename = sys.argv[1]
dest_filename = 'test.svg'

# Check if the file exists.
if os.path.isfile(src_filename) == False:
    print("The source file " + src_filename + " does not exist.")
    show_help()
    sys.exit(2)


line_list = []

# Read the input file.
# f = open(src_filename, "r")
# src_contents = f.read()
# f.close()


f = open(src_filename, "r")
line_list = f.readlines()
f.close()

# Global variables.

dest_contents = ""

commentless_line_list = []

for line in line_list:
    stripped_line = line.strip()
    if not stripped_line.startswith('<!--'):
        commentless_line_list.append(line)

used_defs = []

def find_groups(group_id):
    if used_defs.count(group_id) == 0:
        used_defs.append(group_id)

    group_nest_level = 0
    group_id_str = '<g id="' + group_id + '"' 
    for line in commentless_line_list:
        if not line.isspace():
            stripped_line = line.strip()

            if group_nest_level == 0:
                if stripped_line.startswith(group_id_str):
                    group_nest_level += 1
            else:
                if stripped_line.startswith('<g '):
                    group_nest_level += 1
                elif stripped_line.startswith('</g>'):
                    group_nest_level -= 1
                    if group_nest_level == 0:
                        break
                elif stripped_line.startswith('<use href="#'):
                    id_str = stripped_line[12:stripped_line.find('"', 12)]
                    find_groups(id_str)


def find_main_use_refs():
    global dest_contents
    global dest_filename

    found_end_of_defs = False

    for line in commentless_line_list:
        if not line.isspace():
            stripped_line = line.strip()
            if found_end_of_defs:
                dest_contents = dest_contents + line
                if stripped_line.startswith('<use href="#'):
                    id_str = stripped_line[12:stripped_line.find('"', 12)]
                    if used_defs.count(id_str) == 0:
                        used_defs.append(id_str)
                        find_groups(id_str)
                        dest_filename = id_str + '-circuit.svg'
            else:
                if stripped_line.startswith('</defs>'):
                    found_end_of_defs = True


output_lines = []

def trim():
    st_pre_defs = 0
    st_pre_style = 1
    st_in_style = 2
    st_post_style = 3
    st_post_defs = 4

    state = 0

    group_nest_level = 0
    for line in commentless_line_list:
        if not line.isspace():
            add_line = True
            stripped_line = line.strip()
            if state == st_pre_defs:
                if stripped_line.startswith('<defs>'):
                    state = st_pre_style
            elif state == st_pre_style:
                if stripped_line.startswith('<style>'):
                    state = st_in_style
            elif state == st_in_style:
                if stripped_line.startswith('</style>'):
                    state = st_post_style
            elif state == st_post_style:
                if group_nest_level == 0:
                    if stripped_line.startswith('<g id="'):
                        id_str = stripped_line[7:stripped_line.find('"', 7)]
                        if used_defs.count(id_str) == 1:
                            group_nest_level += 1
                        else:
                            add_line = False
                    elif stripped_line.startswith('</defs>'):
                        state = st_post_defs
                    else:
                        add_line = False
                else:
                    if stripped_line.startswith('<g '):
                        group_nest_level += 1
                    elif stripped_line.startswith('</g>'):
                        group_nest_level -= 1

            if add_line:
                output_lines.append(line)
                # output_lines.append(stripped_line + '\n')


# we should now have a list of id'ed groups that we need to include in the defs section
# and we can get rid of the rest

# print("Trimming `" + src_filename + "` and saving as `" + dest_filename + "` ...")

find_main_use_refs()

print("Trimming `" + src_filename + "` and saving as `" + dest_filename + "` ...")

trim()

# print(dest_contents)
# print('\n')
# for used_def in used_defs:
#     print(used_def)

# Write the output file
f = open(dest_filename, "w")
for line in output_lines:
    f.write(line) 
f.close()

print("Done")
