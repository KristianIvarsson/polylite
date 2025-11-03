//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "node.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <format>
#include <iomanip>
#include <spanstream>
#include <sstream>

#include <stdexcept>


namespace poly
{

   inline namespace v1_2_0
   {
      namespace json
      {

         namespace detail
         {
            struct parser
            {
               parser( std::istream& stream) : mark{ stream} {}

               auto operator()()
               {
                  auto nrv = detect();
                  test( std::char_traits< std::istream::char_type>::eof(), peep());
                  return nrv;
               }

            private:

               auto detect() -> node
               {
                  switch( peep())
                  {
                  case '{':
                     return object();
                  case '[':
                     return array();
                  case '"':
                     return string();
                  case 'n': case 't': case 'f':
                     return simple();
                  default:
                     return number();
                  }
               }

               void test( const char want, const char pick) const
               {
                  if( want != pick) [[unlikely]] error( "unexpected character");
               }

               bool good() const
               {
                  return mark != decltype( mark){};
               }

               auto read( auto&& want)
               {
                  std::string nrv;
                  while( good() && want( *mark))
                     nrv.push_back( *mark++);
                  return nrv;
               }

               char pull()
               {
                  if( good()) [[likely]]
                     return *mark++;
                  [[unlikely]] error( "unexpected end of stream");
               }

               char pick()
               {
                  while( good() && std::isspace( *mark))
                     ++mark;
                  return pull();
               }

               char peek()
               {
                  if( good()) [[likely]]
                     return *mark;
                  return std::char_traits< std::istream::char_type>::eof();
               }

               char peep()
               {
                  while( good() && std::isspace( *mark))
                     ++mark;
                  return peek();
               }

               auto unit()
               {
                  std::array< char, 4> data;
                  std::copy_n( mark, data.size(), data.data());

                  std::int32_t code;
                  const auto result = std::from_chars( data.data(), data.data() + data.size(), code, 16);

                  if( result.ec != std::errc{} || result.ptr != (data.data() + data.size()))
                     [[unlikely]] error( "invalid code point");

                  return code;
               }

               auto code()
               {
                  const auto lead = unit();

                  if( lead < 0xD800 || lead > 0xDFFF)
                     return lead;

                  if( lead > 0xDBFF)
                     [[unlikely]] error( "invalid 1st surrogate");

                  test( '\\', pull()); test( 'u', pull());

                  const auto tail = unit();

                  if( tail < 0xDC00 || tail > 0xDFFF)
                     [[unlikely]] error( "invalid 2nd surrogate");

                  return 0x10000 + ( ( lead - 0xD800) << 10) + ( tail - 0xDC00);
               }

               auto decode() -> std::int32_t
               {
                  switch( pull())
                  {
                  break; case '\\':return '\\';
                  break; case '"': return '\"';
                  break; case 'b': return '\b';
                  break; case 'f': return '\f';
                  break; case 'n': return '\n';
                  break; case 'r': return '\r';
                  break; case 't': return '\t';
                  break; case '/': return '/';
                  break; case 'u': return code();
                  break; default: [[unlikely]] error( "invalid escape character");
                  }
               }

               auto object() -> node::object
               {
                  ++mark; // '{'

                  node::object nrv;

                  if( peep() != '}')
                  {
                     while( true)
                     {
                        test( '"', peep());

                        auto name = string();

                        test( ':', pick());

                        nrv.emplace( std::move( name), detect());

                        if( const auto sign = pick(); sign != ',')
                        {
                           test( '}', sign);
                           break;
                        }
                     }
                  }
                  else
                  {
                     ++mark; // '}'
                  }

                  return nrv;
               }

               auto array() -> node::array
               {
                  ++mark; // '['

                  node::array nrv;

                  if( peep() != ']')
                  {
                     while( true)
                     {
                        nrv.emplace_back( detect());

                        if( const auto sign = pick(); sign != ',')
                        {
                           test( ']', sign);
                           break;
                        }
                     }
                  }
                  else
                  {
                     ++mark; // ']'
                  }

                  return nrv;
               }
               
               auto string() -> node::string
               {
                  ++mark; // '"'

                  std::string nrv;

                  while( true)
                  {
                     const auto sign = pull();

                     if( sign == '"')
                        return nrv;

                     if( sign != '\\') [[likely]]
                     {
                        nrv.push_back( sign);
                     }
                     else
                     {
                        using type = std::string::value_type;

                        if( const auto cp = decode(); cp < 0x80)
                           nrv.insert( nrv.end(), { static_cast< type>( cp)});
                        else if( cp < 0x800)
                           nrv.insert( nrv.end(), { static_cast< type>( 0xC0 | (( cp >> 6) & 0x1F)), static_cast< type>( 0x80 | ( cp & 0x3F))});
                        else if( cp < 0x10000)
                           nrv.insert( nrv.end(), { static_cast< type>( 0xE0 | (( cp >> 12) & 0x0F)), static_cast< type>( 0x80 | (( cp >> 6) & 0x3F)), static_cast< type>( 0x80 | ( cp & 0x3F))});
                        else
                           nrv.insert( nrv.end(), { static_cast< type>( 0xF0 | (( cp >> 18) & 0x07)), static_cast< type>( 0x80 | (( cp >> 12) & 0x3F)), static_cast< type>( 0x80 | (( cp >> 6) & 0x3F)), static_cast< type>( 0x80 | ( cp & 0x3F))});
                     }
                  }
               }

               auto simple() -> node
               {
                  ++mark; // 'n', 't', 'f

                  const auto data = read( []( const auto sign) 
                     { 
                        return std::islower( sign); 
                     });

                  if( data == "ull")
                     return nullptr;

                  if( data == "rue")
                     return true;

                  if( data == "alse")
                     return false;

                  [[unlikely]] error( "unexpected data");
               }

               auto number() -> node
               {
                  const auto data = read( []( const auto sign)
                     {
                        // strict parsing
                        switch( sign)
                        case '-': case '.': case 'e': case 'E': return true;
                        return std::isdigit( sign) != 0;
                        // casual parsing (for NaN, Inf, etc)
                        //case '.': case '-': case '+': return true;
                        //return std::isalnum( sign) != 0;
                     });

                  {
                     node::integer value;
                     const auto result = std::from_chars( data.data(), data.data() + data.size(), value);
                     if( result.ec == std::errc{} && result.ptr == ( data.data() + data.size()))
                        return value;
                  }

                  {
                     node::decimal value;
                     const auto result = std::from_chars( data.data(), data.data() + data.size(), value);
                     if( result.ec == std::errc{} && result.ptr == ( data.data() + data.size()))
                        return value;
                  }

                  [[unlikely]] error( "unexpected data");
               }

               [[noreturn]] void error( const std::string_view message) const
               {
                  throw std::runtime_error{ std::format( "{} with just {} bytes left to parse", message, std::distance( mark, decltype( mark){}))};
               }

            private:

               std::istreambuf_iterator< std::istream::char_type> mark;

            };

         } // detail

         auto parse( std::istream& stream)
         {
            return detail::parser{ stream}();
         }

         auto parse( std::string_view json)
         {
            std::ispanstream stream{ json};
            return parse( stream);
         }

         namespace bom
         {
            //! ignores possible UTF8-BOM
            auto parse( std::istream& stream)
            {
               std::array< char, 3> data;

               const auto count  = stream.read( data.data(), data.size()).gcount();

               if( ! std::ranges::equal( data, std::string_view{ "\xEF\xBB\xBF"}))
                  stream.clear(), stream.seekg( 0 - count, std::ios::cur);
               
               return detail::parser{ stream}();
            }

            auto parse( std::string_view json)
            {
               std::ispanstream stream{ json};
               return parse( stream);
            }
         } // bom

         namespace detail
         {
            constexpr std::size_t spaces = 3;

            template< std::size_t spaces>
            struct writer
            {
               std::ostream& stream;

               writer( std::ostream& stream) : stream{ stream} {}

               void operator() ( const node::object& node)
               {
                  start( '{');

                  auto comma = node.size();
                  for( const auto& [ name, data] : node)
                  {
                     insert( name);
                     if constexpr( spaces) indent = false;
                     std::visit( *this, data);
                     if constexpr( spaces) indent = true;
                     if( --comma) stream << ',';
                  }

                  close( '}');
               }

               void operator() ( const node::array& node)
               {
                  start( '[');

                  auto comma = node.size();
                  for( const auto& data : node)
                  {
                     std::visit( *this, data);
                     if( --comma) stream << ',';
                  }

                  close( ']');
               }

               void operator() ( const node::nothing& node)
               {
                  insert();
                  stream << "null";
               }

               void operator() ( const node::boolean& node)
               {
                  insert();
                  stream << ( node ? "true" : "false");
               }

               void operator() ( const node::integer& node)
               {
                  insert();
                  stream << node;
               }

               void operator() ( const node::decimal& node)
               {
                  if( std::isnan( node) || std::isinf( node))
                     [[unlikely]] throw std::invalid_argument{ std::format( "invalid decimal node [{}]", node)};

                  insert();
                  stream << node;
               }

               void operator() ( const node::string& node)
               {
                  insert();
                  stream << '"';

                  for( const auto data : node)
                  {
                     if( std::iscntrl( data))
                     {
                        stream << R"(\u)" << std::format( "{:04x}", data);
                     }
                     else [[likely]]
                     {
                        switch( data)
                        {
                        case '\\':
                        case '\"':
                           stream << "\\";
                        }

                        stream << data;
                     }
                  }

                  stream << '"';
               }

            private:

               void insert()
               {
                  if constexpr( spaces)
                     if( indent)
                        stream << '\n' << std::setw( column * spaces) << "";
                     else
                        indent = true;
               }

               void insert( const std::string::value_type sign)
               {
                  insert();
                  stream << sign;
               }

               void insert( const std::string& name)
               {
                  insert();
                  //stream << std::quoted( name) << ':';
                  stream << '"' << name << '"' << ':';
                  
                  if constexpr( spaces) stream << ' ';
               }

               void start( const auto sign)
               {
                  insert( sign);
                  if constexpr( spaces) ++column;
               }

               void close( const auto sign)
               {
                  if constexpr( spaces) --column;
                  insert( sign);
               }

            private:

               char column{};
               bool indent{};

            };

         } // detail


         inline namespace elegant
         {
            template< std::size_t spaces = detail::spaces>
            auto write( const node& node, std::ostream& stream)
            {
               std::visit( detail::writer< spaces>{ stream}, node);
            }

            template< std::size_t spaces = detail::spaces>
            auto write( const node& node)
            {
               std::ostringstream stream;
               write< spaces>( node, stream);
               return std::move( stream).str();
            }
         } // elegant

         namespace compact
         {
            auto write( const node& node, std::ostream& stream)
            {
               std::visit( detail::writer< 0>{ stream}, node);
            }

            auto write( const node& node)
            {
               std::ostringstream stream;
               write( node, stream);
               return std::move( stream).str();
            }
         } // compact

      } // json

   } // v1_2_0

} // poly
