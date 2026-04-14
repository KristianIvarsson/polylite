//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "tale.hpp"

#include "node.hpp"

#include <array>
#include <format>
#include <ranges>
#include <string>
#include <istream>
#include <variant>
#include <iterator>
#include <optional>
#include <algorithm>
#include <stdexcept>

namespace poly
{
   inline namespace version
   {
      namespace help
      {

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

            inline bool space( const auto sign)
            {
               return in::range< '\n', '\r'>( sign) || sign == ' ';
            }

            inline bool digit( const auto sign)
            {
               return in::range< '0', '9'>( sign);
            }

            inline bool lower( const auto sign)
            {
               return in::range< 'a', 'z'>( sign);
            }

            inline bool upper( const auto sign)
            {
               return in::range< 'A', 'Z'>( sign);
            }

            inline bool alpha( const auto sign)
            {
               return lower( sign) || upper( sign);
            }

            inline bool alnum( const auto sign)
            {
               return alpha( sign) || digit( sign);
            }

            inline bool xdigit( const auto sign)
            {
               return digit( sign) || in::range< 'a', 'f'>( sign) || in::range< 'A', 'F'>( sign);
            }

            inline bool cntrl( const auto sign)
            {
               return in::range< 0x0, 0x1F>( sign) || sign == 0x7F;
            }
         } // is

         namespace to
         {
            inline auto lower( const auto sign)
            {
               return is::upper( sign) ? sign + ( 'a' - 'A') : sign;
            }

            inline auto upper( const auto sign)
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
            auto number( const auto& data) -> std::optional< node>
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

                  node::integer value;
                  const auto result = std::from_chars( start, cease, value, base);
                  if( result.ec == std::errc{} && result.ptr == cease)
                     return negative ? -value : value;
               }

               // decimal
               {
                  node::decimal value;
                  const auto result = std::from_chars( start, cease, value);
                  if( result.ec == std::errc{} && result.ptr == cease)
                     return negative ? -value : value;
               }

               return {};
            }

            auto simple( const auto& data) -> std::optional< node>
            {
               auto compare = [&data] ( const auto& what)
               {
                  return std::ranges::equal( data, what, [] ( const auto lhs, const auto rhs) { return to::lower( lhs) == rhs; });
               };

               if( compare( std::string_view{ "null"}))
                  return nullptr;
               if( compare( std::string_view{ "true"}))
                  return true;
               if( compare( std::string_view{ "false"}))
                  return false;
               
               return {};
            }

         } // transform

         namespace stream
         {
           
            namespace buffer::iterator
            {
               struct parser
               {
                  static constexpr const auto last = std::istreambuf_iterator< std::istream::char_type>{};

                  std::istreambuf_iterator< std::istream::char_type> mark;

                  parser( std::istream& stream) : mark{ stream} {}

                  bool good() const
                  {
                     return mark != last;
                  }

                  auto read( auto&& till)
                  {
#if defined(_MSC_VER) // https://github.com/microsoft/STL/issues/5066
                     std::string nrv;
                     while(good() && till(*mark)) nrv.push_back(*mark++);
                     return nrv;
#else
                     return std::ranges::subrange( mark, decltype( mark){}) |
                        std::views::take_while( till) |
                        std::ranges::to< std::string>();
#endif
                  }

                  auto leap( auto&& till)
                  {
                     mark =
                        std::ranges::begin(
                           std::ranges::subrange( mark, last) | 
                           std::views::drop_while( till));
                  }

                  char pull()
                  {
                     if( good()) [[likely]] return *mark++;
                     halt( "unexpected end of stream");
                  }

                  char peek() const
                  {
                     if( good()) [[likely]] return *mark;
                     return std::char_traits< std::istream::char_type>::eof();
                  }

                  template< std::size_t size>
                  auto unit()
                  {
                     std::array< char, size> data;

#if defined(_MSC_VER) // https://github.com/microsoft/STL/issues/5066
                     for( auto& sign : data) sign = pull();
#else
                     std::copy_n(mark, data.size(), data.data());
#endif

                     std::int32_t code;
                     const auto result = std::from_chars( data.data(), data.data() + data.size(), code, 16);

                     if( result.ec != std::errc{} || result.ptr != (data.data() + data.size()))
                        [[unlikely]] halt( "invalid code point");

                     return code;
                  }

                  // c-style escape sequences
                  auto code()
                  {
                     const auto lead = unit< 4>();

                     if( lead < 0xD800 || lead > 0xDFFF)
                        return lead;

                     if( lead > 0xDBFF)
                        [[unlikely]] halt( "invalid 1st surrogate");

                     test( '\\', pull()); test( 'u', pull());

                     const auto tail = unit< 4>();

                     if( tail < 0xDC00 || tail > 0xDFFF)
                        [[unlikely]] halt( "invalid 2nd surrogate");

                     return 0x10000 + ( ( lead - 0xD800) << 10) + ( tail - 0xDC00);
                  }

                  // c-style escape sequences
                  template< bool U>
                  auto cast() -> std::int32_t
                  {
                     switch( pull())
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
                     case 'U': if constexpr( U) return unit< 8>(); else [[fallthrough]];
                     default: [[unlikely]] halt( "invalid escape character");
                     }
                  }

                  template< bool U>
                  auto cast( std::string& data)
                  {
                     using type = std::string::value_type;
                     if( const auto cp = cast< U>(); cp < 0x80)
                     {
                        data.push_back( static_cast< type>( cp));
                     }
                     else if( cp < 0x800)
                     {
                        data.push_back( static_cast< type>( 0xC0 | (( cp >> 6) & 0x1F)));
                        data.push_back( static_cast< type>( 0x80 | (( cp & 0x3F))));
                     }
                     else if( cp < 0x10000)
                     {
                        data.push_back( static_cast< type>( 0xE0 | (( cp >> 12) & 0x0F)));
                        data.push_back( static_cast< type>( 0x80 | (( cp >> 6) & 0x3F)));
                        data.push_back( static_cast< type>( 0x80 | (( cp & 0x3F))));
                     }
                     else
                     {
                        data.push_back( static_cast< type>( 0xF0 | (( cp >> 18) & 0x07)));
                        data.push_back( static_cast< type>( 0x80 | (( cp >> 12) & 0x3F)));
                        data.push_back( static_cast< type>( 0x80 | (( cp >> 6) & 0x3F)));
                        data.push_back( static_cast< type>( 0x80 | (( cp & 0x3F))));
                     }
                  }

                  void test( const char want, const char pick) const
                  {
                     if( want != pick) [[unlikely]] halt( "unexpected character");
                  }

                  [[noreturn]] void halt( const std::string_view message) const
                  {
                     throw std::runtime_error{ std::format( "{} with just {} bytes left to parse", message, std::distance( mark, last))};
                  }
               };

               struct writer
               {
                  std::ostreambuf_iterator< std::istream::char_type> mark;

                  writer( std::ostream& stream) : mark{ stream} {}

                  void push( const auto sign)
                  {
                     *mark++ = sign;
                  }

                  void copy( const std::string_view data)
                  {
                     std::ranges::copy( data, mark);
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
               };

            } // buffer::iterator
         } // stream
      } // help
   } // version
} // poly
