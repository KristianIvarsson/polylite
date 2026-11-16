//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "tale.hpp"
#include "type.hpp"
#include "help.hpp"

#include <array>
#include <tuple>
#include <format>
#include <ranges>
#include <string>
#include <cstring>
#include <istream>
#include <utility>
#include <charconv>
#include <concepts>
#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace poly_version
{
   namespace core::load
   {
      namespace pipe
      {
         //! something the parsers can read from
         template< typename type>
         concept source = requires( type& origin) { origin.good(); origin.data(); origin.tell(); origin.hint(); };

         //! the size of the buffer for sources that need that
         constexpr std::size_t gage = 1024;
         //! the size hint when the actual remaining size isn't known
         constexpr std::size_t seed = 1024;

         //! contiguous source, i.e. everything is already available
         template< type::byte unit>
         class memory : type::pinned
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
         template< type::byte unit, typename iterator, typename sentinel>
         class cursor : type::pinned
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
               if( static_cast< std::size_t>( last - mark) < want) fill();
            }

            void fill()
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
         template< type::byte unit>
         class stream : type::pinned
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
               if( static_cast< std::size_t>( last - mark) < want) fill();
            }

            void fill()
            {
               past += mark - buffer.data();
               const auto left = static_cast< std::size_t>( last - mark);
               std::memmove( buffer.data(), mark, left);
               mark = buffer.data();
               last = mark + left + from.rdbuf()->sgetn( buffer.data() + left, gage - left);
            }
         };
      } // pipe

      template< type::byte unit, pipe::source origin>
      struct source
      {
         origin feed;
         const unit*& mark;
         const unit* const& last;

         explicit source( auto& input) : feed{ input}, mark{ std::get< 0>( feed.data())}, last{ std::get< 1>( feed.data())} {}

         //! helper to prevent evil size
         auto clip( const std::size_t size) const
         {
            return std::min( size, feed.hint());
         }

         bool good()
         {
            return feed.good();
         }

         void done()
         {
            if( good()) [[unlikely]] halt( "expected end of stream");
         }

         void drop()
         {
            ++mark;
         }

         char look() const
         {
            return static_cast< char>( *mark);
         }

         char take()
         {
            return static_cast< char>( *mark++);
         }

         char peek()
         {
            if( good()) [[likely]] return look();
            return static_cast< char>( std::char_traits< char>::eof());
         }

         char pull()
         {
            if( good()) [[likely]] return take();
            halt( "unexpected end");
         }

         auto chop( const std::size_t size) -> std::string_view
         {
            auto [ head, tail] = feed.data( size);

            if( static_cast< std::size_t>( tail - head) < size)
               [[unlikely]] halt( "unexpected end");

            const auto lead = reinterpret_cast< const char*>( head);
            head += size;
            return { lead, size};
         }         

         template< typename into>
         auto grab( std::size_t size) -> into
         {
            using type = typename into::value_type;

            into nrv;
            nrv.reserve( clip( size));

            while( size)
            {
               auto [ head, tail] = feed.data( size);

               if( head == tail)
                  [[unlikely]] halt( "unexpected end");

               const auto part = std::min( size, static_cast< std::size_t>( tail - head));
               const auto lead = reinterpret_cast< const type*>( head);
               nrv.insert( nrv.end(), lead, lead + part);
               head += part;
               size -= part;
            }

            return nrv;
         }

         void test( const char want, const char pick) const
         {
            if( want != pick) [[unlikely]] halt( "unexpected sign");
         }

         void test( const char want)
         {
            test( want, pull());
         }

         void test( const std::string_view want)
         {
            if( chop( want.size()) != want)
               [[unlikely]] halt( "unexpected data");
         }

         [[noreturn]] void halt( const std::string_view message) const
         {
            throw std::runtime_error{ std::format( "{} at byte {}", message, feed.tell())};
         }

         [[noreturn]] void deny( const std::string_view message) const
         {
            halt( std::format( "unsupported {}", message));
         }
      };

      template< type::byte unit, pipe::source origin>
      struct parser : source< unit, origin>
      {
         using base = source< unit, origin>;
         using base::base;
         using base::chop;
         using base::deny;
         using base::feed;
         using base::good;
         using base::halt;
         using base::last;
         using base::mark;
         using base::look;
         using base::pull;
         using base::take;
         using base::test;

         void read( auto&& till, std::string& into)
         {
            do
            {
               auto [ head, tail] = feed.data();
               const auto lead = head;
               head = std::find_if_not( head, tail, [&]( const unit sign) { return till( static_cast< char>( sign)); });
               into.append( reinterpret_cast< const char*>( lead), static_cast< std::size_t>( head - lead));
            }
            while( mark == last && good());
         }

         auto read( auto&& till)
         {
            std::string nrv;
            read( till, nrv);
            return nrv;
         }

         auto leap( auto&& till)
         {
            do
            {
               auto [ head, tail] = feed.data();
               head = std::find_if_not( head, tail, [&]( const unit sign) { return till( static_cast< char>( sign)); });
            }
            while( mark == last && good());
         }

         //! rest of the line
         void rest()
         {
            leap( help::till::line);
            if( good()) ++mark; // '\n'
         }

         //! skip whitespace
         void skip()
         {
            leap( [] ( const auto sign) { return help::is::white( sign);});
         }

         template< std::size_t nibs>
         auto code()
         {
            const auto data = chop( nibs);

            type::i32 nrv;

            if( const auto [ stop, code] = std::from_chars( data.data(), data.data() + data.size(), nrv, 16); code != std::errc{} || stop != data.data() + data.size())
               [[unlikely]] halt( "invalid code point");

            return nrv;
         }

         auto half()
         {
            return code< 2>(); // \xHH
         }

         auto real()
         {
            return code< 4>(); // \uHHHH
         }

         auto full()
         {
            return code< 8>(); // \UHHHHHHHH
         }


         // unicode escape sequence
         auto code()
         {
            const auto lead = real();

            if( lead < 0xD800 || lead > 0xDFFF)
               return lead;

            if( lead > 0xDBFF)
               [[unlikely]] halt( "invalid 1st surrogate");

            test( "\\u");

            const auto tail = real();

            if( tail < 0xDC00 || tail > 0xDFFF)
               [[unlikely]] halt( "invalid 2nd surrogate");

            return 0x10000 + ( ( lead - 0xD800) << 10) + ( tail - 0xDC00);
         }

         // c-style escape sequences
         auto cast( const type::byte auto sign) -> type::i32
         {
            switch( sign)
            {
            case '\\':return '\\';
            case '"': return '\"';
            case 'b': return '\b';
            case 'f': return '\f';
            case 'n': return '\n';
            case 'r': return '\r';
            case 't': return '\t';
            case '/': return '/';
            case 'u': return code();
            default: [[unlikely]] halt( "invalid escape character");
            }
         }
      };

      namespace make
      {
         template< template< typename, typename, auto...> class parser, auto... flags>
         auto source( const std::ranges::contiguous_range auto& value)
         {
            using unit = std::ranges::range_value_t< decltype( value)>;
            return parser< unit, pipe::memory< unit>, flags...>( value);
         }

         template< template< typename, typename, auto...> class parser, auto... flags, typename unit>
         auto source( std::basic_istream< unit>& value)
         {
            return parser< unit, pipe::stream< unit>, flags...>( value);
         }

         template< template< typename, typename, auto...> class parser, auto... flags>
         auto source( const std::ranges::input_range auto& value)
            requires ( ! std::ranges::contiguous_range< decltype( value)>)
         {
            using range = std::remove_cvref_t< decltype( value)>;
            using unit  = std::ranges::range_value_t< range>;
            using cursor = pipe::cursor< unit, std::ranges::iterator_t< const range>, std::ranges::sentinel_t< const range>>;
            return parser< unit, cursor, flags...>( value);
         }

         // utility for string literals (automatically deduces size and type) (essentially for testing)
         template< template< typename, typename, auto...> class parser, auto... flags, std::size_t size>
         auto source( const char (&string)[ size])
         {
            return source< parser, flags...>( std::string_view{ string, size - 1});
         }
      } // make
   } // core::load
} // poly_version
