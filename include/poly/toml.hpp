//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "node.hpp"
#include "help.hpp"

#include <ranges>
#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
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
               return help::is::alnum( sign) || sign == '_' || sign == '-';
            };

            struct parser : help::stream::buffer::iterator::parser
            {
               using help::stream::buffer::iterator::parser::parser;

               auto operator()() -> node
               {
                  node nrv = node::table{};

                  auto where = &nrv;

                  while( true)
                  {
                     auto keys = this->keys();

                     if( keys.empty())
                     {
                        test( std::char_traits< std::istream::char_type>::eof(), peep());
                        return nrv;
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

                        where = &nrv;

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
                  leap( [] ( const auto sign) { return help::is::space( sign); });

                  if( good() && *mark == '#')
                  {
                     leap( [] ( const auto sign) { return sign != '\n';});
                     skip();
                  }
               }

               char pick()
               {
                  return skip(), pull();
               }

               char peep()
               {
                  return skip(), peek();
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
                        nrv.push_back( sign);
                     else  
                        if( peek() != '\n' && same)
                           cast< true>( nrv);
                        else
                           skip();
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
                        return help::is::lower( sign); 
                     });

                  if( data == "rue")
                     return true;

                  if( data == "alse")
                     return false;

                  [[unlikely]] halt( "unexpected data");
               }

               auto number() -> node
               {
                  if( peek() == '+')
                     ++mark;

                  auto data = read( []( const auto sign)
                     {
                        switch( sign)
                        case '.': case '+': case '-': case '_': return true;
                        return help::is::alnum( sign);
                     });

                  std::erase( data, '_');

                  // integer
                  {
                     const auto base = [&data]
                     {
                        auto sign = data.begin();

                        if( sign != data.end() && *sign == '-')
                           ++sign;

                        if( sign != data.end() && *sign == '0' && ++sign != data.end())
                        {
                           switch( *sign)
                           {
                           case 'x': return data.erase( sign - 1, sign + 1), 16;
                           case 'o': return data.erase( sign - 1, sign + 1), 8;
                           case 'b': return data.erase( sign - 1, sign + 1), 2;
                           }
                        }
                        return 10;
                     }();

                     node::integer value;
                     const auto result = std::from_chars( data.data(), data.data() + data.size(), value, base);
                     if( result.ec == std::errc{} && result.ptr == ( data.data() + data.size()))
                        return value;
                  }

                  // decimal
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

         inline auto parse( std::istream& stream)
         {
            return detail::parser{ stream}();
         }

         inline auto parse( std::string_view json)
         {
            std::ispanstream stream{ json};
            return parse( stream);
         }

         namespace bom
         {
            inline auto parse( std::istream& stream)
            {
               return detail::parser{ help::stream::ignore::bom( stream)}();
            }

            inline auto parse( std::string_view json)
            {
               std::ispanstream stream{ json};
               return parse( stream);
            }
         } // bom


         namespace detail
         {
            struct writer : help::stream::buffer::iterator::writer
            {
               using help::stream::buffer::iterator::writer::writer;

               void operator()( const node& node)
               {
                  if( node.is_table())
                     (*this)( node.as_table());
                  else
                     (*this)( node::table{ { {}, node}});
               }

               void operator()( const node::table& node)
               {
                  if( node.empty() || std::ranges::any_of( node, []( const auto& pair){ return pair.second.is_trivial(); }))
                     table();

                  for( const auto& [ name, data] : node)
                  {
                     if( trivial( data))
                     {
                        key( name);
                        copy( " = ");
                        std::visit( *this, data);
                        push( '\n');
                     }
                  }

                  for( const auto& [ name, data] : node)
                  {
                     if( complex( data))
                     {
                        stack.emplace_back( name);
                        std::visit( *this, data);
                        stack.pop_back();
                     }
                  }
               }

               void operator()( const node::array& node)
               {
                  if( node.empty() || trivial( node))
                  {
                     push( '[');
                     auto commas = node.size();
                     for(const auto& data : node)
                     {
                        std::visit( *this, data);
                        if( --commas) copy( ", ");
                     }
                     push( ']');
                  }
                  else
                  {
                     for( const auto& data : node)
                     {
                        array();

                        const auto& table = data.as_table();

                        for( const auto& [ name, data] : table)
                        {
                           if( trivial(data))
                           {
                              key(name);
                              copy( " = ");
                              std::visit( *this, data);
                              push( '\n');
                           }
                        }

                        for( const auto& [ name, data] : table)
                        {
                           if( complex( data))
                           {
                              stack.emplace_back( name);
                              std::visit( *this, data);
                              stack.pop_back();
                           }
                        }
                     }
                  }
               }

               void operator()( const node::nothing& node)
               {
                  [[unlikely]] throw std::invalid_argument{ "invalid null node"};
               }

               void operator()( const node::boolean& node)
               {
                  copy( node ? "true" : "false");
               }

               void operator()( const node::integer& node)
               {
                  copy( std::format( "{}", node));
               }

               void operator()( const node::decimal& node)
               {
                  copy( std::format( "{}", node));
               }

               void operator()( const node::string& node)
               {
                  push( '"');

                  cast( node);

                  push( '"');
               }

            private:

               void key( const auto& name)
               {
                  if( ! name.empty() && std::ranges::all_of( name, bare))
                     copy( name); // bare key
                  else
                     (*this)( node::string{ name}); // quoted key
               }

               void glean()
               {
                  auto point = stack.size();
                  for( const auto& name : stack)
                  {
                     key( name);
                     if( --point) push( '.');
                  }
               }

               void table()
               {
                  if( !stack.empty())
                  {
                     copy( "\n[");
                     glean();
                     copy( "]\n");
                  }
               }

               void array()
               {
                  if( !stack.empty())
                  {
                     copy( "\n[[");
                     glean();
                     copy( "]]\n");
                  }
               }

               static bool trivial( const node& node) noexcept
               {
                  if( node.is_trivial()) return true;
                  if( node.is_table()) return false;
                  return std::ranges::any_of( node.as_array(), trivial);
               }

               static bool complex( const node& node) noexcept
               {
                     if( node.is_trivial()) return false;
                     if( node.is_table()) return true;
                     return std::ranges::any_of( node.as_array(), complex);
               }

            private:

               std::vector< std::string_view> stack;
            };
         } // detail

         inline namespace elegant
         {
            inline auto write( const node& node, std::ostream& stream)
            {
               detail::writer{ stream}( node);
            }

            inline auto write( const node& node)
            {
               std::ostringstream stream;
               write( node, stream);
               return std::move( stream).str();
            }
         } // elegant         

      } // toml

   } // version

} // poly
