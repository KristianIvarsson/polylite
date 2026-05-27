//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "tale.hpp"

#include "node.hpp"

#include <array>
#include <chrono>
#include <format>
#include <ranges>
#include <string>
#include <istream>
#include <ostream>
#include <charconv>
#include <iterator>
#include <optional>
#include <algorithm>
#include <stdexcept>
#include <spanstream>
#include <string_view>

namespace poly_version
{
   namespace help
   {

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

         bool space( const auto sign)
         {
            return in::range< '\t', '\r'>( sign) || sign == ' ';
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

      auto trim( auto data)
      {
         auto white = [] ( const auto sign) { return is::space( sign); };
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
               const auto result = std::from_chars( start, cease, value, base);
               if( result.ec == std::errc{} && result.ptr == cease)
               {
                  constexpr auto max = static_cast< std::make_unsigned_t< node::integer>>( std::numeric_limits< node::integer>::max());
                  if( negative)
                  {
                     if( value <= max + 1u)
                        return static_cast< node::integer>( - value);
                  }
                  else
                  {
                     if( value <= max + 0u)
                        return static_cast< node::integer>( + value);
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
            auto parse = [&data] < typename type>( const auto& format) -> std::optional< type>
            {
               type result{};
               std::ispanstream stream{ data};
               if( stream >> std::chrono::parse( format, result) && stream.peek() == std::char_traits< char>::eof())
                  return result;

               return {};
            };

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

            std::uint32_t pack{};
            int bits{};

            for( const auto sign : data)
            {
               if( ! is::space( sign) && sign != '=')
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

         auto point( const std::same_as< std::int32_t> auto code)
         {
            std::string nrv;

            using type = std::string::value_type;

            if( code < 0x80)
            {
               nrv.push_back( static_cast< type>( code));
            }
            else if( code < 0x800)
            {
               nrv.push_back( static_cast< type>( 0xC0 | (( code >> 6) & 0x1F)));
               nrv.push_back( static_cast< type>( 0x80 | (( code & 0x3F))));
            }
            else if( code < 0x10000)
            {
               nrv.push_back( static_cast< type>( 0xE0 | (( code >> 12) & 0x0F)));
               nrv.push_back( static_cast< type>( 0x80 | (( code >> 6) & 0x3F)));
               nrv.push_back( static_cast< type>( 0x80 | (( code & 0x3F))));
            }
            else
            {
               nrv.push_back( static_cast< type>( 0xF0 | (( code >> 18) & 0x07)));
               nrv.push_back( static_cast< type>( 0x80 | (( code >> 12) & 0x3F)));
               nrv.push_back( static_cast< type>( 0x80 | (( code >> 6) & 0x3F)));
               nrv.push_back( static_cast< type>( 0x80 | (( code & 0x3F))));
            }

            return nrv;
         }

      } // transform


      template< typename type>
      concept sign = sizeof( type) == 1;

      template< typename iterator, typename type>
      concept source_iterator = std::input_iterator< iterator> && std::same_as< std::iter_value_t< iterator>, type>;

      template< typename iterator, typename type>
      concept target_iterator = std::output_iterator< iterator, type>;


      template< sign type, source_iterator< type> iterator>
      struct source
      {
         iterator mark;
         iterator last;

         bool good() const
         {
            return mark != last;
         }

         void done() const
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

         char peek() const
         {
            if( good()) [[likely]] return look();
            return std::char_traits< char>::eof();
         }

         char pull()
         {
            if( good()) [[likely]] return take();
            halt( "unexpected end");
         }

         void test( const char want, const char pick) const
         {
            if( want != pick) [[unlikely]] halt( "unexpected sign");
         }

         [[noreturn]] void halt( const std::string_view message) const
         {
            throw std::runtime_error{ std::format( "{} with just {} bytes left to parse", message, std::distance( mark, last))};
         }

         [[noreturn]] void deny( const std::string_view message) const
         {
            halt( std::format( "unsupported {}", message));
         }
      };

      template< sign type, source_iterator< type> iterator>
      struct parser : source< type, iterator>
      {
         using base = source< type, iterator>;
         using base::mark;
         using base::good;
         using base::look;
         using base::take;
         using base::pull;
         using base::test;
         using base::halt;
         using base::deny;

         auto read( auto&& till)
         {
            std::string nrv;
            while( good() && till( look())) nrv.push_back( take());
            return nrv;
         }

         auto leap( auto&& till)
         {
            while( good() && till( look())) ++mark;
         }

         //! rest of the line
         void rest()
         {
            leap( [] ( const auto sign) { return sign != '\n';});
            if( good()) ++mark; // '\n'
         }

         //! skip whitespace
         void skip()
         {
            leap( [] ( const auto sign) { return is::space( sign);});
         }

         template< std::size_t size>
         auto unit()
         {
            std::array< char, size> data;
            for( auto& sign : data) sign = pull();

            std::int32_t code;
            const auto result = std::from_chars( data.data(), data.data() + data.size(), code, 16);

            if( result.ec != std::errc{} || result.ptr != (data.data() + data.size()))
               [[unlikely]] halt( "invalid code point");

            return code;
         }

         auto half()
         {
            return unit< 2>();
         }

         auto real()
         {
            return unit< 4>();
         }

         auto full()
         {
            return unit< 8>();
         }


         // c-style escape sequences
         auto code()
         {
            const auto lead = real();

            if( lead < 0xD800 || lead > 0xDFFF)
               return lead;

            if( lead > 0xDBFF)
               [[unlikely]] halt( "invalid 1st surrogate");

            test( '\\', pull()); test( 'u', pull());

            const auto tail = real();

            if( tail < 0xDC00 || tail > 0xDFFF)
               [[unlikely]] halt( "invalid 2nd surrogate");

            return 0x10000 + ( ( lead - 0xD800) << 10) + ( tail - 0xDC00);
         }

         // c-style escape sequences
         auto cast( const auto sign) -> std::int32_t
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


      template< sign type, target_iterator< type> iterator>
      struct target
      {
         iterator mark;

         void push( const auto sign)
         {
            *mark++ = static_cast< type>( sign);
         }

         void emit( const std::ranges::contiguous_range auto& data)
         {
            for( const auto sign : data) push( sign);
         }

         [[noreturn]] static void halt( const auto& what)
         {
            throw std::invalid_argument{ std::format( "{} is not valid", what)};
         }
      };


      template< sign type, target_iterator< type> iterator>
      struct writer : target< type, iterator>
      {
         using base = target< type, iterator>;
         using base::push;
         using base::emit;

         void copy( const std::string_view data)
         {
            emit( data);
         }

         void fill( std::size_t size)
         {
            while( size--) push( ' ');
         }

         void fold( std::size_t size)
         {
            push( '\n'); fill( size);
         }

         void cast( const auto sign)
         {
            copy( std::format( R"(\u{:04x})", static_cast< unsigned char>( sign)));
         }

         void cast( const std::string& data)
         {
            for( const auto sign : data)
            {
               if( is::cntrl( sign))
               {
                  switch( sign)
                  {
                  case '\b': copy( R"(\b)"); break;
                  case '\t': copy( R"(\t)"); break;
                  case '\n': copy( R"(\n)"); break;
                  case '\f': copy( R"(\f)"); break;
                  case '\r': copy( R"(\r)"); break;
                  default: cast( sign);
                  }
               }
               else [[likely]]
               {
                  switch( sign)
                  case '\\': case '\"': push( '\\');
                  push( sign);
               }
            }
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
                  const auto b1 = std::to_integer< std::uint32_t>( chunk[0]);
                  const auto b2 = std::to_integer< std::uint32_t>( chunk[1]);
                  const auto b3 = std::to_integer< std::uint32_t>( chunk[2]);

                  const uint32_t triple = ( b1 << 16) | ( b2 << 8) | b3;

                  feed( alphabet[ (triple >> 18) & 0x3F]);
                  feed( alphabet[ (triple >> 12) & 0x3F]);
                  feed( alphabet[ (triple >> 6) & 0x3F]);
                  feed( alphabet[ triple & 0x3F]);
               }
               else
               {
                  std::uint32_t triple = std::to_integer< std::uint32_t>( chunk[0]) << 16;
                  if( chunk.size() == 2) triple |= std::to_integer< std::uint32_t>( chunk[1]) << 8;

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
         auto source( std::istream& source)
         {
            using iterator = std::istreambuf_iterator< std::istream::char_type>;
            return parser< std::istream::char_type, iterator, flags...>{ iterator{ source}, iterator{}};
         }

         template< template< typename, typename, auto...> class parser, auto... flags>
         auto source( const auto& source)
            requires ( ! std::derived_from< std::remove_cvref_t< decltype( source)>, std::istream>)
         {
            using type     = std::ranges::range_value_t< decltype( source)>;
            using iterator = std::ranges::iterator_t< const std::remove_cvref_t< decltype( source)>>;
            return parser< type, iterator, flags...>{
               std::ranges::begin( source), std::ranges::end( source)};
         }

         // utility for string literals (automatically deduces size and type) (essentially for testing)
         template< template< typename, typename, auto...> class parser, auto... flags, std::size_t size>
         auto source( const char (&string)[ size])
         {
            return source< parser, flags...>( std::string_view{ string, size - 1});
         }

         template< template< typename, typename, auto...> class writer, auto... flags>
         auto target( std::ostream& target)
         {
            using iterator = std::ostreambuf_iterator< std::ostream::char_type>;
            return writer< std::ostream::char_type, iterator, flags...>{ iterator{ target}};
         }

         template< template< typename, typename, auto...> class writer, auto... flags>
         auto target( auto& target)
            requires ( ! std::derived_from< std::remove_cvref_t< decltype( target)>, std::ostream>)
         {
            using type = std::remove_cvref_t< decltype( target)>;
            return writer< typename type::value_type, std::back_insert_iterator< type>, flags...>{
               std::back_inserter( target)};
         }
      } // make
   } // help
} // poly_version
