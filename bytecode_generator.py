import os
from pathlib import Path
import json
bytecode_diff_list = [
    {
        "rev": "f3f05dc",
        "removed_tokens": ["SYNC", "SLAVE"],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "506df14",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": ["decimals"],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "a7aad78",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["deep_equal"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "5565f55",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["ord"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "6694c11",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["lerp_angle"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "a60f242",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["posmod"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "c00427a",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["move_toward"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "620ec47",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["step_decimals"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "7f7d97f",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["is_equal_approx", "is_zero_approx"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "514a3fb",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["smoothstep"],
        "arg_count_changed": ["var2bytes", "bytes2var"],
        "tokens_renamed": [],
    },
    {
        "rev": "1a36141",
        "removed_tokens": ["DO", "CASE", "SWITCH"],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "1ca61a3",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["push_error", "push_warning"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "d6b31da",
        "removed_tokens": [],
        "added_tokens": ["PUPPET"],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [{"SLAVESYNC": "PUPPETSYNC"}],
    },
    {
        "rev": "8aab9a0",
        "removed_tokens": [],
        "added_tokens": ["AS", "VOID", "FORWARD_ARROW"],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "a3f1ee5",
        "removed_tokens": [],
        "added_tokens": ["CLASS_NAME"],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "8e35d93",
        "removed_tokens": [],
        "added_tokens": ["REMOTESYNC", "MASTERSYNC", "SLAVESYNC"],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "3ea6d9f",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["print_debug"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "a56d6ff",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["get_stack"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "ff1e7cf",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["is_instance_valid"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "054a2ac",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["polar2cartesian", "cartesian2polar"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "91ca725",
        "removed_tokens": [],
        "added_tokens": ["CONST_TAU"],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "216a8aa",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["wrapi", "wrapf"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "d28da86",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["inverse_lerp", "range_lerp"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "c6120e7",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["len"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "015d36d",
        "removed_tokens": [],
        "added_tokens": ["IS"],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "5e938f0",
        "removed_tokens": [],
        "added_tokens": ["CONST_INF", "CONST_NAN"],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "c24c739",
        "removed_tokens": [],
        "added_tokens": ["WILDCARD"],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "f8a7c46",
        "removed_tokens": [],
        "added_tokens": ["MATCH"],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "62273e5",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["validate_json", "parse_json", "to_json"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "8b912d1",
        "removed_tokens": [],
        "added_tokens": ["DOLLAR"],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "23381a5",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["ColorN"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "513c026",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["char"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "4ee82a2",
        "removed_tokens": [],
        "added_tokens": ["ENUM"],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "1add52b",
        "removed_tokens": [],
        "added_tokens": ["REMOTE", "SYNC", "MASTER", "SLAVE"],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "ed80f45",
        "removed_tokens": [],
        "added_tokens": ["ENUM"],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "85585c7",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["ColorN"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "7124599",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["type_exists"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "23441ec",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["var2bytes", "bytes2var"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "6174585",
        "removed_tokens": [],
        "added_tokens": ["CONST_PI"],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "64872ca",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["Color8"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "7d2d144",
        "removed_tokens": [],
        "added_tokens": ["BREAKPOINT"],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "30c1229",
        "removed_tokens": [],
        "added_tokens": ["ONREADY"],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "48f1d02",
        "removed_tokens": [],
        "added_tokens": ["SIGNAL"],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "65d48d6",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["prints"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "be46be7",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [("get_inst", "instance_from_id")],
        "tokens_renamed": [],
    },
    {
        "rev": "97f34a1",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["seed", "get_inst"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "2185c01",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["var2str", "str2var"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "e82dc40",
        "removed_tokens": [],
        "added_tokens": ["SETGET"],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "8cab401",
        "removed_tokens": [],
        "added_tokens": ["YIELD"],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "703004f",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["hash"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "31ce3c5",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["funcref"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "8c1731b",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": ["load"],
        "arg_count_changed": [],
        "tokens_renamed": [],
    },
    {
        "rev": "0b806ee",
        "removed_tokens": [],
        "added_tokens": [],
        "removed_functions": [],
        "added_functions": [],
        "arg_count_changed": [],
        "tokens_renamed": [],
    }
]

bytecode_date_dict = {
    "f3f05dc": {
        "version": "4.0-dev",
        "date": "2020-02-13"
    },
    "506df14": {
        "version": "4.0-dev",
        "date": "2020-02-12"
    },
    "a7aad78": {
        "version": "3.5.0-stable",
        "date": "2020-10-07"
    },
    "5565f55": {
        "version": "3.2.0-stable",
        "date": "2019-08-26"
    },
    "6694c11": {
        "version": "3.2-dev",
        "date": "2019-07-20"
    },
    "a60f242": {
        "version": "3.2-dev",
        "date": "2019-07-19"
    },
    "c00427a": {
        "version": "3.2-dev",
        "date": "2019-06-01"
    },
    "620ec47": {
        "version": "3.2-dev",
        "date": "2019-05-01"
    },
    "7f7d97f": {
        "version": "3.2-dev",
        "date": "2019-04-29"
    },
    "514a3fb": {
        "version": "3.1.1-stable",
        "date": "2019-03-19"
    },
    "1a36141": {
        "version": "3.1.0-stable",
        "date": "2019-02-20"
    },
    "1ca61a3": {
        "version": "3.1-beta5",
        "date": "2018-10-31"
    },
    "d6b31da": {
        "version": "3.1-dev",
        "date": "2018-09-15"
    },
    "8aab9a0": {
        "version": "3.1-dev",
        "date": "2018-07-20"
    },
    "a3f1ee5": {
        "version": "3.1-dev",
        "date": "2018-07-15"
    },
    "8e35d93": {
        "version": "3.1-dev",
        "date": "2018-05-29"
    },
    "3ea6d9f": {
        "version": "3.1-dev",
        "date": "2018-05-28"
    },
    "a56d6ff": {
        "version": "3.1-dev",
        "date": "2018-05-17"
    },
    "ff1e7cf": {
        "version": "3.1-dev",
        "date": "2018-05-07"
    },
    "054a2ac": {
        "version": "3.0.0-stable",
        "min_version": "3.0.0-stable",
        "max_version": "3.0.6-stable",
        "date": "2017-11-20"
    },
    "91ca725": {
        "version": "3.0-dev",
        "date": "2017-11-12"
    },
    "216a8aa": {
        "version": "3.0-dev",
        "date": "2017-10-13"
    },
    "d28da86": {
        "version": "3.0-dev",
        "date": "2017-08-18"
    },
    "c6120e7": {
        "version": "3.0-dev",
        "date": "2017-08-07"
    },
    "015d36d": {
        "version": "3.0-dev",
        "date": "2017-05-27"
    },
    "5e938f0": {
        "version": "3.0-dev",
        "date": "2017-02-28"
    },
    "c24c739": {
        "version": "3.0-dev",
        "date": "2017-01-20"
    },
    "f8a7c46": {
        "version": "3.0-dev",
        "date": "2017-01-11"
    },
    "62273e5": {
        "version": "3.0-dev",
        "date": "2017-01-08"
    },
    "8b912d1": {
        "version": "3.0-dev",
        "date": "2017-01-08"
    },
    "23381a5": {
        "version": "3.0-dev",
        "date": "2016-12-17"
    },
    "513c026": {
        "version": "3.0-dev",
        "date": "2016-10-03"
    },
    "4ee82a2": {
        "version": "3.0-dev",
        "date": "2016-08-27"
    },
    "1add52b": {
        "version": "3.0-dev",
        "date": "2016-08-19"
    },
    "ed80f45": {
        "version": "2.1.3-stable",
        "min_version": "2.1.3-stable",
        "max_version": "2.1.6-stable",
        "date": "2017-04-06"
    },
    "85585c7": {
        "version": "2.1.2-stable",
        "date": "2017-01-12"
    },
    "7124599": {
        "version": "2.1.0-stable",
        "min_version": "2.1.0-stable",
        "max_version": "2.1.1-stable",
        "date": "2016-06-18"
    },
    "23441ec": {
        "version": "2.0.0-stable",
        "min_version": "2.0.0-stable",
        "max_version": "2.0.4-stable",
        "date": "2016-01-02"
    },
    "6174585": {
        "version": "2.0-dev",
        "date": "2016-01-02"
    },
    "64872ca": {
        "version": "2.0-dev",
        "date": "2015-12-31"
    },
    "7d2d144": {
        "version": "2.0-dev",
        "date": "2015-12-29"
    },
    "30c1229": {
        "version": "2.0-dev",
        "date": "2015-12-28"
    },
    "48f1d02": {
        "version": "2.0-dev",
        "date": "2015-06-24"
    },
    "65d48d6": {
        "version": "1.1.0-stable",
        "date": "2015-05-09"
    },
    "be46be7": {
        "version": "1.1-dev",
        "date": "2015-04-18"
    },
    "97f34a1": {
        "version": "1.1-dev",
        "date": "2015-03-25"
    },
    "2185c01": {
        "version": "1.1-dev",
        "date": "2015-02-15"
    },
    "e82dc40": {
        "version": "1.0.0-stable",
        "date": "2014-10-27"
    },
    "8cab401": {
        "version": "1.0-dev",
        "date": "2014-09-15"
    },
    "703004f": {
        "version": "1.0-dev",
        "date": "2014-06-16"
    },
    "31ce3c5": {
        "version": "1.0-dev",
        "date": "2014-03-13"
    },
    "8c1731b": {
        "version": "1.0-dev",
        "date": "2014-02-15"
    },
    "0b806ee": {
        "version": "1.0-dev",
        "date": "2014-02-09"
    }
}
# for the above list, take the revision and stick it in a dictionary
bytecode_stuff_dict = {}
for item in bytecode_diff_list:
    bytecode_stuff_dict[item["rev"]] = item
    bytecode_stuff_dict[item["rev"]]["version"] = bytecode_date_dict[item["rev"]]["version"]
    bytecode_stuff_dict[item["rev"]]["date"] = bytecode_date_dict[item["rev"]]["date"]
    bytecode_stuff_dict[item["rev"]]["is_dev"] = "dev" in bytecode_date_dict[item["rev"]]["version"]
    if "min_version" in bytecode_date_dict[item["rev"]]:
        bytecode_stuff_dict[item["rev"]]["min_version"] = bytecode_date_dict[item["rev"]]["min_version"]
    else:
        bytecode_stuff_dict[item["rev"]]["min_version"] = bytecode_date_dict[item["rev"]]["version"]
    if "max_version" in bytecode_date_dict[item["rev"]]:
        bytecode_stuff_dict[item["rev"]]["max_version"] = bytecode_date_dict[item["rev"]]["max_version"]
    else:
        bytecode_stuff_dict[item["rev"]]["max_version"] = bytecode_date_dict[item["rev"]]["version"]



builtin_func_arg_elements = [
    ("sin", (1, 1)),
    ("cos", (1, 1)),
    ("tan", (1, 1)),
    ("sinh", (1, 1)),
    ("cosh", (1, 1)),
    ("tanh", (1, 1)),
    ("asin", (1, 1)),
    ("acos", (1, 1)),
    ("atan", (1, 1)),
    ("atan2", (2, 2)),
    ("sqrt", (1, 1)),
    ("fmod", (2, 2)),
    ("fposmod", (2, 2)),
    ("posmod", (2, 2)),
    ("floor", (1, 1)),
    ("ceil", (1, 1)),
    ("round", (1, 1)),
    ("abs", (1, 1)),
    ("sign", (1, 1)),
    ("pow", (2, 2)),
    ("log", (1, 1)),
    ("exp", (1, 1)),
    ("is_nan", (1, 1)),
    ("is_inf", (1, 1)),
    ("is_equal_approx", (2, 2)),
    ("is_zero_approx", (1, 1)),
    ("ease", (2, 2)),
    ("decimals", (1, 1)),
    ("step_decimals", (1, 1)),
    ("stepify", (2, 2)),
    ("lerp", (3, 3)),
    ("lerp_angle", (3, 3)),
    ("inverse_lerp", (3, 3)),
    ("range_lerp", (5, 5)),
    ("smoothstep", (3, 3)),
    ("move_toward", (3, 3)),
    ("dectime", (3, 3)),
    ("randomize", (0, 0)),
    ("randi", (0, 0)),
    ("randf", (0, 0)),
    ("rand_range", (2, 2)),
    ("seed", (1, 1)),
    ("rand_seed", (1, 1)),
    ("deg2rad", (1, 1)),
    ("rad2deg", (1, 1)),
    ("linear2db", (1, 1)),
    ("db2linear", (1, 1)),
    ("polar2cartesian", (2, 2)),
    ("cartesian2polar", (2, 2)),
    ("wrapi", (3, 3)),
    ("wrapf", (3, 3)),
    ("max", (2, 2)),
    ("min", (2, 2)),
    ("clamp", (3, 3)),
    ("nearest_po2", (1, 1)),
    ("weakref", (1, 1)),
    ("funcref", (2, 2)),
    ("convert", (2, 2)),
    ("typeof", (1, 1)),
    ("type_exists", (1, 1)),
    ("char", (1, 1)),
    ("ord", (1, 1)),
    ("str", (1, "INT_MAX")),
    ("print", (0, "INT_MAX")),
    ("printt", (0, "INT_MAX")),
    ("prints", (0, "INT_MAX")),
    ("printerr", (0, "INT_MAX")),
    ("printraw", (0, "INT_MAX")),
    ("print_debug", (0, "INT_MAX")),
    ("push_error", (1, 1)),
    ("push_warning", (1, 1)),
    ("var2str", (1, 1)),
    ("str2var", (1, 1)),
    ("var2bytes", (1, 1)),
    ("bytes2var", (1, 1)),
    ("range", (1, 3)),
    ("load", (1, 1)),
    ("inst2dict", (1, 1)),
    ("dict2inst", (1, 1)),
    ("validate_json", (1, 1)),
    ("parse_json", (1, 1)),
    ("to_json", (1, 1)),
    ("hash", (1, 1)),
    ("Color8", (3, 3)),
    ("ColorN", (1, 2)),
    ("print_stack", (0, 0)),
    ("get_stack", (0, 0)),
    ("instance_from_id", (1, 1)),
    ("len", (1, 1)),
    ("is_instance_valid", (1, 1)),
    ("deep_equal", (2, 2)),
    ("get_inst", (1, 1))
]

greater_than_3_1_versions = [
    0xf3f05dc,
    0x506df14,
    0xa7aad78,
    0x5565f55,
    0x6694c11,
    0xa60f242,
    0xc00427a,
    0x620ec47,
    0x7f7d97f,
    0x514a3fb,
]




class BytecodeClass:
    def __init__(self):
        self.func_names = []
        self.tk_names = []
        self.bytecode_version = 0
        self.bytecode_rev = ""
        self.bytecode_rev_num = 0
        self.engine_ver_major = 0
        self.variant_ver_major = 0
        self.version_string = ""
        self.is_dev = False
        self.removed_tokens = []
        self.added_tokens = []
        self.removed_functions = []
        self.added_functions = []
        self.arg_count_changed = []
        self.tokens_renamed = []
        self.date = ""
        self.engine_version = ""
        self.min_engine_version = ""
        self.max_engine_version = ""
        
    @property
    def file_stem(self):
        return "bytecode_" + self.bytecode_rev
    
    @property
    def class_name(self):
        return "GDScriptDecomp_" + self.bytecode_rev


def remove_comments(line:str) -> str:
    line = line.strip()
    if line.startswith("//"):
        return ""
    line = line.split("//")[0].strip()
    return line

def get_class_def(file:Path) -> BytecodeClass:
    file_stem = file.stem
    if file_stem.endswith("_base") or file_stem.endswith("_tester") or file_stem.endswith("_versions"):
        return None
    bytecode_rev:str = file_stem.split("_")[-1] # The bytecode revision
    class_name = "GDScriptDecomp_" + bytecode_rev # Capitalize the first letter and remove `_`
    
    # then, we need to get the function name array
    # it's laid out like this in each file:
    # static const char *func_names[] = {
    # 	"sin",
    # 	"cos",
    # 	"tan",
    # 	[etc...]
    # 	"len",
    # 	"is_instance_valid",
    # };
    # so we need to get the line that starts with "static const char *func_names[] = {"
    # and then get all the lines after that until we hit the line that starts with "};"
    # 
    # Then, we have to get the Token enums
    # the token enums are laid out like this in each file
    # enum Token {
    # 	TK_EOF,
    # 	TK_IDENTIFIER,
    # 	TK_CONSTANT,
    # 	TK_STRING,
    # 	[..etc]
    # 	TK_MAX
    # };
    func_names_line = 0
    tk_names_line = 0
    func_names = []
    tk_names = []
    with open(file, "r") as f:
        lines = f.readlines()
        for i in range(len(lines)):
            if lines[i].startswith("static const char *func_names[]"):
                func_names_line = i
                break
            
        for i in range(len(lines)):
            if lines[i].startswith("enum Token {"):
                tk_names_line = i
                break
        
        
        # now, we need to get the function names
        for i in range(func_names_line + 1, len(lines)):
            line = remove_comments(lines[i].strip()).strip(",")
            if line == "":
                continue
            if line.startswith("};"):
                break
            func_names.append(line.strip().strip('"'))
        
        for i in range(tk_names_line + 1, len(lines)):
            line = remove_comments(lines[i].strip()).strip(",")
            if line.startswith("};"):
                break
            # check if line is empty
            if line == "":
                continue
            #check if line begins with `//`
            if line.startswith('//'):
                continue
            tk_names.append(line.strip())
            
            
    # Now, we have to parse the header file with the same name
    # The header file is laid out like this:
    # ```cpp
    # class GDScriptDecomp_703004f : public GDScriptDecomp {
    # 	GDCLASS(GDScriptDecomp_703004f, GDScriptDecomp);
    # protected:
    # 	static void _bind_methods(){};
    #
    # 	static const int bytecode_version = 2;
    #
    # 	enum {
    # 		TOKEN_BYTE_MASK = 0x80,
    # 		TOKEN_BITS = 8,
    # 		TOKEN_MASK = (1 << TOKEN_BITS) - 1,
    # 		TOKEN_LINE_BITS = 24,
    # 		TOKEN_LINE_MASK = (1 << TOKEN_LINE_BITS) - 1,
    # 	};
    #
    # public:
    # 	virtual Error decompile_buffer(Vector<uint8_t> p_buffer) override;
    # 	virtual BYTECODE_TEST_RESULT test_bytecode(Vector<uint8_t> buffer) override { return BYTECODE_TEST_RESULT::BYTECODE_TEST_UNKNOWN; }; // not implemented
    # 	GDScriptDecomp_703004f() {
    # 		bytecode_rev = 0x703004f;
    # 		engine_ver_major = 1;
    # 		variant_ver_major = 2; // we just use variant parser/writer for v2
    # 	}
    # };
    # ```
    
    #the items we need to get are `bytecode_version`, `bytecode_rev`, `engine_ver_major`, and `variant_ver_major`
    bytecode_version = 0
    bytecode_rev_num = 0
    engine_ver_major = 0
    variant_ver_major = 0
    with open(file.parent / (file.stem + ".h"), "r") as f:
        lines = f.readlines()
        for line in lines:
            line = remove_comments(line.strip())
            if line == "":
                continue
            if line.startswith("static const int bytecode_version"):
                bytecode_version = int(line.split("=")[1].strip().strip(";"))
            elif line.startswith("bytecode_rev"):
                bytecode_rev_num = int(line.split("=")[1].strip().strip(";"), 16)
            elif line.startswith("engine_ver_major"):
                engine_ver_major = int(line.split("=")[1].strip().strip(";"))
            elif line.startswith("variant_ver_major"):
                variant_ver_major = int(line.split("=")[1].strip().strip(";"))

    bytecode_class = BytecodeClass()
    bytecode_class.func_names = func_names
    bytecode_class.tk_names = tk_names
    bytecode_class.bytecode_version = bytecode_version
    bytecode_class.bytecode_rev = bytecode_rev
    bytecode_class.bytecode_rev_num = bytecode_rev_num
    bytecode_class.engine_ver_major = engine_ver_major
    bytecode_class.variant_ver_major = variant_ver_major
    bytecode_class.added_tokens = bytecode_stuff_dict[bytecode_rev]["added_tokens"]
    bytecode_class.removed_tokens = bytecode_stuff_dict[bytecode_rev]["removed_tokens"]
    bytecode_class.added_functions = bytecode_stuff_dict[bytecode_rev]["added_functions"]
    bytecode_class.removed_functions = bytecode_stuff_dict[bytecode_rev]["removed_functions"]
    bytecode_class.arg_count_changed = bytecode_stuff_dict[bytecode_rev]["arg_count_changed"]
    bytecode_class.tokens_renamed = bytecode_stuff_dict[bytecode_rev]["tokens_renamed"]
    bytecode_class.date = bytecode_stuff_dict[bytecode_rev]["date"]
    bytecode_class.engine_version = bytecode_stuff_dict[bytecode_rev]["version"]
    bytecode_class.min_engine_version = bytecode_stuff_dict[bytecode_rev]["min_version"]
    bytecode_class.max_engine_version = bytecode_stuff_dict[bytecode_rev]["max_version"]
    bytecode_class.is_dev = bytecode_stuff_dict[bytecode_rev]["is_dev"]
    return bytecode_class

# Now, we need to generate the bytecode file
# The cpp file will contain the following:
# 1) the header declarations:
# ```cpp
# #include "core/io/marshalls.h"
# #include "core/string/print_string.h"
# 
# #include "<file_stem>.h"
# ```
# 2) the builtin function declarations, laid out like this:
# ```cpp
# static const Pair<String, Pair<int, int>> funcs[] = {
# 	{ "sin", Pair<int, int>(1, 1) },
#   { "cos", Pair<int, int>(1, 1) },
#   { "tan", Pair<int, int>(1, 1) },
#   { "sinh", Pair<int, int>(1, 1) },
# 	[etc...]
# 	{ "is_instance_valid", Pair<int, int>(1, 1) },
# };
# static constexpr int num_funcs = sizeof(funcs) / sizeof(Pair<String, Pair<int, int>>);
# ```
# We use a Pair here because we need to store the number of arguments for each function
# we get this by matching the function name with the builtin_func_arg_elements list
# with special care for the "var2bytes" and "bytes2var" functions, which have a variable number of arguments (1-2) IF the bytecode version is > 3.1
# 3) the Token enum declarations, like this:
# ```cpp
# enum Token {
# 	TK_EOF,
# 	TK_IDENTIFIER,
# 	TK_CONSTANT,
# 	[..etc]
#   TK_MAX
# };
# ```
# 4) the `get_function_name` function, which is laid out like this:
# ```cpp
# String <class_name>::get_function_name(int p_func) const {
# 	return funcs[p_func].first;
# }
# ```
# 5) the `get_function_arg_count` function, which is laid out like this:
# ```cpp
# Pair<int, int> <class_name>::get_function_arg_count(int p_func) const {
# 	return funcs[p_func].second;
# }
# ```
# 5.5) the `get_function_index` function, which is laid out like this:
# ```cpp
# int <class_name>::get_function_index(const String &p_func) const {
#   for (int i = 0; i < num_funcs; i++) {
#     if (funcs[i].first == p_func) {
#       return i;
#     }
#   return -1;
# }
# ```
# 6) the `get_global_token` function, which is laid out like this:
# ```cpp
# GDScriptDecomp::GlobalToken <class_name>::get_global_token(int p_token) const {
# 	if (p_token < 0 || p_token >= TK_MAX) {
# 		return GDScriptDecomp::GlobalToken::G_TK_MAX;
# 	}
# 	switch(Token(p_token)) {
# 		case TK_EOF: return GDScriptDecomp::GlobalToken::G_TK_EOF;
# 		case TK_IDENTIFIER: return GDScriptDecomp::GlobalToken::G_TK_IDENTIFIER;
# 		case TK_CONSTANT: return GDScriptDecomp::GlobalToken::G_TK_CONSTANT;
# 		[..etc]
# 		case TK_MAX: return GDScriptDecomp::GlobalToken::G_TK_MAX;
# 	}
# 	return GDScriptDecomp::GlobalToken::G_TK_MAX;
# }
# ```
# 7) the `get_local_token_val` function, which is laid out like this:
# ```cpp
# int <class_name>::get_local_token_val(GDScriptDecomp::GlobalToken p_token) const {
#   switch(p_token) {
#     case GDScriptDecomp::GlobalToken::G_TK_EOF: return (int) TK_EOF;
#     case GDScriptDecomp::GlobalToken::G_TK_IDENTIFIER: return (int) TK_IDENTIFIER;
#     case GDScriptDecomp::GlobalToken::G_TK_CONSTANT: return (int) TK_CONSTANT;
#     [..etc]
#     case GDScriptDecomp::GlobalToken::G_TK_MAX: return (int) TK_MAX;
#     default: return (int) TK_MAX;
#   }
#   return (int) TK_MAX;
# }
# ```
# That's it. We're done.
def generate_class_cpp(path: Path, bytecode_class:BytecodeClass) -> None:
    file_stem = bytecode_class.file_stem
    class_name = bytecode_class.class_name
    func_names = bytecode_class.func_names
    tk_names = bytecode_class.tk_names
    bytecode_version = bytecode_class.bytecode_version
    bytecode_rev = bytecode_class.bytecode_rev
    bytecode_rev_num = bytecode_class.bytecode_rev_num
    engine_ver_major = bytecode_class.engine_ver_major
    variant_ver_major = bytecode_class.variant_ver_major
    new_dir = Path( str(path.parent) + "2")
    # ensure the directory exists
    if not new_dir.exists():
        new_dir.mkdir()
    new_file_cpp = new_dir / (file_stem + ".cpp")
    with open(new_file_cpp, "w") as f:
        # 1) the header declarations:
        f.write("#include \"core/io/marshalls.h\"\n")
        f.write("#include \"core/string/print_string.h\"\n")
        f.write("\n")
        f.write("#include \"" + file.stem + ".h\"\n")
        f.write("\n")
        
        # 2) the builtin function declarations:
        f.write("static const Pair<String, Pair<int, int>> funcs[] = {\n")
        for func_name in func_names:
            if func_name == "var2bytes" or func_name == "bytes2var":
                if bytecode_rev_num in greater_than_3_1_versions:
                    f.write("\t{ \"" + func_name + "\", Pair<int, int>(1, 2) },\n")
                else:
                    f.write("\t{ \"" + func_name + "\", Pair<int, int>(1, 1) },\n")
            else:
                for builtin_func_arg_element in builtin_func_arg_elements:
                    if builtin_func_arg_element[0] == func_name:
                        f.write("\t{ \"" + func_name + "\", Pair<int, int>(" + str(builtin_func_arg_element[1][0]) + ", " + str(builtin_func_arg_element[1][1]) + ") },\n")
                        break
        f.write("};\n")
        f.write("\n")
        f.write("static constexpr int num_funcs = sizeof(funcs) / sizeof(Pair<String, Pair<int, int>>);\n")
        # 3) the Token enum declarations:
        f.write("enum Token {\n")
        for tk_name in tk_names:
            f.write("\t" + tk_name + ",\n")
        f.write("};\n")
        f.write("\n")
        
        # 3.5) the `get_token_max` function:
        f.write("int " + class_name + "::get_token_max() const {\n")
        f.write("\treturn TK_MAX;\n")
        f.write("}\n")
        
        # 4) the `get_function_name` function:
        f.write("String " + class_name + "::get_function_name(int p_func) const {\n")
        f.write("\tif (p_func < 0 || p_func >= num_funcs) {\n")
        f.write("\t\treturn \"\";\n")
        f.write("\t}\n")
        f.write("\treturn funcs[p_func].first;\n")
        f.write("}\n")
        f.write("\n")
        
        # 4.5) the `get_function_count` function:
        f.write("int " + class_name + "::get_function_count() const {\n")
        f.write("\treturn num_funcs;\n")
        f.write("}\n")
        
        # 5) the `get_function_arg_count` function:
        f.write("Pair<int, int> " + class_name + "::get_function_arg_count(int p_func) const {\n")
        f.write("\tif (p_func < 0 || p_func >= num_funcs) {\n")
        f.write("\t\treturn Pair<int, int>(-1, -1);\n")
        f.write("\t}\n")
        f.write("\treturn funcs[p_func].second;\n")
        f.write("}\n")
        f.write("\n")
        
        f.write("\n")
        # 5.5) the `get_function_index` function:
        f.write("int " + class_name + "::get_function_index(const String &p_func) const {\n")
        f.write("\tfor (int i = 0; i < num_funcs; i++) {\n")
        f.write("\t\tif (funcs[i].first == p_func) {\n")
        f.write("\t\t\treturn i;\n")
        f.write("\t\t}\n")
        f.write("\t}\n")
        f.write("\treturn -1;\n")
        f.write("}\n")
        f.write("\n")
        
        # 6) the `get_global_token` function:
        f.write("GDScriptDecomp::GlobalToken " + class_name + "::get_global_token(int p_token) const {\n")
        f.write("\tif (p_token < 0 || p_token >= TK_MAX) {\n")
        f.write("\t\treturn GDScriptDecomp::GlobalToken::G_TK_MAX;\n")
        f.write("\t}\n")
        f.write("\tswitch(Token(p_token)) {\n")
        for tk_name in tk_names:
            f.write("\t\tcase " + tk_name + ": return GDScriptDecomp::GlobalToken::G_" + tk_name + ";\n")
        f.write("\t\tdefault: return GDScriptDecomp::GlobalToken::G_TK_MAX;\n")
        f.write("\t}\n")
        f.write("\treturn GDScriptDecomp::GlobalToken::G_TK_MAX;\n")
        f.write("}\n")
        f.write("\n")
        
        # 7) the `get_local_token_val` function:
        f.write("int " + class_name + "::get_local_token_val(GDScriptDecomp::GlobalToken p_token) const {\n")
        f.write("\tswitch(p_token) {\n")
        for tk_name in tk_names:
            f.write("\t\tcase GDScriptDecomp::GlobalToken::G_" + tk_name + ": return (int) " + tk_name + ";\n")
        f.write("\t\tdefault: return -1;\n")
        f.write("\t}\n")
        f.write("\treturn -1;\n")
        f.write("}\n")
        f.write("\n")
        
        # 8) the `test_bytecode` function:
        # f.write("GDScriptDecomp::BYTECODE_TEST_RESULT " + class_name + "::test_bytecode(Vector<uint8_t> buffer) {\n")
        # f.write("\treturn GDScriptDecomp::test_bytecode(buffer);\n")
        # f.write("}\n")
        # f.write("\n")

# The class header files will look like this:
# ```cpp
# class <class_name> : public GDScriptDecomp {
# 	GDCLASS(<class_name>, GDScriptDecomp);
# protected:
# 	static void _bind_methods(){};
# 	static constexpr int bytecode_version = <bytecode_version>;
# 	static constexpr int bytecode_rev = <bytecode_rev_num>;
# 	static constexpr int engine_ver_major = <engine_ver_major>;
# 	static constexpr int variant_ver_major = <variant_ver_major>;
# 	static constexpr const char *bytecode_rev_str = "<bytecode_rev_str>";
# public:
# 	virtual String get_function_name(int p_func) const override;
#   virtual int get_token_max() const override;
# 	virtual int get_function_count() const override;
# 	virtual Pair<int, int> get_function_arg_count(int p_func) const override;
#   virtual int get_function_index(const String &p_func) const override;
# 	virtual GDScriptDecomp::GlobalToken get_global_token(int p_token) const override;
# 	virtual int get_local_token_val(GDScriptDecomp::GlobalToken p_token) const override;
# 	virtual BYTECODE_TEST_RESULT test_bytecode(Vector<uint8_t> buffer) override;
# 	virtual int get_bytecode_version() const override { return bytecode_version; }
# 	virtual int get_bytecode_rev() const override { return bytecode_rev; }
# 	virtual int get_engine_ver_major() const override { return engine_ver_major; }
# 	virtual int get_variant_ver_major() const override { return variant_ver_major; }
# 	<class_name>() {}
# };
# ```
def generate_class_header(path: Path, bytecode_class:BytecodeClass) -> None:
    file_stem = bytecode_class.file_stem
    class_name = bytecode_class.class_name
    func_names = bytecode_class.func_names
    tk_names = bytecode_class.tk_names
    bytecode_version = bytecode_class.bytecode_version
    bytecode_rev = bytecode_class.bytecode_rev
    bytecode_rev_num = bytecode_class.bytecode_rev_num
    engine_ver_major = bytecode_class.engine_ver_major
    variant_ver_major = bytecode_class.variant_ver_major
    new_dir = Path( str(path.parent) + "2")
    # ensure the directory exists
    if not new_dir.exists():
        new_dir.mkdir()
    new_file_h = new_dir / (file_stem + ".h")
    with open(new_file_h, "w") as f:
        f.write("#pragma once\n")
        f.write("\n")
        f.write("#include \"bytecode_base.h\"\n")
        f.write("\n")
        f.write("class " + class_name + " : public GDScriptDecomp {\n")
        f.write("\tGDCLASS(" + class_name + ", GDScriptDecomp);\n")
        f.write("protected:\n")
        f.write("\tstatic void _bind_methods(){};\n")
        f.write("\tstatic constexpr int bytecode_version = " + str(bytecode_version) + ";\n")
        f.write("\tstatic constexpr int bytecode_rev = 0x" + bytecode_rev + ";\n")
        f.write("\tstatic constexpr int engine_ver_major = " + str(engine_ver_major) + ";\n")
        f.write("\tstatic constexpr int variant_ver_major = " + str(variant_ver_major) + ";\n")
        f.write("\tstatic constexpr const char *bytecode_rev_str = \"" + bytecode_rev + "\";\n")
        f.write("public:\n")
        f.write("\tvirtual String get_function_name(int p_func) const override;\n")
        f.write("\tvirtual int get_function_count() const override;\n")
        f.write("\tvirtual Pair<int, int> get_function_arg_count(int p_func) const override;\n")
        f.write("\tvirtual int get_token_max() const override;\n")
        f.write("\tvirtual int get_function_index(const String &p_func) const override;\n")
        f.write("\tvirtual GDScriptDecomp::GlobalToken get_global_token(int p_token) const override;\n")
        f.write("\tvirtual int get_local_token_val(GDScriptDecomp::GlobalToken p_token) const override;\n")
        # f.write("\tvirtual BYTECODE_TEST_RESULT test_bytecode(Vector<uint8_t> buffer) override;\n")
        f.write("\tvirtual int get_bytecode_version() const override { return bytecode_version; }\n")
        f.write("\tvirtual int get_bytecode_rev() const override { return bytecode_rev; }\n")
        f.write("\tvirtual int get_engine_ver_major() const override { return engine_ver_major; }\n")
        f.write("\tvirtual int get_variant_ver_major() const override { return variant_ver_major; }\n")
        f.write("\t" + class_name + "() {}\n")
        f.write("};\n")
        f.write("\n")
        
def write_bytecode_json(bytecode_classes:list[BytecodeClass]) -> None:
    bytecode_json = []
    for bytecode_class in bytecode_classes:
        bytecode_json.append({
            "bytecode_rev": bytecode_class.bytecode_rev,
            "bytecode_version": bytecode_class.bytecode_version,
            "date" : bytecode_class.date,
            "engine_version": bytecode_class.engine_version,
            "min_engine_version": bytecode_class.min_engine_version,
            "max_engine_version": bytecode_class.max_engine_version,
            "engine_ver_major": bytecode_class.engine_ver_major,
            "variant_ver_major": bytecode_class.variant_ver_major,
            "is_dev": bytecode_class.is_dev,
            "added_tokens": bytecode_class.added_tokens,
            "removed_tokens": bytecode_class.removed_tokens,
            "added_functions": bytecode_class.added_functions,
            "removed_functions": bytecode_class.removed_functions,
            "arg_count_changed": bytecode_class.arg_count_changed,
            "tokens_renamed": bytecode_class.tokens_renamed,
            "func_names": bytecode_class.func_names,
            "tk_names": bytecode_class.tk_names,
        })
    with open("bytecode_versions.json", "w") as f:
        f.write(json.dumps(bytecode_json, indent=4))
        
def read_bytecode_json(path: Path) -> list[BytecodeClass]:
    bytecode_json = []
    with open(path, "r") as f:
        bytecode_json = json.loads(f.read())
    bytecode_classes = []
    for bytecode in bytecode_json:
        bytecode_class = BytecodeClass()
        bytecode_class.bytecode_rev = bytecode["bytecode_rev"]
        bytecode_class.bytecode_version = bytecode["bytecode_version"]
        bytecode_class.date = bytecode["date"]
        bytecode_class.engine_version = bytecode["engine_version"]
        bytecode_class.min_engine_version = bytecode["min_engine_version"]
        bytecode_class.max_engine_version = bytecode["max_engine_version"]
        bytecode_class.engine_ver_major = bytecode["engine_ver_major"]
        bytecode_class.variant_ver_major = bytecode["variant_ver_major"]
        bytecode_class.func_names = bytecode["func_names"]
        bytecode_class.tk_names = bytecode["tk_names"]
        bytecode_class.added_tokens = bytecode["added_tokens"]
        bytecode_class.removed_tokens = bytecode["removed_tokens"]
        bytecode_class.added_functions = bytecode["added_functions"]
        bytecode_class.removed_functions = bytecode["removed_functions"]
        bytecode_class.arg_count_changed = bytecode["arg_count_changed"]
        bytecode_class.tokens_renamed = bytecode["tokens_renamed"]
        bytecode_class.is_dev = bytecode["is_dev"]
        bytecode_classes.append(bytecode_class)
    return bytecode_classes

bytecode_classes = []

# First, we need to get the bytecode directory
bytecode_dir = Path(os.path.dirname(os.path.realpath(__file__))) / "bytecode"

# Next, we need to get all the .cpp files in the bytecode directory
bytecode_files = [file for file in bytecode_dir.iterdir() if file.suffix == ".cpp"]

bytecode_stuff_dict_copy = bytecode_stuff_dict.copy()
for file in bytecode_files:
    bytecode_class = get_class_def(file)
    if bytecode_class is None:
        continue
    bytecode_classes.append(bytecode_class)
    bytecode_stuff_dict_copy[bytecode_class.bytecode_rev] = bytecode_class
    generate_class_cpp(file, bytecode_class)
    generate_class_header(file, bytecode_class)
#get the values from bytecode_stuff_dict_copy in the same order they're in the dictionary
bytecode_classes = [bytecode_stuff_dict_copy[key] for key in bytecode_stuff_dict_copy]
write_bytecode_json(bytecode_classes)

    
    

        
            
        
    
