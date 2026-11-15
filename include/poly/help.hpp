//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "tale.hpp"
#include "node.hpp"
#include "pipe.hpp"

#include <chrono>
#include <format>
#include <limits>
#include <ranges>
#include <string>
#include <cstdint>
#include <istream>
#include <ostream>
#include <charconv>
#include <iterator>
#include <optional>
#include <algorithm>
#include <stdexcept>
#include <spanstream>
#include <string_view>

#if __has_include(<stdfloat>)
#include <stdfloat>
#endif

namespace poly_version
{
   namespace help
   {
      namespace type
      {
         using i08 = std::int8_t;
         using i16 = std::int16_t;
         using i32 = std::int32_t;
         using i64 = std::int64_t;

         using u08 = std::uint8_t;
         using u16 = std::uint16_t;
         using u32 = std::uint32_t;
         using u64 = std::uint64_t;

#if defined(__STDCPP_FLOAT32_T__)
         using f32 = std::float32_t;
#else
         static_assert( std::numeric_limits< float>::is_iec559 && sizeof( float) == sizeof( u32));
         using f32 = float;
#endif

#if defined(__STDCPP_FLOAT64_T__)
         using f64 = std::float64_t;
#else
         static_assert( std::numeric_limits< double>::is_iec559 && sizeof( double) == sizeof( u64));
         using f64 = double;
#endif
      } // type



      template< typename... types>
      constexpr bool hold( const auto& data)
      {
         return ( std::holds_alternative< types>( data) || ... );
      }

      //! for performance (only)
      namespace is
      {
         namespace in
         {
            template< auto lower, auto upper>
            bool range( const auto sign)
            {
               return sign >= lower && sign <= upper;
            }
         } // in

         //! sign is one of the signs
         template< auto... signs>
         bool some( const auto sign)
         {
            return ( ( sign == signs) || ...);
         }

         //! sign is none of the signs
         template< auto... signs>
         bool none( const auto sign)
         {
            return ( ( sign != signs) && ...);
         }

         bool white( const auto sign)
         {
            return is::some< ' ', '\n', '\r', '\t'>( sign);
         }         

         bool inert( const auto sign)
         {
            return in::range< '\0', ' '>( sign);
         }


         //! @{ somehow as the standard
         bool space( const auto sign)
         {
            return sign == ' ' || in::range< '\t', '\r'>( sign);
         }

         bool digit( const auto sign)
         {
            return in::range< '0', '9'>( sign);
         }

         bool lower( const auto sign)
         {
            return in::range< 'a', 'z'>( sign);
         }

         bool upper( const auto sign)
         {
            return in::range< 'A', 'Z'>( sign);
         }

         bool alpha( const auto sign)
         {
            return lower( sign) || upper( sign);
         }

         bool alnum( const auto sign)
         {
            return alpha( sign) || digit( sign);
         }

         bool xdigit( const auto sign)
         {
            return digit( sign) || in::range< 'a', 'f'>( sign) || in::range< 'A', 'F'>( sign);
         }

         bool cntrl( const auto sign)
         {
            return in::range< 0x0, 0x1F>( sign) || sign == 0x7F;
         }
         //! @}
      } // is

      namespace to
      {
         auto lower( const auto sign)
         {
            return is::upper( sign) ? sign + ( 'a' - 'A') : sign;
         }

         auto upper( const auto sign)
         {
            return is::lower( sign) ? sign - ( 'a' - 'A') : sign;
         }
      } // to
      
      namespace till
      {
         constexpr auto quot = []( const auto sign) { return is::none< '"', '\\'>( sign); };
         constexpr auto apos = []( const auto sign) { return is::none< '\''>( sign); };
         constexpr auto line = []( const auto sign) { return is::none< '\n'>( sign); };         
      } // till

      auto trim( auto data)
      {
         auto white = [] ( const auto sign) { return is::white( sign); };
         data.erase( std::ranges::find_if_not( data | std::views::reverse, white).base(), data.end());
         data.erase( data.begin(), std::ranges::find_if_not( data, white));
         return data;
      }

      namespace transform
      {
         inline auto simple( std::string_view data) -> std::optional< node>
         {
            auto compare = [&data] ( std::string_view what)
            {
               return std::ranges::equal( data, what, [] ( const auto lhs, const auto rhs) { return to::lower( lhs) == rhs; });
            };

            if( compare( "null"))
               return nullptr;
            if( compare( "true"))
               return true;
            if( compare( "false"))
               return false;
            
            return {};
         }

         inline auto number( std::string_view data) -> std::optional< node>
         {
            const auto positive = data.starts_with( '+');
            const auto negative = data.starts_with( '-');
            
            auto start = data.data() + positive + negative;
            const auto cease = data.data() + data.size();

            // integer
            {
               const auto base = [&]
               {
                  if( std::distance( start, cease) > 2 && *start == '0')
                  {
                     switch( *(start + 1))
                     {
                     case 'b': std::advance( start, 2); return 2;
                     case 'o': std::advance( start, 2); return 8;
                     case 'x': std::advance( start, 2); return 16;
                     }
                  }
                  return 10;
               }();

               std::make_unsigned_t< node::integer> value;
               if( const auto [ptr, ec] = std::from_chars( start, cease, value, base); ptr == cease && ec == std::errc{})
               {
                  constexpr auto max = static_cast< std::make_unsigned_t< node::integer>>( std::numeric_limits< node::integer>::max());
                  if( negative)
                  {
                     if( value <= max + 1u)
                        return static_cast< node::integer>( 0u - value);
                  }
                  else
                  {
                     if( value <= max + 0u)
                        return static_cast< node::integer>( 0u + value);
                  }
                  return {};
               }
            }

            // decimal
            {
               node::decimal value;
               const auto result = std::from_chars( start, cease, value);
               if( result.ec == std::errc{} && result.ptr == cease)
                  return negative ? - value : + value;
            }

            return {};
         }

         inline auto instant( std::string_view data) -> std::optional< node::instant>
         {
            auto parse = [&data] < typename unit>( const auto& format) -> std::optional< unit>
            {
               // parse with nanosecond precision and truncate to the precision of type
               decltype( std::chrono::floor< std::chrono::nanoseconds>( unit{})) result{};
               std::ispanstream stream{ data};
               if( stream >> std::chrono::parse( format, result) && stream.peek() == std::ispanstream::traits_type::eof())
                  return std::chrono::floor< decltype( unit{} - unit{})>( result);

               return {};
            };

            if( data.ends_with( 'Z'))
               if( auto result = parse.template operator()< std::chrono::system_clock::time_point>( "%FT%TZ"))
                  return *result;

            if( auto result = parse.template operator()< node::local_date>( "%F"))
               return *result;

            if( auto result = parse.template operator()< node::local_time::precision>( "%T"))
               return node::local_time( *result);

            if( auto result = parse.template operator()< node::local_datetime>( "%FT%T"))
               return *result;

            if( auto result = parse.template operator()< std::chrono::system_clock::time_point>( "%FT%T%Ez"))
               return *result;

            return {};
         }

         inline auto binary( std::string_view data) -> std::optional< node::binary>
         {
            node::binary result;
            result.reserve( data.size() * 3 / 4);

            type::u32 pack{};
            int bits{};

            for( const auto sign : data)
            {
               if( ! is::white( sign) && sign != '=')
               {
                  const auto spot = [sign]
                  {
                     if( is::upper( sign)) return sign - 'A';
                     if( is::lower( sign)) return sign - 'a' + 26;
                     if( is::digit( sign)) return sign - '0' + 52;
                     if( sign == '+') return 62;
                     if( sign == '/') return 63;
                     return -1;
                  }();

                  if( spot < 0)
                     return {};

                  pack = ( pack << 6) | spot;
                  bits += 6;

                  if( bits >= 8)
                  {
                     bits -= 8;
                     result.push_back( static_cast< std::byte>( (pack >> bits) & 0xFF));
                  }
               }
            }

            return result;
         }            

         auto point( const std::same_as< type::i32> auto rune)
         {
            std::string nrv;

            using unit = std::string::value_type;

            if( rune < 0x80)
            {
               nrv.push_back( static_cast< unit>( rune));
            }
            else if( rune < 0x800)
            {
               nrv.push_back( static_cast< unit>( 0xC0 | (( rune >> 6) & 0x1F)));
               nrv.push_back( static_cast< unit>( 0x80 | (( rune & 0x3F))));
            }
            else if( rune < 0x10000)
            {
               nrv.push_back( static_cast< unit>( 0xE0 | (( rune >> 12) & 0x0F)));
               nrv.push_back( static_cast< unit>( 0x80 | (( rune >> 6) & 0x3F)));
               nrv.push_back( static_cast< unit>( 0x80 | (( rune & 0x3F))));
            }
            else
            {
               nrv.push_back( static_cast< unit>( 0xF0 | (( rune >> 18) & 0x07)));
               nrv.push_back( static_cast< unit>( 0x80 | (( rune >> 12) & 0x3F)));
               nrv.push_back( static_cast< unit>( 0x80 | (( rune >> 6) & 0x3F)));
               nrv.push_back( static_cast< unit>( 0x80 | (( rune & 0x3F))));
            }

            return nrv;
         }

      } // transform


      using pipe::byte;


      template< byte unit, pipe::source origin>
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

      template< byte unit, pipe::source origin>
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
            leap( till::line);
            if( good()) ++mark; // '\n'
         }

         //! skip whitespace
         void skip()
         {
            leap( [] ( const auto sign) { return is::white( sign);});
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
         auto cast( const byte auto sign) -> type::i32
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


      template< byte unit, pipe::target output>
      struct target
      {
         output sink;

         explicit target( auto& origin) : sink{ origin} {}

         void push( const byte auto data)
         {
            sink.push( data);
         }

         void emit( const std::ranges::contiguous_range auto& data)
         {
            sink.copy( data);
         }

         [[noreturn]] static void halt( const auto& what)
         {
            throw std::invalid_argument{ std::format( "{} is not valid", what)};
         }
      };


      template< byte unit, pipe::target output>
      struct writer : target< unit, output>
      {
         using base = target< unit, output>;
         using base::base;
         using base::push;
         using base::emit;

         void copy( const std::string_view data)
         {
            emit( data);
         }

         void fill( std::size_t size)
         {
            static constexpr std::string_view hole{ "                                "};
            while( size > hole.size()) emit( hole), size -= hole.size();
            emit( hole.substr( 0, size));
         }

         void fold( const std::size_t size)
         {
            push( '\n'); fill( size);
         }

         void cast( const std::string_view data)
         {
            auto stop = std::ranges::find_if( data, [] ( const auto sign) { return is::some< '\\', '\"'>( sign) || is::cntrl( sign); });

            if( stop == std::end( data)) [[likely]]
               return emit( data);

            emit( std::string_view{ std::begin( data), stop});

            push( '\\');

            if( const auto sign = *stop++; is::cntrl( sign))
               if( is::some< '\b', '\t', '\n', '\f', '\r'>( sign))
                  push( "btnvfr"[ sign - '\b']);
               else
                  emit( std::format( "u{:04x}", static_cast< unsigned char>( sign)));
            else
               push( sign);

            cast( std::string_view{ stop, std::end( data)});
         }

         void time( const node::local_time data)
         {
            const auto duration = data.to_duration();

            auto sink = [&] ( const auto floored)
            {
               if( duration != floored) return false;
               emit( std::format( "{:%T}", floored));
               return true;
            };

            sink( std::chrono::floor< std::chrono::seconds>( duration)) ||
            sink( std::chrono::floor< std::chrono::milliseconds>( duration)) ||
            sink( std::chrono::floor< std::chrono::microseconds>( duration)) ||
            sink( duration);
         }

         void time( const node::local_date data)
         {
            emit( std::format( "{:%F}", data));
         }

         void time( const node::local_datetime data)
         {
            time( std::chrono::floor< std::chrono::days>( data));
            push( 'T');
            time( std::chrono::hh_mm_ss{ data - std::chrono::floor< std::chrono::days>( data)});
         }

         void time( const node::zoned_datetime data)
         {
            time( data.get_local_time());
            emit( std::format( "{:%Ez}", data));
         }

         template< std::size_t size = 76>
         auto wrap( const auto& blob, const std::size_t dent = 0)
         {
            constexpr std::string_view alphabet{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};

            auto chunks = blob | std::views::chunk( 3);

            std::size_t list = size - 1;

            auto feed = [&]( const char sign)
            {
               if constexpr( size)
                  if( ++list == size) { fold( dent); list = 0; }

               push( sign);
            };

            for( auto chunk : chunks)
            {
               if( chunk.size() == 3)
               {
                  const auto b1 = std::to_integer< type::u32>( chunk[0]);
                  const auto b2 = std::to_integer< type::u32>( chunk[1]);
                  const auto b3 = std::to_integer< type::u32>( chunk[2]);

                  const type::u32 triple = ( b1 << 16) | ( b2 << 8) | b3;

                  feed( alphabet[ (triple >> 18) & 0x3F]);
                  feed( alphabet[ (triple >> 12) & 0x3F]);
                  feed( alphabet[ (triple >> 6) & 0x3F]);
                  feed( alphabet[ triple & 0x3F]);
               }
               else
               {
                  type::u32 triple = std::to_integer< type::u32>( chunk[0]) << 16;
                  if( chunk.size() == 2) triple |= std::to_integer< type::u32>( chunk[1]) << 8;

                  feed( alphabet[ (triple >> 18) & 0x3F]);
                  feed( alphabet[ (triple >> 12) & 0x3F]);
                  feed( ( chunk.size() == 2) ? alphabet[ (triple >> 6) & 0x3F] : '=');
                  feed( '=');
               }
            }

            if constexpr( size)
               push( '\n');
         }

         auto flat( const auto& blob)
         {
            wrap< 0>( blob);
         }
      };


      namespace make
      {
         template< template< typename, typename, auto...> class parser, auto... flags>
         auto source( const std::ranges::contiguous_range auto& value)
         {
            using unit = std::ranges::range_value_t< decltype( value)>;
            return parser< unit, pipe::parse::memory< unit>, flags...>( value);
         }

         template< template< typename, typename, auto...> class parser, auto... flags, typename unit>
         auto source( std::basic_istream< unit>& value)
         {
            return parser< unit, pipe::parse::stream< unit>, flags...>( value);
         }

         template< template< typename, typename, auto...> class parser, auto... flags>
         auto source( const std::ranges::input_range auto& value)
            requires ( ! std::ranges::contiguous_range< decltype( value)>)
         {
            using range = std::remove_cvref_t< decltype( value)>;
            using unit  = std::ranges::range_value_t< range>;
            using cursor = pipe::parse::cursor< unit, std::ranges::iterator_t< const range>, std::ranges::sentinel_t< const range>>;
            return parser< unit, cursor, flags...>( value);
         }

         // utility for string literals (automatically deduces size and type) (essentially for testing)
         template< template< typename, typename, auto...> class parser, auto... flags, std::size_t size>
         auto source( const char (&string)[ size])
         {
            return source< parser, flags...>( std::string_view{ string, size - 1});
         }

         template< template< typename, typename, auto...> class writer, auto... flags, typename unit>
         auto target( std::basic_ostream< unit>& value)
         {
            return writer< unit, pipe::write::stream< unit>, flags...>( value);
         }

         template< template< typename, typename, auto...> class writer, auto... flags>
         auto target( std::ranges::range auto& value)
         {
            using into = std::remove_cvref_t< decltype( value)>;
            return writer< std::ranges::range_value_t< into>, pipe::write::memory< std::ranges::range_value_t< into>, into>, flags...>( value);
         }
      } // make
   } // help
} // poly_version
