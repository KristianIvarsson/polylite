//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#define version v1_4_2

#include <array>
#include <format>
#include <ranges>
#include <string>
#include <istream>
#include <iterator>
#include <algorithm>
#include <stdexcept>

namespace poly::help 
{
   inline namespace version
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
            return in::range<'\n', '\r'>( sign) || sign == ' ';
         }

         inline bool digit( const auto sign)
         {
            return in::range<'0', '9'>( sign);
         }

         inline bool lower( const auto sign)
         {
            return in::range<'a', 'z'>( sign);
         }

         inline bool upper( const auto sign)
         {
            return in::range<'A', 'Z'>( sign);
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
            return digit( sign) || in::range<'a', 'f'>( sign) || in::range<'A', 'F'>( sign);
         }

         inline bool cntrl( const auto sign)
         {
            return in::range<0x0, 0x1F>( sign) || sign == 0x7F;
         }
      } // is

      namespace stream
      {
         namespace ignore
         {
            //! ignores possible UTF8-BOM
            inline std::istream& bom( std::istream& stream)
            {
               std::array< char, 3> data{};

               const auto count  = stream.read( data.data(), data.size()).gcount();

               if( ! std::ranges::equal( data, std::string_view{ "\xEF\xBB\xBF"}))
                  stream.clear(), stream.seekg( 0 - count, std::ios::cur);
               
               return stream;
            }
         } // ignore
         
         namespace buffer::iterator
         {
            struct parser
            {
               std::istreambuf_iterator< std::istream::char_type> mark;

               parser( std::istream& stream) : mark{ stream} {}

               bool good() const
               {
                  return mark != decltype( mark){};
               }

               auto read( auto&& till)
               {
                  std::string nrv;
                  while( good() && till( *mark)) nrv.push_back( *mark++);
                  return nrv;
               }

               auto leap( auto&& till)
               {
                  mark = 
                     std::ranges::begin(std::ranges::subrange( mark, decltype( mark){}) | 
                     std::views::drop_while( till));
               }

               char pull()
               {
                  if( good()) [[likely]] return *mark++;
                  [[unlikely]] halt( "unexpected end of stream");
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

                  for( auto& sign : data) sign = pull();

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
                  throw std::runtime_error{ std::format( "{} with just {} bytes left to parse", message, std::distance( mark, decltype( mark){}))};
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
   } // version
} // poly::help
