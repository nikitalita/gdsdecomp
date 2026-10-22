/*
 * Copyright (c) 2016, 2017, Yutaka Tsutano
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "jitana/util/axml_parser.hpp"
#include "jitana/util/stream_reader.hpp"

using namespace jitana;

namespace {
    // Converts a UTF-16LE buffer of `len` code units to a UTF-8 std::string.
    // Handles surrogate pairs; lone surrogates are emitted as U+FFFD-equivalent
    // raw bytes following the same shape as the original byte stream so that
    // malformed input is still round-tripped without throwing.
    std::string utf16le_to_utf8(const uint16_t* data, std::size_t len)
    {
        std::string out;
        out.reserve(len);
        for (std::size_t i = 0; i < len; ++i) {
            std::uint32_t cp = data[i];
            if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < len) {
                std::uint32_t lo = data[i + 1];
                if (lo >= 0xDC00 && lo <= 0xDFFF) {
                    cp = 0x10000 + (((cp - 0xD800) << 10) | (lo - 0xDC00));
                    ++i;
                }
            }
            if (cp < 0x80) {
                out.push_back(static_cast<char>(cp));
            }
            else if (cp < 0x800) {
                out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
            else if (cp < 0x10000) {
                out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
            else {
                out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
        }
        return out;
    }
}

namespace {
    class axml_parser_impl {
    private:
        [[maybe_unused]]
        static constexpr uint16_t res_null_type = 0x0001;
        static constexpr uint16_t res_string_pool_type = 0x0001;
        [[maybe_unused]]
        static constexpr uint16_t res_table_type = 0x0002;
        static constexpr uint16_t res_xml_type = 0x0003;
        [[maybe_unused]]
        static constexpr uint16_t res_xml_first_chunk_type = 0x0100;
        static constexpr uint16_t res_xml_start_namespace_type = 0x0100;
        static constexpr uint16_t res_xml_end_namespace_type = 0x0101;
        static constexpr uint16_t res_xml_start_element_type = 0x0102;
        static constexpr uint16_t res_xml_end_element_type = 0x0103;
        static constexpr uint16_t res_xml_cdata_type = 0x0104;
        [[maybe_unused]]
        static constexpr uint16_t res_xml_last_chunk_type = 0x017f;
        static constexpr uint16_t res_xml_resource_map_type = 0x0180;

        [[maybe_unused]]
        static constexpr uint16_t res_table_package_type = 0x0200;
        [[maybe_unused]]
        static constexpr uint16_t res_table_type_type = 0x0201;
        [[maybe_unused]]
        static constexpr uint16_t res_table_type_spec_type = 0x0202;
        [[maybe_unused]]
        static constexpr uint16_t res_table_library_type = 0x0203;

        std::vector<uint32_t> attr_names_res_ids_;

    public:
        enum complex_unit {
            complex_unit_px = 0,
            complex_unit_dip = 1,
            complex_unit_sp = 2,
            complex_unit_pt = 3,
            complex_unit_in = 4,
            complex_unit_mm = 5,
            complex_unit_fraction = 0,
            complex_unit_fraction_parent = 1
        };

        struct res_chunk_header {
            uint16_t type;
            uint16_t header_size;
            uint32_t size;
        };

        struct resource_value {
            uint16_t size;
            uint8_t res0;
            uint8_t data_type;
            uint32_t data;

            enum {
                // The 'data' is either 0 or 1, specifying this resource is
                // either undefined or empty, respectively.
                type_null = 0x00,
                // The 'data' holds a ResTable_ref, a reference to another
                // resource table entry.
                type_reference = 0x01,
                // The 'data' holds an attribute resource identifier.
                type_attribute = 0x02,
                // The 'data' holds an index into the containing resource
                // table's global value string pool.
                type_string = 0x03,
                // The 'data' holds a single-precision floating point number.
                type_float = 0x04,
                // The 'data' holds a complex number encoding a dimension value,
                // such as "100in".
                type_dimension = 0x05,
                // The 'data' holds a complex number encoding a fraction of a
                // container.
                type_fraction = 0x06,
                // The 'data' holds a dynamic ResTable_ref, which needs to be
                // resolved before it can be used like a type_reference.
                type_dynamic_reference = 0x07,

                // Beginning of integer flavors...
                type_first_int = 0x10,

                // The 'data' is a raw integer value of the form n..n.
                type_int_dec = 0x10,
                // The 'data' is a raw integer value of the form 0xn..n.
                type_int_hex = 0x11,
                // The 'data' is either 0 or 1, for input "false" or "true"
                // respectively.
                type_int_boolean = 0x12,

                // Beginning of color integer flavors...
                type_first_color_int = 0x1c,

                // The 'data' is a raw integer value of the form #aarrggbb.
                type_int_color_argb8 = 0x1c,
                // The 'data' is a raw integer value of the form #rrggbb.
                type_int_color_rgb8 = 0x1d,
                // The 'data' is a raw integer value of the form #argb.
                type_int_color_argb4 = 0x1e,
                // The 'data' is a raw integer value of the form #rgb.
                type_int_color_rgb4 = 0x1f,

                // ...end of integer flavors.
                type_last_color_int = 0x1f,

                // ...end of integer flavors.
                type_last_int = 0x1f
            };

            friend inline std::ostream& operator<<(std::ostream& os,
                                                   const resource_value& x)
            {
                auto print_complex = [&](bool frac) {
                    constexpr float mantissa_mult = 1.0f / (1 << 8);
                    constexpr float radix_mults[]
                            = {mantissa_mult * 1.0f,
                               mantissa_mult * 1.0f / (1 << 7),
                               mantissa_mult * 1.0f / (1 << 15),
                               mantissa_mult * 1.0f / (1 << 23)};
                    float value = static_cast<int32_t>(x.data & 0xffffff00)
                            * radix_mults[(x.data >> 4) & 0x3];

                    if (frac) {
                        os << value * 100;
                        switch (x.data & 0xf) {
                        case complex_unit_fraction:
                            os << "%";
                            break;
                        case complex_unit_fraction_parent:
                            os << "%p";
                            break;
                        }
                    }
                    else {
                        os << value;
                        switch (x.data & 0xf) {
                        case complex_unit_px:
                            os << "px";
                            break;
                        case complex_unit_dip:
                            os << "dip";
                            break;
                        case complex_unit_sp:
                            os << "sp";
                            break;
                        case complex_unit_pt:
                            os << "pt";
                            break;
                        case complex_unit_in:
                            os << "in";
                            break;
                        case complex_unit_mm:
                            os << "mm";
                            break;
                        }
                    }
                };

                switch (x.data_type) {
                case type_null:
                    os << "null";
                    break;
                // case type_reference:
                //     break;
                // case type_attribute:
                //     break;
                // case type_string:
                //     break;
                case type_float:
                    float f;
                    std::memcpy(&f, &x.data, sizeof(f));
                    os << f;
                    break;
                case type_dimension:
                    print_complex(false);
                    break;
                case type_fraction:
                    print_complex(true);
                    break;
                // case type_dynamic_reference:
                //     break;
                // case type_first_int:
                //     break;
                case type_int_dec:
                    os << static_cast<int32_t>(x.data);
                    break;
                case type_int_hex:
                    os << "0x" << std::hex << x.data;
                    break;
                case type_int_boolean:
                    os << (x.data ? "true" : "false");
                    break;
                // case type_first_color_int:
                //     os << x.data;
                //     break;
                // case type_int_color_argb8:
                //     break;
                // case type_int_color_rgb8:
                //     break;
                // case type_int_color_argb4:
                //     break;
                // case type_int_color_rgb4:
                //     break;
                // case type_last_color_int:
                //     break;
                // case type_last_int:
                //     break;
                default:
                    os << "@ref/0x" << std::setw(8) << std::setfill('0') << std::hex << x.data;
                }

                return os;
            }
        };

    public:
        axml_parser_impl(jitana::stream_reader& reader,
                         jitana::xml_node& root) noexcept
                : reader_(reader), root_(root)
        {
        }

        /// Returns the parser's terminal error, if any. Synthesizes a
        /// `parse_error` from a sticky reader overflow when no other error has
        /// been recorded.
        std::optional<jitana::axml_parser::error> take_error()
        {
            if (error_) {
                return std::move(error_);
            }
            if (reader_.has_error()) {
                return jitana::axml_parser::error{
                        jitana::axml_parser::error_code::parse_error,
                        "unexpected end of stream"};
            }
            return std::nullopt;
        }

        void parse()
        {
            xml_stack_.clear();
            xml_stack_.emplace_back(&root_);

            // Make sure that the file is large enough.
            if (reader_.size() < sizeof(res_chunk_header)) {
                fail(jitana::axml_parser::error_code::not_an_axml_file,
                     "not a binary XML file");
                return;
            }

            const auto& header = reader_.get<res_chunk_header>();
            if (failed()) {
                return;
            }

            // Make sure it's the right file type.
            if (header.type != res_xml_type) {
                fail(jitana::axml_parser::error_code::not_an_axml_file,
                     "not a binary XML file");
                return;
            }

            // Apply pull parsing.
            while (reader_.head() < static_cast<size_t>(header.size)) {
                if (failed()) {
                    return;
                }

                auto saved_reader = reader_;

                const auto& header = reader_.peek<res_chunk_header>();
                if (failed()) {
                    return;
                }

                switch (header.type) {
                case res_string_pool_type:
                    parse_string_pool();
                    break;
                case res_xml_resource_map_type:
                    parse_resource_map();
                    break;
                case res_xml_start_namespace_type:
                    parse_start_namespace();
                    break;
                case res_xml_end_namespace_type:
                    parse_end_namespace();
                    break;
                case res_xml_start_element_type:
                    parse_xml_start_element();
                    break;
                case res_xml_end_element_type:
                    parse_xml_end_element();
                    break;
                case res_xml_cdata_type:
                    parse_xml_cdata();
                    break;
                default: {
                    std::stringstream ss;
                    ss << "unknown chunk type 0x" << std::hex << header.type;
                    fail(jitana::axml_parser::error_code::parse_error, ss.str());
                    return;
                }
                }

                if (failed()) {
                    return;
                }

                reader_ = saved_reader;
                reader_.move_head_forward(header.size);
            }
        }

    private:
        void parse_string_pool()
        {
            if (failed()) return;

            /*const auto& header =*/reader_.get<res_chunk_header>();

            auto string_count = reader_.get<uint32_t>();
            auto style_count = reader_.get<uint32_t>();
            auto flags = reader_.get<uint32_t>();
            // bool sorted_flag = flags & (1 << 0);
            bool utf8_flag = flags & (1 << 8);
            auto strings_start = reader_.get<uint32_t>();
            /*auto styles_start =*/reader_.get<uint32_t>();

            if (failed()) return;

            if (style_count != 0) {
                fail(jitana::axml_parser::error_code::parse_error,
                     "styles are not supported");
                return;
            }

            // Get the string offsets.
            std::vector<uint32_t> string_offsets(string_count);
            for (auto& off : string_offsets) {
                off = reader_.get<uint32_t>();
            }

            // Fill the string table.
            strings_.clear();
            strings_.reserve(string_count);
            for (auto off : string_offsets) {
                reader_.move_head(strings_start + 8 + off);

                strings_.emplace_back();
                auto& str = strings_.back();

                if (utf8_flag) {
                    reader_.get<uint8_t>();

                    // Compute the string length.
                    size_t len = reader_.get<uint8_t>();
                    if (len & 0x80) {
                        /*len |= ((len & 0x7f) << 8) |*/ reader_.get<uint8_t>();
                    }

                    // Fill characters.
                    if (len != 0) {
                        str = reader_.get_c_str();
                    }
                }
                else {
                    // Compute the string length.
                    size_t len = reader_.get<uint16_t>();
                    if (len & 0x8000) {
                        len |= ((len & 0x7fff) << 16) | reader_.get<uint16_t>();
                    }

                    // Convert to UTF-8.
                    auto* ptr = &reader_.peek<uint16_t>();
                    str = utf16le_to_utf8(ptr, len);
                }
            }
        }

        void parse_resource_map()
        {
            if (failed()) return;

            const auto& header = reader_.get<res_chunk_header>();
            if (failed()) return;

            attr_names_res_ids_.clear();
            for (size_t i = 0; i < (header.size - sizeof(header)) / 4; ++i) {
                auto id = reader_.get<uint32_t>();
                attr_names_res_ids_.push_back(id);
            }
        }

        void parse_start_namespace()
        {
            if (failed()) return;

            /*const auto& header =*/reader_.get<res_chunk_header>();

            /*auto line_num =*/reader_.get<uint32_t>();
            /*auto comment =*/reader_.get<uint32_t>();
            auto prefix = reader_.get<uint32_t>();
            auto uri = reader_.get<uint32_t>();

            xml_stack_.back().namespaces.emplace_back(uri, prefix);
        }

        void parse_end_namespace()
        {
            if (failed()) return;

            /*const auto& header =*/reader_.get<res_chunk_header>();

            /*auto line_num =*/reader_.get<uint32_t>();
            /*auto comment =*/reader_.get<uint32_t>();
            /*auto prefix =*/reader_.get<uint32_t>();
            /*auto uri =*/reader_.get<uint32_t>();

            if (xml_stack_.back().namespaces.empty()) {
                fail(jitana::axml_parser::error_code::parse_error,
                     "namespace stack underflow");
                return;
            }
            xml_stack_.back().namespaces.pop_back();
        }

        void parse_xml_start_element()
        {
            if (failed()) return;

            /*const auto& header =*/reader_.get<res_chunk_header>();

            /*auto line_num =*/reader_.get<uint32_t>();
            /*auto comment =*/reader_.get<uint32_t>();
            /*auto ns =*/reader_.get<uint32_t>();
            auto name = reader_.get<uint32_t>();
            /*auto attribute_size =*/reader_.get<uint32_t>();
            auto attribute_count = reader_.get<uint16_t>();
            /*auto id_index =*/reader_.get<uint16_t>();
            /*auto class_index =*/reader_.get<uint16_t>();
            /*auto style_index =*/reader_.get<uint16_t>();

            if (failed() || name >= strings_.size()) {
                fail(jitana::axml_parser::error_code::parse_error,
                     "invalid element name index");
                return;
            }

            // Create the new element node.
            auto& parent = *xml_stack_.back().node;
            parent.children.emplace_back();
            auto& elem = parent.children.back();
            elem.name = strings_[name];

            for (const auto& ns : xml_stack_.back().namespaces) {
                elem.attributes.emplace_back("xmlns:" + strings_[ns.second],
                                             strings_[ns.first]);
            }
            xml_stack_.emplace_back(&elem);

            for (int i = 0; i < attribute_count; ++i) {
                auto attr_ns = reader_.get<uint32_t>();
                auto attr_name = reader_.get<uint32_t>();
                auto attr_raw_val = reader_.get<uint32_t>();
                auto value = reader_.get<resource_value>();

                if (failed()) return;

                std::string attr_qname;
                if (attr_ns != 0xffffffff) {
                    auto prefix = lookup_prefix(attr_ns);
                    if (prefix != 0xffffffff) {
                        attr_qname += strings_[prefix];
                        attr_qname += ":";
                    }
                }
                if (attr_name >= strings_.size()) {
                    fail(jitana::axml_parser::error_code::parse_error,
                         "invalid attribute name index");
                    return;
                }
                if (strings_[attr_name].empty()) {
                    if (attr_name >= attr_names_res_ids_.size()) {
                        fail(jitana::axml_parser::error_code::parse_error,
                             "undefined attr name");
                        return;
                    }
                    attr_qname += get_resource_string(
                            attr_names_res_ids_[attr_name]);
                }
                else {
                    attr_qname += strings_[attr_name];
                }

                std::stringstream ss;
                if (attr_raw_val != 0xffffffff) {
                    if (attr_raw_val >= strings_.size()) {
                        fail(jitana::axml_parser::error_code::parse_error,
                             "invalid attribute value index");
                        return;
                    }
                    ss << strings_[attr_raw_val];
                }
                else {
                    // TODO: print in human readable format.
                    ss << value;
                }

                elem.attributes.emplace_back(std::move(attr_qname), ss.str());
            }
        }

        void parse_xml_end_element()
        {
            if (failed()) return;

            /*const auto& header =*/reader_.get<res_chunk_header>();

            /*auto line_num =*/reader_.get<uint32_t>();
            /*auto comment =*/reader_.get<uint32_t>();
            /*auto ns =*/reader_.get<uint32_t>();
            /*auto name =*/reader_.get<uint32_t>();

            if (xml_stack_.size() <= 1) {
                fail(jitana::axml_parser::error_code::parse_error,
                     "element stack underflow");
                return;
            }
            xml_stack_.pop_back();
        }

        void parse_xml_cdata()
        {
            if (failed()) return;

            /*const auto& header =*/reader_.get<res_chunk_header>();

            /*auto line_num =*/reader_.get<uint32_t>();
            /*auto comment =*/reader_.get<uint32_t>();
            auto text = reader_.get<uint32_t>();
            reader_.get<uint32_t>();
            reader_.get<uint32_t>();

            if (failed() || text >= strings_.size()) {
                fail(jitana::axml_parser::error_code::parse_error,
                     "invalid cdata index");
                return;
            }

            auto& parent = *xml_stack_.back().node;
            parent.children.emplace_back();
            auto& tn = parent.children.back();
            tn.name = "<xmltext>";
            tn.text = strings_[text];
        }

        uint32_t lookup_prefix(uint32_t uri)
        {
            auto stack_it = xml_stack_.rbegin();
            ++stack_it;
            for (; stack_it != xml_stack_.rend(); ++stack_it) {
                const auto& v = stack_it->namespaces;
                auto it = std::find_if(
                        v.rbegin(), v.rend(),
                        [&](const std::pair<uint32_t, uint32_t>& x) {
                            return x.first == uri;
                        });
                if (it != v.rend()) {
                    return it->second;
                }
            }

            return 0xffffffff;
        }

        std::string get_resource_string(uint16_t id)
        {
            static const char* attr_names[]
                    = {"theme",
                       "label",
                       "icon",
                       "name",
                       "manageSpaceActivity",
                       "allowClearUserData",
                       "permission",
                       "readPermission",
                       "writePermission",
                       "protectionLevel",
                       "permissionGroup",
                       "sharedUserId",
                       "hasCode",
                       "persistent",
                       "enabled",
                       "debuggable",
                       "exported",
                       "process",
                       "taskAffinity",
                       "multiprocess",
                       "finishOnTaskLaunch",
                       "clearTaskOnLaunch",
                       "stateNotNeeded",
                       "excludeFromRecents",
                       "authorities",
                       "syncable",
                       "initOrder",
                       "grantUriPermissions",
                       "priority",
                       "launchMode",
                       "screenOrientation",
                       "configChanges",
                       "description",
                       "targetPackage",
                       "handleProfiling",
                       "functionalTest",
                       "value",
                       "resource",
                       "mimeType",
                       "scheme",
                       "host",
                       "port",
                       "path",
                       "pathPrefix",
                       "pathPattern",
                       "action",
                       "data",
                       "targetClass",
                       "colorForeground",
                       "colorBackground",
                       "backgroundDimAmount",
                       "disabledAlpha",
                       "textAppearance",
                       "textAppearanceInverse",
                       "textColorPrimary",
                       "textColorPrimaryDisableOnly",
                       "textColorSecondary",
                       "textColorPrimaryInverse",
                       "textColorSecondaryInverse",
                       "textColorPrimaryNoDisable",
                       "textColorSecondaryNoDisable",
                       "textColorPrimaryInverseNoDisable",
                       "textColorSecondaryInverseNoDisable",
                       "textColorHintInverse",
                       "textAppearanceLarge",
                       "textAppearanceMedium",
                       "textAppearanceSmall",
                       "textAppearanceLargeInverse",
                       "textAppearanceMediumInverse",
                       "textAppearanceSmallInverse",
                       "textCheckMark",
                       "textCheckMarkInverse",
                       "buttonStyle",
                       "buttonStyleSmall",
                       "buttonStyleInset",
                       "buttonStyleToggle",
                       "galleryItemBackground",
                       "listPreferredItemHeight",
                       "expandableListPreferredItemPaddingLeft",
                       "expandableListPreferredChildPaddingLeft",
                       "expandableListPreferredItemIndicatorLeft",
                       "expandableListPreferredItemIndicatorRight",
                       "expandableListPreferredChildIndicatorLeft",
                       "expandableListPreferredChildIndicatorRight",
                       "windowBackground",
                       "windowFrame",
                       "windowNoTitle",
                       "windowIsFloating",
                       "windowIsTranslucent",
                       "windowContentOverlay",
                       "windowTitleSize",
                       "windowTitleStyle",
                       "windowTitleBackgroundStyle",
                       "alertDialogStyle",
                       "panelBackground",
                       "panelFullBackground",
                       "panelColorForeground",
                       "panelColorBackground",
                       "panelTextAppearance",
                       "scrollbarSize",
                       "scrollbarThumbHorizontal",
                       "scrollbarThumbVertical",
                       "scrollbarTrackHorizontal",
                       "scrollbarTrackVertical",
                       "scrollbarAlwaysDrawHorizontalTrack",
                       "scrollbarAlwaysDrawVerticalTrack",
                       "absListViewStyle",
                       "autoCompleteTextViewStyle",
                       "checkboxStyle",
                       "dropDownListViewStyle",
                       "editTextStyle",
                       "expandableListViewStyle",
                       "galleryStyle",
                       "gridViewStyle",
                       "imageButtonStyle",
                       "imageWellStyle",
                       "listViewStyle",
                       "listViewWhiteStyle",
                       "popupWindowStyle",
                       "progressBarStyle",
                       "progressBarStyleHorizontal",
                       "progressBarStyleSmall",
                       "progressBarStyleLarge",
                       "seekBarStyle",
                       "ratingBarStyle",
                       "ratingBarStyleSmall",
                       "radioButtonStyle",
                       "scrollbarStyle",
                       "scrollViewStyle",
                       "spinnerStyle",
                       "starStyle",
                       "tabWidgetStyle",
                       "textViewStyle",
                       "webViewStyle",
                       "dropDownItemStyle",
                       "spinnerDropDownItemStyle",
                       "dropDownHintAppearance",
                       "spinnerItemStyle",
                       "mapViewStyle",
                       "preferenceScreenStyle",
                       "preferenceCategoryStyle",
                       "preferenceInformationStyle",
                       "preferenceStyle",
                       "checkBoxPreferenceStyle",
                       "yesNoPreferenceStyle",
                       "dialogPreferenceStyle",
                       "editTextPreferenceStyle",
                       "ringtonePreferenceStyle",
                       "preferenceLayoutChild",
                       "textSize",
                       "typeface",
                       "textStyle",
                       "textColor",
                       "textColorHighlight",
                       "textColorHint",
                       "textColorLink",
                       "state_focused",
                       "state_window_focused",
                       "state_enabled",
                       "state_checkable",
                       "state_checked",
                       "state_selected",
                       "state_active",
                       "state_single",
                       "state_first",
                       "state_middle",
                       "state_last",
                       "state_pressed",
                       "state_expanded",
                       "state_empty",
                       "state_above_anchor",
                       "ellipsize",
                       "x",
                       "y",
                       "windowAnimationStyle",
                       "gravity",
                       "autoLink",
                       "linksClickable",
                       "entries",
                       "layout_gravity",
                       "windowEnterAnimation",
                       "windowExitAnimation",
                       "windowShowAnimation",
                       "windowHideAnimation",
                       "activityOpenEnterAnimation",
                       "activityOpenExitAnimation",
                       "activityCloseEnterAnimation",
                       "activityCloseExitAnimation",
                       "taskOpenEnterAnimation",
                       "taskOpenExitAnimation",
                       "taskCloseEnterAnimation",
                       "taskCloseExitAnimation",
                       "taskToFrontEnterAnimation",
                       "taskToFrontExitAnimation",
                       "taskToBackEnterAnimation",
                       "taskToBackExitAnimation",
                       "orientation",
                       "keycode",
                       "fullDark",
                       "topDark",
                       "centerDark",
                       "bottomDark",
                       "fullBright",
                       "topBright",
                       "centerBright",
                       "bottomBright",
                       "bottomMedium",
                       "centerMedium",
                       "id",
                       "tag",
                       "scrollX",
                       "scrollY",
                       "background",
                       "padding",
                       "paddingLeft",
                       "paddingTop",
                       "paddingRight",
                       "paddingBottom",
                       "focusable",
                       "focusableInTouchMode",
                       "visibility",
                       "fitsSystemWindows",
                       "scrollbars",
                       "fadingEdge",
                       "fadingEdgeLength",
                       "nextFocusLeft",
                       "nextFocusRight",
                       "nextFocusUp",
                       "nextFocusDown",
                       "clickable",
                       "longClickable",
                       "saveEnabled",
                       "drawingCacheQuality",
                       "duplicateParentState",
                       "clipChildren",
                       "clipToPadding",
                       "layoutAnimation",
                       "animationCache",
                       "persistentDrawingCache",
                       "alwaysDrawnWithCache",
                       "addStatesFromChildren",
                       "descendantFocusability",
                       "layout",
                       "inflatedId",
                       "layout_width",
                       "layout_height",
                       "layout_margin",
                       "layout_marginLeft",
                       "layout_marginTop",
                       "layout_marginRight",
                       "layout_marginBottom",
                       "listSelector",
                       "drawSelectorOnTop",
                       "stackFromBottom",
                       "scrollingCache",
                       "textFilterEnabled",
                       "transcriptMode",
                       "cacheColorHint",
                       "dial",
                       "hand_hour",
                       "hand_minute",
                       "format",
                       "checked",
                       "button",
                       "checkMark",
                       "foreground",
                       "measureAllChildren",
                       "groupIndicator",
                       "childIndicator",
                       "indicatorLeft",
                       "indicatorRight",
                       "childIndicatorLeft",
                       "childIndicatorRight",
                       "childDivider",
                       "animationDuration",
                       "spacing",
                       "horizontalSpacing",
                       "verticalSpacing",
                       "stretchMode",
                       "columnWidth",
                       "numColumns",
                       "src",
                       "antialias",
                       "filter",
                       "dither",
                       "scaleType",
                       "adjustViewBounds",
                       "maxWidth",
                       "maxHeight",
                       "tint",
                       "baselineAlignBottom",
                       "cropToPadding",
                       "textOn",
                       "textOff",
                       "baselineAligned",
                       "baselineAlignedChildIndex",
                       "weightSum",
                       "divider",
                       "dividerHeight",
                       "choiceMode",
                       "itemTextAppearance",
                       "horizontalDivider",
                       "verticalDivider",
                       "headerBackground",
                       "itemBackground",
                       "itemIconDisabledAlpha",
                       "rowHeight",
                       "maxRows",
                       "maxItemsPerRow",
                       "moreIcon",
                       "max",
                       "progress",
                       "secondaryProgress",
                       "indeterminate",
                       "indeterminateOnly",
                       "indeterminateDrawable",
                       "progressDrawable",
                       "indeterminateDuration",
                       "indeterminateBehavior",
                       "minWidth",
                       "minHeight",
                       "interpolator",
                       "thumb",
                       "thumbOffset",
                       "numStars",
                       "rating",
                       "stepSize",
                       "isIndicator",
                       "checkedButton",
                       "stretchColumns",
                       "shrinkColumns",
                       "collapseColumns",
                       "layout_column",
                       "layout_span",
                       "bufferType",
                       "text",
                       "hint",
                       "textScaleX",
                       "cursorVisible",
                       "maxLines",
                       "lines",
                       "height",
                       "minLines",
                       "maxEms",
                       "ems",
                       "width",
                       "minEms",
                       "scrollHorizontally",
                       "password",
                       "singleLine",
                       "selectAllOnFocus",
                       "includeFontPadding",
                       "maxLength",
                       "shadowColor",
                       "shadowDx",
                       "shadowDy",
                       "shadowRadius",
                       "numeric",
                       "digits",
                       "phoneNumber",
                       "inputMethod",
                       "capitalize",
                       "autoText",
                       "editable",
                       "freezesText",
                       "drawableTop",
                       "drawableBottom",
                       "drawableLeft",
                       "drawableRight",
                       "drawablePadding",
                       "completionHint",
                       "completionHintView",
                       "completionThreshold",
                       "dropDownSelector",
                       "popupBackground",
                       "inAnimation",
                       "outAnimation",
                       "flipInterval",
                       "fillViewport",
                       "prompt",
                       "startYear",
                       "endYear",
                       "mode",
                       "layout_x",
                       "layout_y",
                       "layout_weight",
                       "layout_toLeftOf",
                       "layout_toRightOf",
                       "layout_above",
                       "layout_below",
                       "layout_alignBaseline",
                       "layout_alignLeft",
                       "layout_alignTop",
                       "layout_alignRight",
                       "layout_alignBottom",
                       "layout_alignParentLeft",
                       "layout_alignParentTop",
                       "layout_alignParentRight",
                       "layout_alignParentBottom",
                       "layout_centerInParent",
                       "layout_centerHorizontal",
                       "layout_centerVertical",
                       "layout_alignWithParentIfMissing",
                       "layout_scale",
                       "visible",
                       "variablePadding",
                       "constantSize",
                       "oneshot",
                       "duration",
                       "drawable",
                       "shape",
                       "innerRadiusRatio",
                       "thicknessRatio",
                       "startColor",
                       "endColor",
                       "useLevel",
                       "angle",
                       "type",
                       "centerX",
                       "centerY",
                       "gradientRadius",
                       "color",
                       "dashWidth",
                       "dashGap",
                       "radius",
                       "topLeftRadius",
                       "topRightRadius",
                       "bottomLeftRadius",
                       "bottomRightRadius",
                       "left",
                       "top",
                       "right",
                       "bottom",
                       "minLevel",
                       "maxLevel",
                       "fromDegrees",
                       "toDegrees",
                       "pivotX",
                       "pivotY",
                       "insetLeft",
                       "insetRight",
                       "insetTop",
                       "insetBottom",
                       "shareInterpolator",
                       "fillBefore",
                       "fillAfter",
                       "startOffset",
                       "repeatCount",
                       "repeatMode",
                       "zAdjustment",
                       "fromXScale",
                       "toXScale",
                       "fromYScale",
                       "toYScale",
                       "fromXDelta",
                       "toXDelta",
                       "fromYDelta",
                       "toYDelta",
                       "fromAlpha",
                       "toAlpha",
                       "delay",
                       "animation",
                       "animationOrder",
                       "columnDelay",
                       "rowDelay",
                       "direction",
                       "directionPriority",
                       "factor",
                       "cycles",
                       "searchMode",
                       "searchSuggestAuthority",
                       "searchSuggestPath",
                       "searchSuggestSelection",
                       "searchSuggestIntentAction",
                       "searchSuggestIntentData",
                       "queryActionMsg",
                       "suggestActionMsg",
                       "suggestActionMsgColumn",
                       "menuCategory",
                       "orderInCategory",
                       "checkableBehavior",
                       "title",
                       "titleCondensed",
                       "alphabeticShortcut",
                       "numericShortcut",
                       "checkable",
                       "selectable",
                       "orderingFromXml",
                       "key",
                       "summary",
                       "order",
                       "widgetLayout",
                       "dependency",
                       "defaultValue",
                       "shouldDisableView",
                       "summaryOn",
                       "summaryOff",
                       "disableDependentsState",
                       "dialogTitle",
                       "dialogMessage",
                       "dialogIcon",
                       "positiveButtonText",
                       "negativeButtonText",
                       "dialogLayout",
                       "entryValues",
                       "ringtoneType",
                       "showDefault",
                       "showSilent",
                       "scaleWidth",
                       "scaleHeight",
                       "scaleGravity",
                       "ignoreGravity",
                       "foregroundGravity",
                       "tileMode",
                       "targetActivity",
                       "alwaysRetainTaskState",
                       "allowTaskReparenting",
                       "searchButtonText",
                       "colorForegroundInverse",
                       "textAppearanceButton",
                       "listSeparatorTextViewStyle",
                       "streamType",
                       "clipOrientation",
                       "centerColor",
                       "minSdkVersion",
                       "windowFullscreen",
                       "unselectedAlpha",
                       "progressBarStyleSmallTitle",
                       "ratingBarStyleIndicator",
                       "apiKey",
                       "textColorTertiary",
                       "textColorTertiaryInverse",
                       "listDivider",
                       "soundEffectsEnabled",
                       "keepScreenOn",
                       "lineSpacingExtra",
                       "lineSpacingMultiplier",
                       "listChoiceIndicatorSingle",
                       "listChoiceIndicatorMultiple",
                       "versionCode",
                       "versionName",
                       "marqueeRepeatLimit",
                       "windowNoDisplay",
                       "backgroundDimEnabled",
                       "inputType",
                       "isDefault",
                       "windowDisablePreview",
                       "privateImeOptions",
                       "editorExtras",
                       "settingsActivity",
                       "fastScrollEnabled",
                       "reqTouchScreen",
                       "reqKeyboardType",
                       "reqHardKeyboard",
                       "reqNavigation",
                       "windowSoftInputMode",
                       "imeFullscreenBackground",
                       "noHistory",
                       "headerDividersEnabled",
                       "footerDividersEnabled",
                       "candidatesTextStyleSpans",
                       "smoothScrollbar",
                       "reqFiveWayNav",
                       "keyBackground",
                       "keyTextSize",
                       "labelTextSize",
                       "keyTextColor",
                       "keyPreviewLayout",
                       "keyPreviewOffset",
                       "keyPreviewHeight",
                       "verticalCorrection",
                       "popupLayout",
                       "state_long_pressable",
                       "keyWidth",
                       "keyHeight",
                       "horizontalGap",
                       "verticalGap",
                       "rowEdgeFlags",
                       "codes",
                       "popupKeyboard",
                       "popupCharacters",
                       "keyEdgeFlags",
                       "isModifier",
                       "isSticky",
                       "isRepeatable",
                       "iconPreview",
                       "keyOutputText",
                       "keyLabel",
                       "keyIcon",
                       "keyboardMode",
                       "isScrollContainer",
                       "fillEnabled",
                       "updatePeriodMillis",
                       "initialLayout",
                       "voiceSearchMode",
                       "voiceLanguageModel",
                       "voicePromptText",
                       "voiceLanguage",
                       "voiceMaxResults",
                       "bottomOffset",
                       "topOffset",
                       "allowSingleTap",
                       "handle",
                       "content",
                       "animateOnClick",
                       "configure",
                       "hapticFeedbackEnabled",
                       "innerRadius",
                       "thickness",
                       "sharedUserLabel",
                       "dropDownWidth",
                       "dropDownAnchor",
                       "imeOptions",
                       "imeActionLabel",
                       "imeActionId",
                       "UNKNOWN",
                       "imeExtractEnterAnimation",
                       "imeExtractExitAnimation",
                       "tension",
                       "extraTension",
                       "anyDensity",
                       "searchSuggestThreshold",
                       "includeInGlobalSearch",
                       "onClick",
                       "targetSdkVersion",
                       "maxSdkVersion",
                       "testOnly",
                       "contentDescription",
                       "gestureStrokeWidth",
                       "gestureColor",
                       "uncertainGestureColor",
                       "fadeOffset",
                       "fadeDuration",
                       "gestureStrokeType",
                       "gestureStrokeLengthThreshold",
                       "gestureStrokeSquarenessThreshold",
                       "gestureStrokeAngleThreshold",
                       "eventsInterceptionEnabled",
                       "fadeEnabled",
                       "backupAgent",
                       "allowBackup",
                       "glEsVersion",
                       "queryAfterZeroResults",
                       "dropDownHeight",
                       "smallScreens",
                       "normalScreens",
                       "largeScreens",
                       "progressBarStyleInverse",
                       "progressBarStyleSmallInverse",
                       "progressBarStyleLargeInverse",
                       "searchSettingsDescription",
                       "textColorPrimaryInverseDisableOnly",
                       "autoUrlDetect",
                       "resizeable",
                       "required",
                       "accountType",
                       "contentAuthority",
                       "userVisible",
                       "windowShowWallpaper",
                       "wallpaperOpenEnterAnimation",
                       "wallpaperOpenExitAnimation",
                       "wallpaperCloseEnterAnimation",
                       "wallpaperCloseExitAnimation",
                       "wallpaperIntraOpenEnterAnimation",
                       "wallpaperIntraOpenExitAnimation",
                       "wallpaperIntraCloseEnterAnimation",
                       "wallpaperIntraCloseExitAnimation",
                       "supportsUploading",
                       "killAfterRestore",
                       "restoreNeedsApplication",
                       "smallIcon",
                       "accountPreferences",
                       "textAppearanceSearchResultSubtitle",
                       "textAppearanceSearchResultTitle",
                       "summaryColumn",
                       "detailColumn",
                       "detailSocialSummary",
                       "thumbnail",
                       "detachWallpaper",
                       "finishOnCloseSystemDialogs",
                       "scrollbarFadeDuration",
                       "scrollbarDefaultDelayBeforeFade",
                       "fadeScrollbars",
                       "colorBackgroundCacheHint",
                       "dropDownHorizontalOffset",
                       "dropDownVerticalOffset",
                       "quickContactBadgeStyleWindowSmall",
                       "quickContactBadgeStyleWindowMedium",
                       "quickContactBadgeStyleWindowLarge",
                       "quickContactBadgeStyleSmallWindowSmall",
                       "quickContactBadgeStyleSmallWindowMedium",
                       "quickContactBadgeStyleSmallWindowLarge",
                       "author",
                       "autoStart",
                       "expandableListViewWhiteStyle",
                       "installLocation",
                       "vmSafeMode",
                       "webTextViewStyle",
                       "restoreAnyVersion",
                       "tabStripLeft",
                       "tabStripRight",
                       "tabStripEnabled",
                       "logo",
                       "xlargeScreens",
                       "immersive",
                       "overScrollMode",
                       "overScrollHeader",
                       "overScrollFooter",
                       "filterTouchesWhenObscured",
                       "textSelectHandleLeft",
                       "textSelectHandleRight",
                       "textSelectHandle",
                       "textSelectHandleWindowStyle",
                       "popupAnimationStyle",
                       "screenSize",
                       "screenDensity",
                       "allContactsName",
                       "windowActionBar",
                       "actionBarStyle",
                       "navigationMode",
                       "displayOptions",
                       "subtitle",
                       "customNavigationLayout",
                       "hardwareAccelerated",
                       "measureWithLargestChild",
                       "animateFirstView",
                       "dropDownSpinnerStyle",
                       "actionDropDownStyle",
                       "actionButtonStyle",
                       "showAsAction",
                       "previewImage",
                       "actionModeBackground",
                       "actionModeCloseDrawable",
                       "windowActionModeOverlay",
                       "valueFrom",
                       "valueTo",
                       "valueType",
                       "propertyName",
                       "ordering",
                       "fragment",
                       "windowActionBarOverlay",
                       "fragmentOpenEnterAnimation",
                       "fragmentOpenExitAnimation",
                       "fragmentCloseEnterAnimation",
                       "fragmentCloseExitAnimation",
                       "fragmentFadeEnterAnimation",
                       "fragmentFadeExitAnimation",
                       "actionBarSize",
                       "imeSubtypeLocale",
                       "imeSubtypeMode",
                       "imeSubtypeExtraValue",
                       "splitMotionEvents",
                       "listChoiceBackgroundIndicator",
                       "spinnerMode",
                       "animateLayoutChanges",
                       "actionBarTabStyle",
                       "actionBarTabBarStyle",
                       "actionBarTabTextStyle",
                       "actionOverflowButtonStyle",
                       "actionModeCloseButtonStyle",
                       "titleTextStyle",
                       "subtitleTextStyle",
                       "iconifiedByDefault",
                       "actionLayout",
                       "actionViewClass",
                       "activatedBackgroundIndicator",
                       "state_activated",
                       "listPopupWindowStyle",
                       "popupMenuStyle",
                       "textAppearanceLargePopupMenu",
                       "textAppearanceSmallPopupMenu",
                       "breadCrumbTitle",
                       "breadCrumbShortTitle",
                       "listDividerAlertDialog",
                       "textColorAlertDialogListItem",
                       "loopViews",
                       "dialogTheme",
                       "alertDialogTheme",
                       "dividerVertical",
                       "homeAsUpIndicator",
                       "enterFadeDuration",
                       "exitFadeDuration",
                       "selectableItemBackground",
                       "autoAdvanceViewId",
                       "useIntrinsicSizeAsMinimum",
                       "actionModeCutDrawable",
                       "actionModeCopyDrawable",
                       "actionModePasteDrawable",
                       "textEditPasteWindowLayout",
                       "textEditNoPasteWindowLayout",
                       "textIsSelectable",
                       "windowEnableSplitTouch",
                       "indeterminateProgressStyle",
                       "progressBarPadding",
                       "animationResolution",
                       "state_accelerated",
                       "baseline",
                       "homeLayout",
                       "opacity",
                       "alpha",
                       "transformPivotX",
                       "transformPivotY",
                       "translationX",
                       "translationY",
                       "scaleX",
                       "scaleY",
                       "rotation",
                       "rotationX",
                       "rotationY",
                       "showDividers",
                       "dividerPadding",
                       "borderlessButtonStyle",
                       "dividerHorizontal",
                       "itemPadding",
                       "buttonBarStyle",
                       "buttonBarButtonStyle",
                       "segmentedButtonStyle",
                       "staticWallpaperPreview",
                       "allowParallelSyncs",
                       "isAlwaysSyncable",
                       "verticalScrollbarPosition",
                       "fastScrollAlwaysVisible",
                       "fastScrollThumbDrawable",
                       "fastScrollPreviewBackgroundLeft",
                       "fastScrollPreviewBackgroundRight",
                       "fastScrollTrackDrawable",
                       "fastScrollOverlayPosition",
                       "customTokens",
                       "nextFocusForward",
                       "firstDayOfWeek",
                       "showWeekNumber",
                       "minDate",
                       "maxDate",
                       "shownWeekCount",
                       "selectedWeekBackgroundColor",
                       "focusedMonthDateColor",
                       "unfocusedMonthDateColor",
                       "weekNumberColor",
                       "weekSeparatorLineColor",
                       "selectedDateVerticalBar",
                       "weekDayTextAppearance",
                       "dateTextAppearance",
                       "UNKNOWN",
                       "spinnersShown",
                       "calendarViewShown",
                       "state_multiline",
                       "detailsElementBackground",
                       "textColorHighlightInverse",
                       "textColorLinkInverse",
                       "editTextColor",
                       "editTextBackground",
                       "horizontalScrollViewStyle",
                       "layerType",
                       "alertDialogIcon",
                       "windowMinWidthMajor",
                       "windowMinWidthMinor",
                       "queryHint",
                       "fastScrollTextColor",
                       "largeHeap",
                       "windowCloseOnTouchOutside",
                       "datePickerStyle",
                       "calendarViewStyle",
                       "textEditSidePasteWindowLayout",
                       "textEditSideNoPasteWindowLayout",
                       "actionMenuTextAppearance",
                       "actionMenuTextColor",
                       "textCursorDrawable",
                       "resizeMode",
                       "requiresSmallestWidthDp",
                       "compatibleWidthLimitDp",
                       "largestWidthLimitDp",
                       "state_hovered",
                       "state_drag_can_accept",
                       "state_drag_hovered",
                       "stopWithTask",
                       "switchTextOn",
                       "switchTextOff",
                       "switchPreferenceStyle",
                       "switchTextAppearance",
                       "track",
                       "switchMinWidth",
                       "switchPadding",
                       "thumbTextPadding",
                       "textSuggestionsWindowStyle",
                       "textEditSuggestionItemLayout",
                       "rowCount",
                       "rowOrderPreserved",
                       "columnCount",
                       "columnOrderPreserved",
                       "useDefaultMargins",
                       "alignmentMode",
                       "layout_row",
                       "layout_rowSpan",
                       "layout_columnSpan",
                       "actionModeSelectAllDrawable",
                       "isAuxiliary",
                       "accessibilityEventTypes",
                       "packageNames",
                       "accessibilityFeedbackType",
                       "notificationTimeout",
                       "accessibilityFlags",
                       "canRetrieveWindowContent",
                       "listPreferredItemHeightLarge",
                       "listPreferredItemHeightSmall",
                       "actionBarSplitStyle",
                       "actionProviderClass",
                       "backgroundStacked",
                       "backgroundSplit",
                       "textAllCaps",
                       "colorPressedHighlight",
                       "colorLongPressedHighlight",
                       "colorFocusedHighlight",
                       "colorActivatedHighlight",
                       "colorMultiSelectHighlight",
                       "drawableStart",
                       "drawableEnd",
                       "actionModeStyle",
                       "minResizeWidth",
                       "minResizeHeight",
                       "actionBarWidgetTheme",
                       "uiOptions",
                       "subtypeLocale",
                       "subtypeExtraValue",
                       "actionBarDivider",
                       "actionBarItemBackground",
                       "actionModeSplitBackground",
                       "textAppearanceListItem",
                       "textAppearanceListItemSmall",
                       "targetDescriptions",
                       "directionDescriptions",
                       "overridesImplicitlyEnabledSubtype",
                       "listPreferredItemPaddingLeft",
                       "listPreferredItemPaddingRight",
                       "requiresFadingEdge",
                       "publicKey",
                       "parentActivityName",
                       "UNKNOWN",
                       "isolatedProcess",
                       "importantForAccessibility",
                       "keyboardLayout",
                       "fontFamily",
                       "mediaRouteButtonStyle",
                       "mediaRouteTypes",
                       "supportsRtl",
                       "textDirection",
                       "textAlignment",
                       "layoutDirection",
                       "paddingStart",
                       "paddingEnd",
                       "layout_marginStart",
                       "layout_marginEnd",
                       "layout_toStartOf",
                       "layout_toEndOf",
                       "layout_alignStart",
                       "layout_alignEnd",
                       "layout_alignParentStart",
                       "layout_alignParentEnd",
                       "listPreferredItemPaddingStart",
                       "listPreferredItemPaddingEnd",
                       "singleUser",
                       "presentationTheme",
                       "subtypeId",
                       "initialKeyguardLayout",
                       "UNKNOWN",
                       "widgetCategory",
                       "permissionGroupFlags",
                       "labelFor",
                       "permissionFlags",
                       "checkedTextViewStyle",
                       "showOnLockScreen",
                       "format12Hour",
                       "format24Hour",
                       "timeZone",
                       "mipMap",
                       "mirrorForRtl",
                       "windowOverscan",
                       "requiredForAllUsers",
                       "indicatorStart",
                       "indicatorEnd",
                       "childIndicatorStart",
                       "childIndicatorEnd",
                       "restrictedAccountType",
                       "requiredAccountType",
                       "canRequestTouchExplorationMode",
                       "canRequestEnhancedWebAccessibility",
                       "canRequestFilterKeyEvents",
                       "layoutMode",
                       "keySet",
                       "targetId",
                       "fromScene",
                       "toScene",
                       "transition",
                       "transitionOrdering",
                       "fadingMode",
                       "startDelay",
                       "ssp",
                       "sspPrefix",
                       "sspPattern",
                       "addPrintersActivity",
                       "vendor",
                       "category",
                       "isAsciiCapable",
                       "autoMirrored",
                       "supportsSwitchingToNextInputMethod",
                       "requireDeviceUnlock",
                       "apduServiceBanner",
                       "accessibilityLiveRegion",
                       "windowTranslucentStatus",
                       "windowTranslucentNavigation",
                       "advancedPrintOptionsActivity",
                       "banner",
                       "windowSwipeToDismiss",
                       "isGame",
                       "allowEmbedded",
                       "setupActivity",
                       "fastScrollStyle",
                       "windowContentTransitions",
                       "windowContentTransitionManager",
                       "translationZ",
                       "tintMode",
                       "controlX1",
                       "controlY1",
                       "controlX2",
                       "controlY2",
                       "transitionName",
                       "transitionGroup",
                       "viewportWidth",
                       "viewportHeight",
                       "fillColor",
                       "pathData",
                       "strokeColor",
                       "strokeWidth",
                       "trimPathStart",
                       "trimPathEnd",
                       "trimPathOffset",
                       "strokeLineCap",
                       "strokeLineJoin",
                       "strokeMiterLimit",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "colorControlNormal",
                       "colorControlActivated",
                       "colorButtonNormal",
                       "colorControlHighlight",
                       "persistableMode",
                       "titleTextAppearance",
                       "subtitleTextAppearance",
                       "slideEdge",
                       "actionBarTheme",
                       "textAppearanceListItemSecondary",
                       "colorPrimary",
                       "colorPrimaryDark",
                       "colorAccent",
                       "nestedScrollingEnabled",
                       "windowEnterTransition",
                       "windowExitTransition",
                       "windowSharedElementEnterTransition",
                       "windowSharedElementExitTransition",
                       "windowAllowReturnTransitionOverlap",
                       "windowAllowEnterTransitionOverlap",
                       "sessionService",
                       "stackViewStyle",
                       "switchStyle",
                       "elevation",
                       "excludeId",
                       "excludeClass",
                       "hideOnContentScroll",
                       "actionOverflowMenuStyle",
                       "documentLaunchMode",
                       "maxRecents",
                       "autoRemoveFromRecents",
                       "stateListAnimator",
                       "toId",
                       "fromId",
                       "reversible",
                       "splitTrack",
                       "targetName",
                       "excludeName",
                       "matchOrder",
                       "windowDrawsSystemBarBackgrounds",
                       "statusBarColor",
                       "navigationBarColor",
                       "contentInsetStart",
                       "contentInsetEnd",
                       "contentInsetLeft",
                       "contentInsetRight",
                       "paddingMode",
                       "layout_rowWeight",
                       "layout_columnWeight",
                       "translateX",
                       "translateY",
                       "selectableItemBackgroundBorderless",
                       "elegantTextHeight",
                       "UNKNOWN",
                       "UNKNOWN",
                       "UNKNOWN",
                       "windowTransitionBackgroundFadeDuration",
                       "overlapAnchor",
                       "progressTint",
                       "progressTintMode",
                       "progressBackgroundTint",
                       "progressBackgroundTintMode",
                       "secondaryProgressTint",
                       "secondaryProgressTintMode",
                       "indeterminateTint",
                       "indeterminateTintMode",
                       "backgroundTint",
                       "backgroundTintMode",
                       "foregroundTint",
                       "foregroundTintMode",
                       "buttonTint",
                       "buttonTintMode",
                       "thumbTint",
                       "thumbTintMode",
                       "fullBackupOnly",
                       "propertyXName",
                       "propertyYName",
                       "relinquishTaskIdentity",
                       "tileModeX",
                       "tileModeY",
                       "actionModeShareDrawable",
                       "actionModeFindDrawable",
                       "actionModeWebSearchDrawable",
                       "transitionVisibilityMode",
                       "minimumHorizontalAngle",
                       "minimumVerticalAngle",
                       "maximumAngle",
                       "searchViewStyle",
                       "closeIcon",
                       "goIcon",
                       "searchIcon",
                       "voiceIcon",
                       "commitIcon",
                       "suggestionRowLayout",
                       "queryBackground",
                       "submitBackground",
                       "buttonBarPositiveButtonStyle",
                       "buttonBarNeutralButtonStyle",
                       "buttonBarNegativeButtonStyle",
                       "popupElevation",
                       "actionBarPopupTheme",
                       "multiArch",
                       "touchscreenBlocksFocus",
                       "windowElevation",
                       "launchTaskBehindTargetAnimation",
                       "launchTaskBehindSourceAnimation",
                       "restrictionType",
                       "dayOfWeekBackground",
                       "dayOfWeekTextAppearance",
                       "headerMonthTextAppearance",
                       "headerDayOfMonthTextAppearance",
                       "headerYearTextAppearance",
                       "yearListItemTextAppearance",
                       "yearListSelectorColor",
                       "calendarTextColor",
                       "recognitionService",
                       "timePickerStyle",
                       "timePickerDialogTheme",
                       "headerTimeTextAppearance",
                       "headerAmPmTextAppearance",
                       "numbersTextColor",
                       "numbersBackgroundColor",
                       "numbersSelectorColor",
                       "amPmTextColor",
                       "amPmBackgroundColor",
                       "UNKNOWN",
                       "checkMarkTint",
                       "checkMarkTintMode",
                       "popupTheme",
                       "toolbarStyle",
                       "windowClipToOutline",
                       "datePickerDialogTheme",
                       "showText",
                       "windowReturnTransition",
                       "windowReenterTransition",
                       "windowSharedElementReturnTransition",
                       "windowSharedElementReenterTransition",
                       "resumeWhilePausing",
                       "datePickerMode",
                       "timePickerMode",
                       "inset",
                       "letterSpacing",
                       "fontFeatureSettings",
                       "outlineProvider",
                       "contentAgeHint",
                       "country",
                       "windowSharedElementsUseOverlay",
                       "reparent",
                       "reparentWithOverlay",
                       "ambientShadowAlpha",
                       "spotShadowAlpha",
                       "navigationIcon",
                       "navigationContentDescription",
                       "fragmentExitTransition",
                       "fragmentEnterTransition",
                       "fragmentSharedElementEnterTransition",
                       "fragmentReturnTransition",
                       "fragmentSharedElementReturnTransition",
                       "fragmentReenterTransition",
                       "fragmentAllowEnterTransitionOverlap",
                       "fragmentAllowReturnTransitionOverlap",
                       "patternPathData",
                       "strokeAlpha",
                       "fillAlpha",
                       "windowActivityTransitions",
                       "colorEdgeEffect",
                       "resizeClip",
                       "collapseContentDescription",
                       "accessibilityTraversalBefore",
                       "accessibilityTraversalAfter",
                       "dialogPreferredPadding",
                       "searchHintIcon",
                       "revisionCode",
                       "drawableTint",
                       "drawableTintMode",
                       "fraction",
                       "trackTint",
                       "trackTintMode",
                       "start",
                       "end",
                       "breakStrategy",
                       "hyphenationFrequency",
                       "allowUndo",
                       "windowLightStatusBar",
                       "numbersInnerTextColor",
                       "colorBackgroundFloating",
                       "titleTextColor",
                       "subtitleTextColor",
                       "thumbPosition",
                       "scrollIndicators",
                       "contextClickable",
                       "fingerprintAuthDrawable",
                       "logoDescription",
                       "extractNativeLibs",
                       "fullBackupContent",
                       "usesCleartextTraffic",
                       "lockTaskMode",
                       "autoVerify",
                       "showForAllUsers",
                       "supportsAssist",
                       "supportsLaunchVoiceAssistFromKeyguard",
                       "listMenuViewStyle",
                       "subMenuArrow",
                       "defaultWidth",
                       "defaultHeight",
                       "resizeableActivity",
                       "supportsPictureInPicture",
                       "titleMargin",
                       "titleMarginStart",
                       "titleMarginEnd",
                       "titleMarginTop",
                       "titleMarginBottom",
                       "maxButtonHeight",
                       "buttonGravity",
                       "collapseIcon",
                       "level",
                       "contextPopupMenuStyle",
                       "textAppearancePopupMenuHeader",
                       "windowBackgroundFallback",
                       "defaultToDeviceProtectedStorage",
                       "directBootAware",
                       "preferenceFragmentStyle",
                       "canControlMagnification",
                       "languageTag",
                       "pointerIcon",
                       "tickMark",
                       "tickMarkTint",
                       "tickMarkTintMode",
                       "canPerformGestures",
                       "externalService",
                       "supportsLocalInteraction",
                       "startX",
                       "startY",
                       "endX",
                       "endY",
                       "offset",
                       "use32bitAbi",
                       "bitmap",
                       "hotSpotX",
                       "hotSpotY",
                       "version",
                       "backupInForeground",
                       "countDown",
                       "canRecord",
                       "tunerCount",
                       "fillType",
                       "popupEnterTransition",
                       "popupExitTransition",
                       "forceHasOverlappingRendering",
                       "contentInsetStartWithNavigation",
                       "contentInsetEndWithActions",
                       "numberPickerStyle",
                       "enableVrMode",
                       "UNKNOWN",
                       "networkSecurityConfig",
                       "shortcutId",
                       "shortcutShortLabel",
                       "shortcutLongLabel",
                       "shortcutDisabledMessage",
                       "roundIcon",
                       "contextUri",
                       "contextDescription",
                       "showMetadataInPreview",
                       "colorSecondary"};

            // For now, we only care about the attribute names.
            id -= 0x1010000;
            if (id >= sizeof(attr_names) / sizeof(attr_names[0])) {
				char id_string[32];
                // sprintf(id_string, "%d", id);
				snprintf(id_string, sizeof(id_string), "%d", id);
                std::stringstream ss;
	            ss << "unknown_" << id_string;
                return ss.str();
            }
            return attr_names[id];
        }

    private:
        void fail(jitana::axml_parser::error_code code, std::string msg)
        {
            if (!error_) {
                error_ = jitana::axml_parser::error{code, std::move(msg)};
            }
        }

        bool failed() const noexcept
        {
            return error_.has_value() || reader_.has_error();
        }

        jitana::stream_reader& reader_;
        std::vector<std::string> strings_;
        jitana::xml_node& root_;
        std::optional<jitana::axml_parser::error> error_;

        struct xml_stack_item {
            jitana::xml_node* node;
            std::vector<std::pair<uint32_t, uint32_t>> namespaces;

            explicit xml_stack_item(jitana::xml_node* node) : node(node)
            {
            }
        };
        std::vector<xml_stack_item> xml_stack_;
    };
}

std::optional<std::string> jitana::xml_node::get_attribute_value(const std::string& name) const noexcept {
    auto it = std::find_if(attributes.begin(), attributes.end(), [&](const std::pair<std::string, std::string>& attr) {
        return attr.first == name;
    });
    if (it != attributes.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<jitana::axml_parser::error>
jitana::read_axml(const std::string& filename, jitana::xml_node& root)
{
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) {
        return jitana::axml_parser::error{
                jitana::axml_parser::error_code::io_error,
                "failed to open " + filename};
    }
    return read_axml(ifs, root);
}

std::optional<jitana::axml_parser::error>
jitana::read_axml(std::istream& stream, jitana::xml_node& root)
{
    std::vector<uint8_t> buffer;
    std::for_each(std::istreambuf_iterator<char>(stream),
                  std::istreambuf_iterator<char>(),
                  [&buffer](char c) {
                      buffer.push_back(static_cast<uint8_t>(c));
                  });
    return read_axml(buffer.data(), buffer.size(), root);
}

std::optional<jitana::axml_parser::error>
jitana::read_axml(const uint8_t* data, std::size_t size,
                  jitana::xml_node& root)
{
    stream_reader reader(data, data + size);
    axml_parser_impl p(reader, root);
    p.parse();
    return p.take_error();
}
