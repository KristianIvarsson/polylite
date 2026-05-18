//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "help.hpp"
#include "json.hpp"

#include <format>
#include <string>
#include <vector>
#include <cassert>
#include <sstream>
#include <optional>
#include <stdexcept>
#include <algorithm>
#include <spanstream>
#include <string_view>
#include <unordered_map>


namespace poly
{
   inline namespace version
   {
      namespace yaml
      {
         namespace detail
         {
            constexpr auto bare = [] ( const auto sign)
            {
               return help::is::alnum( sign) || sign == '_' || sign == '-' || sign == '.';
            };

            constexpr auto rest = [] ( const auto sign)
            {
               return sign != '\n';
            };

            struct parser : help::stream::buffer::iterator::parser
            {
               using base::base;

            protected:

               using nill = std::monostate;
               using name = node::object::key_type;

               struct dash {};
               struct begin {};
               struct cease {};

               using info = std::variant< nill, name, node, dash, begin, cease>;

               template< class... types>
               static bool hold( const auto& data)
               {
                  return ( std::holds_alternative< types>( data) || ... );
               }

               void skip()
               {
                  leap( [] ( const auto sign) { return help::is::space( sign); });

                  if( peek() == '#')
                  {
                     leap( rest);
                     if( good()) ++mark;
                     skip();
                  }
               }

               char pick() { return skip(), pull(); }
               char peep() { return skip(), peek(); }


               auto quoted()
               {
                  ++mark; // '"'

                  std::string nrv;

                  while( true)
                  {
                     const auto sign = pull();

                     if( sign == '"') break;

                     if( sign != '\\') [[likely]]
                        nrv.push_back( sign);
                     else
                        nrv.append( help::transform::point( cast( pull())));
                  }

                  return nrv;
               }

               auto single()
               {
                  ++mark; // '\''

                  std::string nrv;

                  while( true)
                  {
                     const auto sign = pull();

                     if( sign == '\'')
                     {
                        if( peek() != '\'') break;
                        else ++mark;
                     }

                     nrv.push_back( sign);
                  }

                  return nrv;
               }

               auto cast( const auto sign) -> std::int32_t
               {
                  switch( sign)
                  {
                  case '0': return '\0';
                  case 'a': return '\a';
                  case 'e': return 0x1B; // \e
                  case 'v': return '\v';
                  case 'x': return unit< 2>();
                  case 'U': return unit< 8>();
                  case 'N': return 0x85;
                  case '_': return 0xA0;
                  case 'L': return 0x2028;
                  case 'P': return 0x2029;
                  default:  return base::cast( sign);
                  }
               }

               static auto resolve( std::string data) -> info
               {
                  if( data.empty())
                     return nill{};

                  if( data == "~")
                     return node{ nullptr};

                  if( auto result = help::transform::simple( data))
                     return std::move( *result);

                  if( data == ".nan" || data == ".inf" || data == "+.inf" || data == "-.inf")
                     std::erase( data, '.');

                  if( auto result = help::transform::number( data))
                     return std::move( *result);

                  return node{ std::move( data)};
               }

               bool cusp() const
               {
                  return ! good() || help::is::space( *mark);
               }
            };

            struct flow : parser
            {
               using parser::parser;

               auto spot() -> node
               {
                  auto data = scan();

                  if( auto* result = std::get_if< node>( &data))
                     return std::move( *result);

                  [[unlikely]] halt( "expected value");
               }

            private:

               info decide( auto data)
               {
                  if( peep() == ':') 
                     return ++mark, name{ std::move( data)};
                  return node{ std::move( data)};
               }

               auto scan() -> info
               {
                  switch( peep())
                  {
                  case '{':   return node{ object()};
                  case '[':   return node{ array()};
                  case '"':   return decide( quoted());
                  case '\'':  return decide( single());
                  default:    return absent();
                  }
               }

               auto object() -> node::object
               {
                  ++mark; // '{'

                  node::object nrv;

                  while( peep() != '}')
                  {
                     auto data = scan();

                     if( ! std::holds_alternative< name>( data))
                        [[unlikely]] halt( "expected key");

                     nrv.emplace( std::get< name>( std::move( data)), spot());

                     if( peep() == '}') break;

                     test( ',', pull());
                  }

                  ++mark; // '}'

                  return nrv;
               }

               auto array() -> node::array
               {
                  ++mark; // '['

                  node::array nrv;

                  while( peep() != ']')
                  {
                     nrv.emplace_back( spot());

                     if( peep() == ']') break;
                     
                     test( ',', pull());
                  }

                  ++mark; // ']'

                  return nrv;
               }

               bool more( const std::string& data) const
               {
                  return good() and not ( *mark == ',' || *mark == '}' || *mark == ']' || ( *mark == '#' && data.ends_with( ' ')));
               }

               info absent()
               {
                  name data;

                  while( more( data))
                  {
                     data.push_back( *mark++);

                     if( data.back() == ':' && cusp())
                        return data.pop_back(), help::trim( std::move( data));
                  }

                  return resolve( help::trim( std::move( data)));
               }

            };

            struct block : parser
            {
               using parser::parser;

               auto operator()() -> std::optional<node>
               {
                  directives();
                  anchors.clear();
                  return spot( 0);
               }

            private:

               using size = int;

               size dent{};

            private:

               void line()
               {
                  leap( rest);
                  if( good()) ++mark;
               }

               size step()
               {
                  auto size = 0;
                  leap( [&size] ( const auto sign) { return sign == ' ' ? ++size, true : false;});
                  return size;
               }

               size next()
               {
                  auto size = step();

                  if( done())
                     return line(), next();

                  return size;
               }

               void directives()
               {
                  auto part = [this]
                  {
                     skip();
                     return read( [] ( const auto sign) { return ! help::is::space( sign); });
                  };

                  while( peep() == '%')
                  {
                     ++mark; // '%'

                     const auto name = part();

                     if( name == "YAML")
                        // we ignore the version (but consumes it)
                        part();
                     else if( name == "TAG")
                        // we ignore custom tags (but consumes handle and prefix)
                        part(), part(); 
                     else [[unlikely]]
                        halt( "unsupported directive");
                  }
               }

               info decide( auto data)
               {
                  if( peek() == ':') 
                     return ++mark, name{ std::move( data)};
                  return node{ std::move( data)};
               }
               
               auto scan()
               {
                  switch( peek())
                  {
                  case '{': case '[':
                     return info{ flow{ mark}.spot()};
                  case '*':
                     return alias();
                  case '&':
                     return anchor();
                  case '!':
                     return tagged();
                  case '|': case '>':
                     return styled();
                  case '\"': 
                     return decide( quoted());
                  case '\'': 
                     return decide( single());
                  default: 
                     return absent();
                  }
               }


               bool edge( const info& data)
               {
                  if( hold< begin, cease>( data))
                     return line(), dent = 0, true;

                  return false;
               }

               void wrap()
               {
                  step();
                  if( done())
                     dent = next();
               }

               auto head() -> std::optional< info>
               {
                  while( good())
                  {
                     auto info = scan();

                     if( hold< begin, nill>( info))
                        dent = next();
                     else
                        return info;
                  }

                  return std::nullopt;
               }

               auto grab( const size base) -> node
               {
                  step();
                  auto info = scan();

                  if( hold< name>( info))
                     halt( "unexpected key");

                  wrap();

                  if( hold< node>( info))
                     return std::move( std::get< node>( std::move( info)));

                  if( base < dent || ( base == dent && peek() == '-'))
                     return *spot( dent);

                  return nullptr;
               }

               auto spot( const size base) -> std::optional< node>
               {
                  auto data = head();

                  if( ! data) return base ? std::optional< node>{ nullptr} : std::nullopt;

                  if( edge( *data)) return node{ nullptr};

                  if( hold< node>( *data))
                     return line(), dent = next(), std::get< node>( std::move( *data));

                  if( hold< dash>( *data))
                  {
                     node::array array;

                     do
                        array.emplace_back( *spot( dent + 1 + step()));
                     while( dent >= base && peek() == '-' && hold< dash>( scan()));

                     return { array};
                  }

                  node::object object;

                  auto& info = *data;

                  while( good())
                  {
                     while( good() && hold< nill>( info))
                        line(), info = scan();

                     if( ! good())
                        break;

                     if( edge( info))
                        break;

                     object[ std::get< name>( std::move( info))] = grab( dent);

                     if( dent < base)
                        break;

                     wrap();

                     if( dent < base)
                        break;

                     info = scan();
                  }

                  return { object};
              }

               bool more( const std::string& data) const
               {
                  return good() and not ( *mark == '\n' || ( *mark == '#' && data.ends_with( ' ')));                  
               }

               info absent()
               {
                  name data;

                  while( more( data))
                  {
                     data.push_back( *mark++);

                     if( data.back() == ':' && cusp())
                        return data.pop_back(), help::trim( std::move( data));

                     if( data.back() == '-' && cusp() && data.size() == 1)
                        return dash{};
                  }

                  if( dent == 0)
                  {
                     // consuming potential values after start/end markers
                     if( data.starts_with( "---")) return begin{};
                     if( data.starts_with( "...")) return cease{};
                  }

                  return resolve( help::trim( std::move( data)));
               }

               bool done() const
               {
                  return peek() == '\n' || peek() == '#';
               }

               info alias()
               {
                  ++mark; // '*'

                  const auto name = read( bare);

                  if( anchors.contains( name))
                     return anchors.at( name);

                  [[unlikely]] halt( "invalid alias");
               }

               info anchor()
               {
                  ++mark; // '&'
                  
                  auto name = read( bare);
                  skip();
                  auto data = scan();

                  if( std::holds_alternative< node>( data))
                     return anchors[ std::move( name)] = std::get< node>( std::move( data));

                  [[unlikely]] halt( "invalid anchor");                  
               }

               auto scalar()
               {
                  std::string nrv;

                  while( more( nrv))
                     nrv.push_back( *mark++);

                  return nrv;
               }

               info tagged()
               {
                  ++mark; // '!'

                  // only support core tags
                  test( '!', pull());

                  const auto tag = read( []( const auto sign) { return help::is::lower( sign); });

                  skip();

                  if( tag == "str")
                     if( peek() != '"' && peek() != '\'')
                        return node{ help::trim( scalar())};
   
                  auto info = scan();

                  if( std::holds_alternative< nill>( info))
                     info = node{ nullptr};

                  if( auto data = std::get_if< node>( &info))
                  {
                     if( data->is_string() && tag == "str")
                        return info;

                     if( data->is_nothing() && tag == "null")  
                        return info;

                     if( data->is_boolean() && tag == "bool")  
                        return info;

                     if( data->is_integer() && tag == "int")  
                        return info;

                     if( data->is_decimal() && tag == "float")
                        return info;

                     if( data->is_integer() && tag == "float")
                        return node{ static_cast< node::decimal>( data->as_integer())};
                       
                     if( data->is_string() && tag == "timestamp")
                        if( auto result = help::transform::instant( help::trim( data->as_string())))
                           return *result;
                     
                     if( data->is_string() && tag == "binary")
                        if( auto result = help::transform::binary( help::trim( data->as_string())))
                           return *result;
                  }

                  [[unlikely]] halt( std::format( "!!{} is invalid", tag));
               }

               info styled()
               {
                  const auto style = pull();
                  const auto chomp = peek() == '+' || peek() == '-' ? pull() : '\0';

                  leap( rest);

                  std::string data;
                  size base{};
                  size nada{};

                  while( good())
                  {
                     ++mark; // '\n'

                     dent = step();

                     if( dent < base && peek() != '\n')
                        break;

                     const auto line = read( rest);

                     if( line.empty())
                     {
                        ++nada;
                        continue;
                     }

                     if( data.empty())
                     {
                        data.append( nada, '\n');
                     }
                     else
                     {
                        if( style == '>')
                        {
                           if( nada)
                              data.append( nada, '\n');
                           else
                              data.push_back( ' ');
                        }
                        else
                        {
                           data.append( nada + 1, '\n');
                        }
                     }

                     if( ! base) 
                        base = dent;

                     data.append( line);

                     nada = 0;
                  }

                  data.append( nada * (chomp == '+') + (chomp != '-'), '\n');

                  return node{ std::move( data)};
               }

            private:

               std::unordered_map< std::string, node> anchors;

            };

         } // detail

         inline namespace one
         {
            inline auto parse( std::istream& stream)
            {
               return detail::block{ stream}().value();
            }

            inline auto parse( std::string_view data)
            {
               std::ispanstream stream{ data};
               return parse( stream);
            }
         } // one

         namespace all
         {
            inline auto parse( std::istream& stream)
            {
               detail::block parser{ stream};

               node::array nrv;

               while( auto document = parser())
                  nrv.emplace_back( std::move( *document));
               
               return nrv;
            }

            inline auto parse( std::string_view yaml)
            {
               std::ispanstream stream{ yaml};
               return parse( stream);
            }
         } // all


         namespace detail
         {
            constexpr std::size_t spaces = 2;

            template< std::size_t spaces, bool strict>
            struct writer : help::stream::buffer::iterator::writer
            {
               using base::base;

               void operator() ( const node::object& node)
               {
                  if constexpr( spaces)
                  {
                     for( const auto& [ name, data] : node)
                     {
                        fill();
                        (*this)( name);
                        if( data.is_trivial())
                        {
                           copy( ": ");
                           indent = false;
                           std::visit( *this, data);
                           indent = true;
                        }
                        else
                        {
                           push( ':');
                           ++column;
                           std::visit( *this, data);
                           --column;
                        }
                     }
                  }
                  else
                  {
                     json::detail::writer< 0, strict>{ mark}( node);
                  }
               }

               void operator() ( const node::array& node)
               {
                  if constexpr( spaces)
                  {
                     for( const auto& data : node)
                     {
                        fill();
                        copy( "- ");
                        indent = false;
                        ++column;
                        std::visit( *this, data);
                        --column;
                        indent = true;
                     }
                  }
                  else
                  {
                     json::detail::writer< 0, strict>{ mark}( node);
                  }
               }

               void operator() ( const node::nothing& )
               {
                  copy( "null");
               }

               void operator() ( const node::boolean& node)
               {
                  copy( node ? "true" : "false");
               }

               void operator() ( const node::integer& node)
               {
                  copy( std::format( "{}", node));
               }

               void operator() ( const node::decimal& node)
               {
                  if( std::isnan( node))
                     [[unlikely]] return copy( ".nan");

                  if( std::isinf( node))
                     [[unlikely]] return copy( std::signbit( node) ? "-.inf" : "+.inf");

                  copy( std::format( "{}", node));
               }

               void operator() ( const node::instant& node)
               {
                  if( ! std::holds_alternative< node::local_time>( node))
                     copy( "!!timestamp ");
                  else
                     if constexpr( strict)
                        halt( "node::local_time");
                  
                  std::visit( [ this]( const auto& data) { time( data); }, node);
               }

               void operator() ( const node::string& node)
               {
                  auto plain = [&node]
                  {
                     if( node.empty())
                        return false;

                     switch( node.front())
                     case '{': case '[': case '*': case '&': 
                     case '|': case '!': case '>': case '"': 
                     case '~': case '-': case '.': case '\'':
                        return false;

                     if( help::is::space( node.front()) || help::is::space( node.back()))
                        return false;

                     if( node.find_first_of( ":#\n") != std::string::npos)
                        return false;

                     return ! help::transform::simple( node) && ! help::transform::number( node);
                  };

                  if( plain())
                     copy( node);
                  else
                     push( '"'), cast( node), push( '"');
               }

               void operator() ( const node::binary& node)
               {
                  copy( "!!binary ");

                  if constexpr( spaces)
                     push( '|'), data( node, column * spaces);
                  else
                     data< 0>( node);
               }

            private:

               void fill()
               {
                  if( indent)
                     push( '\n'), std::fill_n( mark, column * spaces, ' ');
                  else
                     indent = true;
               }

            private:

               char column{};
               bool indent{};

            };

            template< std::size_t spaces, bool strict>
            auto write( const node& node, std::ostream& stream)
            {
               std::visit( writer< spaces, strict>{ stream}, node);
            }

            template< std::size_t spaces, bool strict>
            auto write( const node& node)
            {
               std::ostringstream stream;
               write< spaces, strict>( node, stream);
               return std::move( stream).str();
            }

         } // detail


         inline namespace elegant
         {
            inline namespace strict
            {
               template< std::size_t spaces = detail::spaces>
               auto write( auto&&... parameters)
               {
                  return detail::write< spaces, true>( std::forward< decltype( parameters)>( parameters)...);
               }
            } // strict

            namespace gentle
            {
               template< std::size_t spaces = detail::spaces>
               auto write( auto&&... parameters)
               {
                  return detail::write< spaces, false>( std::forward< decltype( parameters)>( parameters)...);
               }
            } // gentle
         } // elegant

         namespace compact
         {
            inline namespace strict
            {
               auto write( auto&&... parameters)
               {
                  return detail::write< 0, true>( std::forward< decltype( parameters)>( parameters)...);
               }
            } // strict

            namespace gentle
            {
               auto write( auto&&... parameters)
               {
                  return detail::write< 0, false>( std::forward< decltype( parameters)>( parameters)...);
               }
            } // gentle
         } // compact

      } // yaml

   } // version

} // poly
