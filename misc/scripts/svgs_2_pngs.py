# -*- coding: utf-8 -*-

# Basic exporter for svg icons

from os import listdir
from os.path import isfile, join, dirname, realpath
import subprocess
import sys

# import gi
# gi.require_version('Rsvg', '2.0')
# from gi.repository import Rsvg as rsvg
# import cairo
import cairosvg

last_svg_path = None
last_svg_data = None

SCRIPT_FOLDER = dirname(realpath(__file__)) + '/'
icons_dir_base = join(SCRIPT_FOLDER,'../../editor/icons/')
icons_dir_2x = join(icons_dir_base,'2x/')
icons_dir_source = join(icons_dir_base + 'source/')


def export_icons():
    svgs_path = icons_dir_source

    file_names = [f for f in listdir(svgs_path) if isfile(join(svgs_path, f))]

    for file_name in file_names:
        # name without extensions
        name_only = file_name.replace('.svg', '')

        out_icon_names = [name_only]  # export to a png with the same file name

        source_path = '%s%s.svg' % (svgs_path, name_only)
        print(source_path)
        print(out_icon_names)
        for out_icon_name in out_icon_names:
            cairosvg.svg2png(url=source_path, write_to=join(icons_dir_base,out_icon_name + ".png"), dpi=90)
            cairosvg.svg2png(url=source_path, write_to=join(icons_dir_2x,out_icon_name + ".png"), dpi=180)


export_icons()
