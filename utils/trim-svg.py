# Optimises an svg by doing the following:
# - Removing comments
# - Removing unused defs
# - Removing unused styles
# - Can optionally remove all leading whitespace


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
used_classes = []

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
                # we're in a group
                if stripped_line.startswith('<g '):
                    group_nest_level += 1
                elif stripped_line.startswith('</g>'):
                    group_nest_level -= 1
                    if group_nest_level == 0:
                        break
                elif stripped_line.startswith('<use href="#'):
                    id_str = stripped_line[12:stripped_line.find('"', 12)]
                    find_groups(id_str)

            if group_nest_level > 0:
                class_start = stripped_line.find(' class="')
                if class_start > 0:
                    classes_str = stripped_line[class_start + 8:stripped_line.find('"', class_start + 8)]
                    class_strs = classes_str.rsplit()
                    for class_str in class_strs:
                        if used_classes.count(class_str) == 0:
                            used_classes.append(class_str)

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

                class_start = stripped_line.find(' class="')
                if class_start > 0:
                    classes_str = stripped_line[class_start + 8:stripped_line.find('"', class_start + 8)]
                    class_strs = classes_str.rsplit()
                    for class_str in class_strs:
                        if used_classes.count(class_str) == 0:
                            used_classes.append(class_str)

            else:
                if stripped_line.startswith('</defs>'):
                    found_end_of_defs = True


output_lines = []

def trim(minify = False):
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
                if group_nest_level == 0:
                    if stripped_line.startswith('</style>'):
                        state = st_post_style
                    else:
                        add_line = False
                        if stripped_line.startswith('.'):
                            if stripped_line.find(',') < 0:
                                # assume single class - do we use it?
                                class_str = stripped_line[1:stripped_line.find('{', 1)].strip()
                                if used_classes.count(class_str) == 1:
                                    # we do use it
                                    add_line = True
                                    group_nest_level += 1
                            else:
                                # assume multi class
                                # add_line = True
                                class_strs = stripped_line.split(',')
                                cleaned_line = ''
                                # clean
                                for class_str in class_strs:
                                    clean_str = class_str.replace('.', ' ').replace('{', ' ').strip()
                                    if used_classes.count(clean_str) == 1:
                                        if cleaned_line != '':
                                            cleaned_line = cleaned_line + ', '
                                        cleaned_line = cleaned_line + '.' + clean_str
                                if cleaned_line != '':
                                    # add_line = True

                                    if not minify:
                                        cleaned_line = '      ' + cleaned_line
                                    output_lines.append(cleaned_line + ' {\n')
                                    group_nest_level += 1

                else:
                    if stripped_line.endswith('}'):
                        group_nest_level -= 1

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
                if minify:
                    output_lines.append(stripped_line + '\n')
                else:
                    output_lines.append(line)


# we should now have a list of id'ed groups that we need to include in the defs section
# and we can get rid of the rest

# print("Trimming `" + src_filename + "` and saving as `" + dest_filename + "` ...")

find_main_use_refs()

print("Trimming `" + src_filename + "` and saving as `" + dest_filename + "` ...")

trim(False)

# print(dest_contents)
# print('\n')
# for used_def in used_defs:
#     print(used_def)

# for used_class in used_classes:
#     print(used_class)

# Write the output file
f = open(dest_filename, "w")
for line in output_lines:
    f.write(line) 
f.close()

print("Done")
