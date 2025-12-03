//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "node.hpp"
#include "help.hpp"

#include <array>
#include <string>
#include <vector>
#include <algorithm>
#include <spanstream>


namespace poly
{
   inline namespace version
   {
      namespace toml
      {
         namespace detail
         {
            auto bare = [] ( const auto sign)
            {
               return std::isalnum( sign) || sign == '_' || sign == '-';
            };

            struct parser : help::stream::buffer::iterator::parser
            {
               using help::stream::buffer::iterator::parser::parser;

               auto operator()()
               {
                  node root = node::table{};

                  auto where = &root;

                  while( true)
                  {
                     auto keys = this->keys();

                     if( keys.empty())
                     {
                        test( std::char_traits< std::istream::char_type>::eof(), peep());
                        return root.as_table();
                     }
                     
                     if( const auto next = pick(); next == '=')
                     {
                        auto afore = where;

                        for( auto&& name : std::move( keys))
                           where = &(*where)[ std::move( name)];

                        *where = spot();

                        where = afore;
                     }
                     else
                     {
                        test(']', next);

                        where = &root;

                        for( auto&& name : std::move( keys))
                           where = &(*where)[ std::move( name)];

                        if( peek() == ']')
                        {
                           ++mark; // ']'

                           if( ! where->is_array())
                              *where = node::array{};
                           
                           where = &where->as_array().emplace_back( node::table{});
                        }
                        else
                        {
                           if( ! where->is_table())
                              *where = node::table{};
                        }
                     }
                  }
               }

            private:

               auto spot() -> node
               {
                  switch( peep())
                  {
                  case '{':
                     return table();
                  case '[':
                     return array();
                  case '"':
                     return basic();
                  case '\'':
                     return literal();
                  case 't': case 'f':
                     return simple();
                  default:
                     return number();
                  }
               }

               auto name()
               {
                  switch( peep())
                  {
                  case '"': 
                     return basic();
                  case '\'':
                     return literal();
                  default:
                     return read( bare);
                  }
               }

               auto keys() -> std::vector< node::table::key_type>
               {
                  if( peep() == '[') ++mark; // '['
                  if( peek() == '[') ++mark; // '['

                  std::vector< node::string> nrv;

                  while( good())
                  {
                     nrv.push_back( name());

                     if( peep() == '.')
                        ++mark; // '.'
                     else
                        break;
                  }

                  return nrv;
               }

               void skip() 
               {
                  while( good() && std::isspace( *mark)) 
                     ++mark;

                  if( good() && *mark == '#')
                  {
                     while( good() && *mark != '\n')
                        ++mark;
                     skip();
                  }
               }

               char pick()
               {
                  skip();
                  return pull();
               }

               char peep()
               {
                  skip();
                  return peek();
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

               auto decode( const auto sign) -> std::int32_t
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
                  case 'U': return unit< 8>();
                  default: [[unlikely]] halt( "invalid escape character");
                  }
               }
               
               auto table() -> node::table
               {
                  ++mark; // '{'

                  node nrv;

                  if( peep() != '}')
                  {
                     while( true)
                     {
                        auto where = &nrv;

                        for( auto&& name : keys())
                           where = &(*where)[ std::move( name)];

                        test( '=', pick());

                        *where = spot();

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

                  return std::move( nrv).as_table();
               }

               auto array() -> node::array
               {
                  ++mark; // '['

                  node::array nrv;

                  if( peep() != ']')
                  {
                     while( true)
                     {
                        nrv.emplace_back( spot());

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

               auto basic() -> node::string
               {
                  if( ++mark, peek() == '"')
                     if( ++mark, peek() != '"')
                        return {};

                  const auto same = ! ( peek() == '"' ? pull(), skip(), true : false);

                  std::string nrv;

                  while( true)
                  {
                     const auto sign = pull();

                     if( sign == '"')
                        if( same || peek() != '"' && [&nrv] { return nrv.ends_with( R"("")") ? nrv.erase( nrv.size() - 2), true : false; }())
                           return nrv;

                     if( sign != '\\') [[likely]]
                     {
                        nrv.push_back( sign);
                     }
                     else  
                     {
                        if( const auto sign = pull(); sign != '\n' && same) [[likely]]
                        {
                           using type = std::string::value_type;

                           if( const auto cp = decode( sign); cp < 0x80)
                              nrv.insert( nrv.end(), { static_cast< type>( cp)});
                           else if( cp < 0x800)
                              nrv.insert( nrv.end(), { static_cast< type>( 0xC0 | (( cp >> 6) & 0x1F)), static_cast< type>( 0x80 | ( cp & 0x3F))});
                           else if( cp < 0x10000)
                              nrv.insert( nrv.end(), { static_cast< type>( 0xE0 | (( cp >> 12) & 0x0F)), static_cast< type>( 0x80 | (( cp >> 6) & 0x3F)), static_cast< type>( 0x80 | ( cp & 0x3F))});
                           else
                              nrv.insert( nrv.end(), { static_cast< type>( 0xF0 | (( cp >> 18) & 0x07)), static_cast< type>( 0x80 | (( cp >> 12) & 0x3F)), static_cast< type>( 0x80 | (( cp >> 6) & 0x3F)), static_cast< type>( 0x80 | ( cp & 0x3F))});
                        }
                        else
                        {
                           skip();
                        }
                     }
                  }
               }

               auto literal() -> node::string
               {
                  if( ++mark, peek() == '\'')
                     if( ++mark, peek() != '\'')
                        return {};

                  const auto same = ! ( peek() == '\'' ? pull(), skip(), true : false); 

                  std::string nrv;

                  while( true)
                  {
                     const auto sign = pull();

                     if( sign == '\'')
                        if( same || peek() != '\'' && [&nrv] { return nrv.ends_with( R"('')") ? nrv.erase( nrv.size() - 2), true : false; }())
                           return nrv;

                     nrv.push_back( sign);
                  }
               }

               auto simple() -> node
               {
                  ++mark; // 't', 'f

                  const auto data = read( []( const auto sign) 
                     { 
                        return std::islower( sign); 
                     });

                  if( data == "rue")
                     return true;

                  if( data == "alse")
                     return false;

                  [[unlikely]] halt( "unexpected data");
               }

               auto number() -> node
               {
                  auto data = read( []( const auto sign)
                     {
                        // strict parsing
                        switch( sign)
                        case '+': case '-': case '.': 
                        case 'x': case 'o': case 'b': 
                        case '_': case 'e': case 'E': return true;
                        return std::isxdigit( sign) != 0;
                        // casual parsing (for NaN, Inf, etc)
                        //case '.': case '-': case '+': return true;
                        //return std::isalnum( sign) != 0;
                     });

                  std::erase( data, '_');

                  {
                     const auto base = []( const auto& data)
                     {
                        if( data.size() > 2 && data[0] == '0')
                        {
                           switch( data[1])
                           {
                           break; case 'x': return 16;
                           break; case 'o': return 8;
                           break; case 'b': return 2;
                           }
                        }
                        return 10;
                     }( data);

                     if( base != 10)
                        data.erase( 0, 2);

                     node::integer value;
                     const auto result = std::from_chars( data.data(), data.data() + data.size(), value, base);
                     if( result.ec == std::errc{} && result.ptr == ( data.data() + data.size()))
                        return value;
                  }

                  {
                     node::decimal value;
                     const auto result = std::from_chars( data.data(), data.data() + data.size(), value);
                     if( result.ec == std::errc{} && result.ptr == ( data.data() + data.size()))
                        return value;
                  }

                  [[unlikely]] halt( "unexpected data");
               }

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
            auto parse( std::istream& stream)
            {
               return detail::parser{ help::stream::ignore::bom( stream)}();
            }

            auto parse( std::string_view json)
            {
               std::ispanstream stream{ json};
               return parse( stream);
            }
         } // bom

      } // toml

   } // version

} // poly
