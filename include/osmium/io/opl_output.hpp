#ifndef OSMIUM_IO_OPL_OUTPUT_HPP
#define OSMIUM_IO_OPL_OUTPUT_HPP

/*

This file is part of Osmium (http://osmcode.org/osmium).

Copyright 2013 Jochen Topf <jochen@topf.org> and others (see README).

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

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <cstdio>

#include <osmium/io/output.hpp>
#include <osmium/io/detail/read_write.hpp>
#include <osmium/handler.hpp>
#include <osmium/utils/timestamp.hpp>

namespace osmium {

    namespace io {

        class OPLOutput : public osmium::io::Output, public osmium::handler::Handler<OPLOutput> {

            // size of the output buffer, there is one system call for each
            // time this is flushed, so it shouldn't be too small
            static const size_t output_buffer_size = 1024*1024;

            // temporary buffer for writing out IDs and other numbers, must
            // be big enough to always hold them
            static const size_t tmp_buffer_size = 100;

            std::string m_out;

            char m_tmp_buffer[tmp_buffer_size+1];

            OPLOutput(const OPLOutput&) = delete;
            OPLOutput& operator=(const OPLOutput&) = delete;

        public:

            OPLOutput(const osmium::io::File& file) :
                Output(file),
                m_out(),
                m_tmp_buffer() {
                m_out.reserve(output_buffer_size * 2);
            }

            void handle_collection(osmium::memory::Buffer::const_iterator begin, osmium::memory::Buffer::const_iterator end) override {
                this->operator()(begin, end);
            }

            void node(const osmium::Node& node) {
                m_out += 'n';
                write_meta(node);

                if (node.location()) {
                    snprintf(m_tmp_buffer, tmp_buffer_size, " x%.7f y%.7f", node.lon(), node.lat());
                    m_out += m_tmp_buffer;
                } else {
                    m_out += " x y";
                }

                write_tags(node.tags());

                if (m_out.size() > output_buffer_size) {
                    flush();
                }
            }

            void way(const osmium::Way& way) {
                m_out += 'w';
                write_meta(way);

                m_out += " N";
                bool first = true;
                for (const auto& wn : way.nodes()) {
                    if (first) {
                        first = false;
                    } else {
                        m_out += ',';
                    }
                    snprintf(m_tmp_buffer, tmp_buffer_size, "n%" PRId64, wn.ref());
                    m_out += m_tmp_buffer;
                }

                write_tags(way.tags());

                if (m_out.size() > output_buffer_size) {
                    flush();
                }
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
                    snprintf(m_tmp_buffer, tmp_buffer_size, "%" PRId64 "@", member.ref());
                    m_out += m_tmp_buffer;
                    m_out += member.role();
                }

                write_tags(relation.tags());

                if (m_out.size() > output_buffer_size) {
                    flush();
                }
            }

            void close() override {
                flush();
            }

        private:

            void flush() {
                osmium::io::detail::reliable_write(this->fd(), m_out.c_str(), m_out.size());
                m_out.clear();
            }

            void append_encoded_string(const std::string& data) {
                static constexpr char hex[] = "0123456789abcdef";

                for (char c : data) {
                    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == ':' || c == ';') {
                        m_out += c;
                    } else {
                        m_out += '%';
                        m_out += hex[(c & 0xf0) >> 4];
                        m_out += hex[c & 0x0f];
                    }
                }
            }

            void write_meta(const osmium::Object& object) {
                snprintf(m_tmp_buffer, tmp_buffer_size, "%" PRId64 " v%d d", object.id(), object.version());
                m_out += m_tmp_buffer;
                m_out += (object.visible() ? 'V' : 'D');
                snprintf(m_tmp_buffer, tmp_buffer_size, " c%d t", object.changeset());
                m_out += m_tmp_buffer;
                m_out += timestamp::to_iso(object.timestamp());
                snprintf(m_tmp_buffer, tmp_buffer_size, " i%d u", object.uid());
                m_out += m_tmp_buffer;
                append_encoded_string(object.user());
            }

            void write_tags(const osmium::TagList& tags) {
                m_out += " T";
                bool first = true;
                for (auto& tag : tags) {
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

        }; // class OPLOutput

        namespace {

            const bool registered_opl_output = osmium::io::OutputFactory::instance().register_output_format({
                osmium::io::Encoding::OPL(),
                osmium::io::Encoding::OPLgz(),
                osmium::io::Encoding::OPLbz2()
            }, [](const osmium::io::File& file) {
                return new osmium::io::OPLOutput(file);
            });

        } // anonymous namespace

    } // namespace io

} // namespace osmium

#endif // OSMIUM_IO_OPL_OUTPUT_HPP
