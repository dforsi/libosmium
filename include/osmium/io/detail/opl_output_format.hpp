#ifndef OSMIUM_IO_DETAIL_OPL_OUTPUT_FORMAT_HPP
#define OSMIUM_IO_DETAIL_OPL_OUTPUT_FORMAT_HPP

/*

This file is part of Osmium (http://osmcode.org/libosmium).

Copyright 2013,2014 Jochen Topf <jochen@topf.org> and others (see README).

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

#include <chrono>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <future>
#include <iterator>
#include <memory>
#include <ratio>
#include <string>
#include <thread>
#include <utility>

#include <boost/version.hpp>

#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wmissing-noreturn"
# pragma clang diagnostic ignored "-Wsign-conversion"
#endif

#if BOOST_VERSION >= 104800
# include <boost/regex/pending/unicode_iterator.hpp>
#else
# include <boost_unicode_iterator.hpp>
#endif

#ifdef __clang__
# pragma clang diagnostic pop
#endif

#include <osmium/handler.hpp>
#include <osmium/io/detail/output_format.hpp>
#include <osmium/io/file_format.hpp>
#include <osmium/memory/buffer.hpp>
#include <osmium/memory/collection.hpp>
#include <osmium/osm/box.hpp>
#include <osmium/osm/changeset.hpp>
#include <osmium/osm/item_type.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/osm/node.hpp>
#include <osmium/osm/object.hpp>
#include <osmium/osm/relation.hpp>
#include <osmium/osm/tag.hpp>
#include <osmium/osm/timestamp.hpp>
#include <osmium/osm/way.hpp>
#include <osmium/thread/pool.hpp>
#include <osmium/visitor.hpp>

namespace osmium {

    namespace io {

        class File;

        namespace detail {

            /**
             * Writes out one buffer with OSM data in OPL format.
             */
            class OPLOutputBlock : public osmium::handler::Handler {

                static constexpr size_t tmp_buffer_size = 100;

                osmium::memory::Buffer m_input_buffer;

                std::string m_out;

                char m_tmp_buffer[tmp_buffer_size+1];

                template <typename... TArgs>
                void output_formatted(const char* format, TArgs&&... args) {
#ifndef _MSC_VER
                    int len = snprintf(m_tmp_buffer, tmp_buffer_size, format, std::forward<TArgs>(args)...);
#else
                    int len = _snprintf(m_tmp_buffer, tmp_buffer_size, format, std::forward<TArgs>(args)...);
#endif
                    assert(len > 0 && static_cast<size_t>(len) < tmp_buffer_size);
                    m_out += m_tmp_buffer;
                }

                void append_encoded_string(const std::string& data) {
                    boost::u8_to_u32_iterator<std::string::const_iterator> it(data.cbegin(), data.cbegin(), data.cend());
                    boost::u8_to_u32_iterator<std::string::const_iterator> end(data.cend(), data.cend(), data.cend());
                    boost::utf8_output_iterator<std::back_insert_iterator<std::string>> oit(std::back_inserter(m_out));

                    for (; it != end; ++it) {
                        uint32_t c = *it;

                        // This is a list of Unicode code points that we let
                        // through instead of escaping them. It is incomplete
                        // and can be extended later.
                        // Generally we don't want to let through any character
                        // that has special meaning in the OPL format such as
                        // space, comma, @, etc. and any non-printing characters.
                        if ((0x0021 <= c && c <= 0x0024) ||
                            (0x0026 <= c && c <= 0x002b) ||
                            (0x002d <= c && c <= 0x003c) ||
                            (0x003e <= c && c <= 0x003f) ||
                            (0x0041 <= c && c <= 0x007e) ||
                            (0x00a1 <= c && c <= 0x00ac) ||
                            (0x00ae <= c && c <= 0x05ff)) {
                            *oit = c;
                        } else {
                            m_out += '%';
                            output_formatted("%04x", c);
                        }
                    }
                }

                void write_meta(const osmium::OSMObject& object) {
                    output_formatted("%" PRId64 " v%d d", object.id(), object.version());
                    m_out += (object.visible() ? 'V' : 'D');
                    output_formatted(" c%d t", object.changeset());
                    m_out += object.timestamp().to_iso();
                    output_formatted(" i%d u", object.uid());
                    append_encoded_string(object.user());
                    m_out += " T";
                    bool first = true;
                    for (const auto& tag : object.tags()) {
                        if (first) {
                            first = false;
                        } else {
                            m_out += ',';
                        }
                        append_encoded_string(tag.key());
                        m_out += '=';
                        append_encoded_string(tag.value());
                    }
                }

                void write_location(const osmium::Location location, const char x, const char y) {
                    if (location) {
                        output_formatted(" %c%.7f %c%.7f", x, location.lon_without_check(), y, location.lat_without_check());
                    } else {
                        m_out += ' ';
                        m_out += x;
                        m_out += ' ';
                        m_out += y;
                    }
                }

            public:

                explicit OPLOutputBlock(osmium::memory::Buffer&& buffer) :
                    m_input_buffer(std::move(buffer)),
                    m_out(),
                    m_tmp_buffer() {
                }

                OPLOutputBlock(const OPLOutputBlock&) = delete;
                OPLOutputBlock& operator=(const OPLOutputBlock&) = delete;

                OPLOutputBlock(OPLOutputBlock&& other) = default;
                OPLOutputBlock& operator=(OPLOutputBlock&& other) = default;

                std::string operator()() {
                    osmium::apply(m_input_buffer.cbegin(), m_input_buffer.cend(), *this);

                    std::string out;
                    std::swap(out, m_out);
                    return out;
                }

                void node(const osmium::Node& node) {
                    m_out += 'n';
                    write_meta(node);
                    write_location(node.location(), 'x', 'y');
                    m_out += '\n';
                }

                void way(const osmium::Way& way) {
                    m_out += 'w';
                    write_meta(way);

                    m_out += " N";
                    bool first = true;
                    for (const auto& node_ref : way.nodes()) {
                        if (first) {
                            first = false;
                        } else {
                            m_out += ',';
                        }
                        output_formatted("n%" PRId64, node_ref.ref());
                    }
                    m_out += '\n';
                }

                void relation(const osmium::Relation& relation) {
                    m_out += 'r';
                    write_meta(relation);

                    m_out += " M";
                    bool first = true;
                    for (const auto& member : relation.members()) {
                        if (first) {
                            first = false;
                        } else {
                            m_out += ',';
                        }
                        m_out += item_type_to_char(member.type());
                        output_formatted("%" PRId64 "@", member.ref());
                        m_out += member.role();
                    }
                    m_out += '\n';
                }

                void changeset(const osmium::Changeset& changeset) {
                    output_formatted("c%d k%d s", changeset.id(), changeset.num_changes());
                    m_out += changeset.created_at().to_iso();
                    m_out += " e";
                    m_out += changeset.closed_at().to_iso();
                    output_formatted(" i%d u", changeset.uid());
                    append_encoded_string(changeset.user());
                    write_location(changeset.bounds().bottom_left(), 'x', 'y');
                    write_location(changeset.bounds().top_right(), 'X', 'Y');
                    m_out += " T";
                    bool first = true;
                    for (const auto& tag : changeset.tags()) {
                        if (first) {
                            first = false;
                        } else {
                            m_out += ',';
                        }
                        append_encoded_string(tag.key());
                        m_out += '=';
                        append_encoded_string(tag.value());
                    }

                    m_out += '\n';
                }

            }; // OPLOutputBlock

            class OPLOutputFormat : public osmium::io::detail::OutputFormat {

                OPLOutputFormat(const OPLOutputFormat&) = delete;
                OPLOutputFormat& operator=(const OPLOutputFormat&) = delete;

            public:

                OPLOutputFormat(const osmium::io::File& file, data_queue_type& output_queue) :
                    OutputFormat(file, output_queue) {
                }

                void write_buffer(osmium::memory::Buffer&& buffer) override final {
                    osmium::thread::SharedPtrWrapper<OPLOutputBlock> output_block(std::move(buffer));
                    m_output_queue.push(osmium::thread::Pool::instance().submit(std::move(output_block)));
                    while (m_output_queue.size() > 10) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // XXX
                    }
                }

                void close() override final {
                    std::string out;
                    std::promise<std::string> promise;
                    m_output_queue.push(promise.get_future());
                    promise.set_value(out);
                }

            }; // class OPLOutputFormat

            namespace {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
                const bool registered_opl_output = osmium::io::detail::OutputFormatFactory::instance().register_output_format(osmium::io::file_format::opl,
                    [](const osmium::io::File& file, data_queue_type& output_queue) {
                        return new osmium::io::detail::OPLOutputFormat(file, output_queue);
                });
#pragma GCC diagnostic pop

            } // anonymous namespace

        } // namespace detail

    } // namespace io

} // namespace osmium

#endif // OSMIUM_IO_DETAIL_OPL_OUTPUT_FORMAT_HPP
