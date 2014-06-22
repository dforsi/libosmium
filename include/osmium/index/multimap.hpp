#ifndef OSMIUM_INDEX_MULTIMAP_HPP
#define OSMIUM_INDEX_MULTIMAP_HPP

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

#include <cstddef>
#include <type_traits>
#include <utility>

#include <osmium/index/index.hpp>

namespace osmium {

    namespace index {

        /**
         * @brief Key-value containers with multiple values for an integer key
         */
        namespace multimap {

            template <typename TId, typename TValue>
            class Multimap {

                static_assert(std::is_integral<TId>::value && std::is_unsigned<TId>::value,
                              "TId template parameter for class Multimap must be unsigned integral type");

                typedef typename std::pair<TId, TValue> element_type;

                Multimap(const Multimap&) = delete;
                Multimap& operator=(const Multimap&) = delete;

            protected:

                Multimap(Multimap&&) = default;
                Multimap& operator=(Multimap&&) = default;

            public:

                /// The "key" type, usually osmium::unsigned_object_id_type.
                typedef TId key_type;

                /// The "value" type, usually a Location or size_t.
                typedef TValue value_type;

                Multimap() = default;

                virtual ~Multimap() noexcept = default;

                /// Set the field with id to value.
                virtual void set(const TId id, const TValue value) = 0;

                typedef element_type* iterator;

//                virtual std::pair<iterator, iterator> get_all(const TId id) const = 0;

                /**
                 * Get the approximate number of items in the storage. The storage
                 * might allocate memory in blocks, so this size might not be
                 * accurate. You can not use this to find out how much memory the
                 * storage uses. Use used_memory() for that.
                 */
                virtual size_t size() const = 0;

                /**
                 * Get the memory used for this storage in bytes. Note that this
                 * is not necessarily entirely accurate but an approximation.
                 * For storage classes that store the data in memory, this is
                 * the main memory used, for storage classes storing data on disk
                 * this is the memory used on disk.
                 */
                virtual size_t used_memory() const = 0;

                /**
                 * Clear memory used for this storage. After this you can not
                 * use the storage container any more.
                 */
                virtual void clear() = 0;

                /**
                 * Sort data in map. Call this after writing all data and
                 * before reading. Not all implementations need this.
                 */
                virtual void sort() {
                    // default implementation is empty
                }

            }; // class Multimap

        } // namespace map

    } // namespace index

} // namespace osmium

#endif // OSMIUM_INDEX_MULTIMAP_HPP
