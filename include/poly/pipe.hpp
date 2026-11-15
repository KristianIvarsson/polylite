//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "tale.hpp"

#include <array>
#include <tuple>
#include <ranges>
#include <cstring>
#include <utility>
#include <istream>
#include <algorithm>

namespace poly_version
{
   namespace pipe
   {
      template< typename unit>
      concept byte = sizeof( unit) == 1;

      //! something the parsers can read from
      template< typename type>
      concept source = requires( type& origin) { origin.good(); origin.data(); origin.tell(); origin.hint(); };
      
      
      //! the size of the buffer for sources that need that
      constexpr std::size_t gage = 1024;
      //! the size hint when the actual remaining size isn't known
      constexpr std::size_t seed = 1024;      


      //! prevents copying (and moving) of types referring to their own members
      struct pinned
      {
         pinned() = default;
         pinned( const pinned&) = delete;
         pinned& operator=( const pinned&) = delete;
      };

      namespace parse
      {

         //! contiguous source, i.e. everything is already available
         template< byte unit>
         class memory : pinned
         {
         private:

            const unit* mark;
            const unit* last;
            const unit* root{ mark};

         public:

            explicit memory( const std::ranges::contiguous_range auto& source)
               : mark{ std::ranges::data( source)}, last{ mark + std::ranges::size( source)} {}

            bool good() const { return mark != last; }

            auto data( std::size_t = 0) { return std::tie( mark, std::as_const( last)); }

            auto tell() const -> std::size_t { return mark - root; }

            auto hint() const -> std::size_t { return last - mark; }
         };

         template< std::ranges::contiguous_range source>
         memory( const source&) -> memory< std::ranges::range_value_t< source>>;

         //! fills an internal buffer from a non contiguous range as needed
         template< byte unit, typename iterator, typename sentinel>
         class cursor : pinned
         {
         private:

            iterator head;
            sentinel tail;
            std::array< unit, gage> buffer;
            const unit* mark = buffer.data();
            const unit* last = buffer.data();
            std::size_t past{};
            std::size_t size{};

         public:

            explicit cursor( const std::ranges::input_range auto& source)
               : head{ std::ranges::begin( source)}, tail{ std::ranges::end( source)}
            {
               if constexpr( std::ranges::sized_range< decltype( source)>)
                  size = std::ranges::size( source);
            }

            bool good() { return fill( 1), mark != last;}

            auto data( const std::size_t want = 1) { return fill( want), std::tie( mark, std::as_const( last));}

            auto tell() const -> std::size_t { return past + ( mark - buffer.data()); }

            auto hint() const -> std::size_t { return size ? size - tell() : std::max< std::size_t>( last - mark, seed); }

         private:

            void fill( const std::size_t want)
            {
               if( static_cast< std::size_t>( last - mark) < want) load();
            }

            void load()
            {
               past += mark - buffer.data();
               const auto left = static_cast< std::size_t>( last - mark);
               std::memmove( buffer.data(), mark, left);
               mark = buffer.data();

               auto into = buffer.data() + left;

               if constexpr( std::sized_sentinel_for< sentinel, iterator>)
               {
                  const auto part = std::min< std::size_t>( gage - left, tail - head);
                  into = std::copy( head, head + part, into);
                  head += part;
               }
               else
               {
                  while( into != buffer.data() + gage && head != tail)
                     *into++ = *head++;
               }                  

               last = into;
            }
         };

         template< std::ranges::input_range source>
         cursor( const source&) -> cursor< std::ranges::range_value_t< source>, std::ranges::iterator_t< const source>, std::ranges::sentinel_t< const source>>;

         //! fills an internal buffer from an istream as needed
         template< byte unit>
         class stream : pinned
         {
         private:

            std::basic_istream< unit>& from;
            std::array< unit, gage> buffer;
            const unit* mark = buffer.data();
            const unit* last = buffer.data();
            std::size_t past{};

         public:

            explicit stream( std::basic_istream< unit>& from) : from{ from} {}

            bool good() { return fill( 1), mark != last;}

            auto data( const std::size_t want = 1) { return fill( want), std::tie( mark, std::as_const( last));}

            auto tell() const -> std::size_t { return past + ( mark - buffer.data()); }

            auto hint() const -> std::size_t { return std::max< std::size_t>( last - mark, seed); }

         private:

            void fill( const std::size_t want)
            {
               if( static_cast< std::size_t>( last - mark) < want) load();
            }

            void load()
            {
               past += mark - buffer.data();
               const auto left = static_cast< std::size_t>( last - mark);
               std::memmove( buffer.data(), mark, left);
               mark = buffer.data();
               last = mark + left + from.rdbuf()->sgetn( buffer.data() + left, gage - left);
            }
         };
      } // parse

   } // pipe
} // poly_version
