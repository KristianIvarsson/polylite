//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#define version v1_3_0

#include <array>
#include <format>
#include <ranges>
#include <string>
#include <istream>
#include <iterator>
#include <stdexcept>

namespace poly::help 
{
   inline namespace version
   {
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
         } // bom
         
         namespace buffer
         {
            namespace iterator
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
                     return 
                        std::ranges::subrange( mark, decltype( mark){}) | 
                        std::views::take_while( till) | 
                        std::ranges::to< std::string>();
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
                     std::copy_n( mark, data.size(), data.data());

                     std::int32_t code;
                     const auto result = std::from_chars( data.data(), data.data() + data.size(), code, 16);

                     if( result.ec != std::errc{} || result.ptr != (data.data() + data.size()))
                        [[unlikely]] halt( "invalid code point");

                     return code;
                  }

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

                  auto code( const auto sign) -> std::int32_t
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
               };

            } // iterator
         } // buffer
      } // stream
   } // version
} // poly::help
