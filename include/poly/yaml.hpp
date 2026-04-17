//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "help.hpp"
#include "json.hpp"

#include <ranges>
#include <string>
#include <sstream>
#include <optional>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <spanstream>


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

               auto operator()() -> std::optional<node>
               {
                  directives();
                  return spot( 0);
               }

            private:

               using size = int;

               using nill = std::monostate;
               using name = node::object::key_type;

               struct dash {};

               struct begin {};
               struct cease {};

               using info = std::variant< nill, name, node, dash, begin, cease>;

               void line()
               {
                  leap( rest);
                  if( good()) ++mark;
               }               

               void skip()
               {
                  leap( [] ( const auto sign) { return help::is::space( sign); });

                  if( peek() == '#')
                  {
                     line();
                     skip();
                  }
               }

               size step()
               {
                  auto size = 0;
                  leap( [&size] ( const auto sign) { return sign == ' ' ? ++size, true : false;});
                  return size;
               }

               char pick()
               {
                  return skip(), pull();
               }

               char peep()
               {
                  return skip(), peek();
               }               
               
               size next()
               {
                  auto size = step();

                  switch( peek())
                  case '#': case '\n':
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

               auto scan()
               {
                  switch( peek())
                  {
                  case '!':
                     return tagged();
                  case '{': case '[':
                     return flow();
                  case '|': case '>':
                     return block_scalar();
                  case '\"': 
                     return double_quote();
                  case '\'': 
                     return single_quote();
                  default: 
                     return absent_quote();
                  }
               }

               auto spot( const auto base) -> std::optional< node>
               {
                  auto info = scan();

                  auto boundary = [this] ( const auto info)
                  {
                     if( std::holds_alternative< begin>( info) || std::holds_alternative< cease>( info))
                        return line(), dent = 0, true;
                     
                     return false;
                  };

                  if( std::holds_alternative< begin>( info))
                     line(), info = scan();

                  while( std::holds_alternative< nill>( info) && good())
                     line(), info = scan();

                  if( std::holds_alternative< nill>( info))
                     return base ? std::optional< node>{ nullptr} : std::nullopt;

                  if( boundary( info))
                     return node{ nullptr};

                  if( std::holds_alternative< dash>( info))
                  {
                     node::array array;

                     while( good())
                     {
                        const auto content = dent + 1 + step();
                        array.emplace_back( *spot( content));

                        if( dent < base)
                           break;

                        info = scan();

                        if( ! std::holds_alternative< dash>( info))
                           break;
                     }

                     return array;
                  }

                  if( std::holds_alternative< node>( info))
                     return line(), dent = next(), std::get< node>( std::move( info));

                  node::object map;

                  while( good())
                  {
                     if( std::holds_alternative< nill>( info))
                     {
                        line();
                        info = scan();
                        continue;
                     }

                     if( boundary( info))
                        break;

                     auto& data = map[ std::get< name>( std::move( info)) ];

                     step();
                     info = scan();

                     if( std::holds_alternative< name>( info))
                        halt( "unexpected key");

                     auto size = peek() == '\n' || ! good() ? next() : dent;

                     if( size > base)
                     {
                        if( std::holds_alternative< node>( info))
                           halt( "unexpected scalar");

                        dent = size;
                        data = *spot( size);

                        if( dent < base)
                           break;
                     }

                     if( std::holds_alternative< node>( info))
                        data = std::get< node>( std::move( info));

                     if( size < base)
                     {
                        dent = size;
                        break;
                     }

                     if( good())
                        info = scan();
                     else
                        break;

                     if( boundary( info))
                        break;

                     if( std::holds_alternative< node>( info))
                        halt( "unexpected scalar");
                  }

                  return map;
               }

               auto cast( const auto sign) -> std::int32_t
               {
                  switch( sign)
                  {
                  case '0': return '\0';
                  case 'a': return '\a';
                  case 'v': return '\v';
                  case 'e': return '\e';
                  case 'x': return unit< 2>();
                  case 'U': return unit< 8>();
                  case 'N': return 0x85;
                  case '_': return 0xA0;
                  case 'L': return 0x2028;
                  case 'P': return 0x2029;
                  default:  return base::cast( sign);
                  }
               }               

               info flow()
               {
                  return json::detail::parser{ mark}.spot();
               }

               auto scalar()
               {
                  std::string nrv;

                  while( good())
                     if( *mark == '\n' || *mark == '#' && nrv.ends_with( ' '))
                        break;
                     else
                        nrv.push_back( *mark++);

                  return nrv;
               }

               info tagged()
               {
                  ++mark; // '!'

                  // only support core tags
                  test( '!', pull());

                  const auto tag = read( bare);  // reads until non-bare char

                  skip();

                  if( tag == "str")
                     if( peek() != '"' && peek() != '\'')
                        return node{ scalar()};
   
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
                  }

                  [[unlikely]] halt( std::format( "invalid !!{} construct", tag));
               }

               info absent_quote()
               {
                  std::string data;

                  while( good())
                  {
                     if( *mark == '\n' || *mark == '#' && data.ends_with( ' '))
                        break;

                     data.push_back( *mark++);

                     if( data.back() == ':' && cusp())
                        return data.pop_back(), data;
                     
                     if( data.back() == '-' && cusp() && data.size() == 1)
                        return dash{};
                  }

                  return [this]( auto data) -> info
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

                     if( dent == 0)
                     {
                        if( data == "---") return begin{};
                        if( data == "...") return cease{};
                     }

                     return node{ data};

                  }( help::trim( std::move( data)));
               }

               info block_scalar()
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

               info single_quote()
               {
                  ++mark; // '

                  std::string data;

                  while( true)
                  {
                     const auto sign = pull();

                     if( sign == '\'')
                        if( peek() != '\'')
                           break;
                        else
                           ++mark;

                     data.push_back( sign);
                  }

                  if( peek() != ':')
                     return node{ std::move( data)};

                  return ++mark, data;
               }

               info double_quote()
               {
                  ++mark; // "

                  std::string data;

                  while( true)
                  {
                     const auto sign = pull();

                     if( sign == '"')
                        break;

                     if( sign != '\\') [[likely]]
                        data.push_back( sign);
                     else
                        data.append( help::transform::point( cast( pull())));
                  }

                  if( peek() != ':')
                     return node{ std::move( data)};

                  return ++mark, data;
               }

               bool cusp() const
               {
                  return ! good() || help::is::space( *mark);
               }

            private:

               int dent{};

            };

         } // detail

         inline namespace one
         {
            inline auto parse( std::istream& stream)
            {
               return detail::parser{ stream}().value();
            }

            inline auto parse( std::string_view json)
            {
               std::ispanstream stream{ json};
               return parse( stream);
            }
         } // one

         namespace all
         {
            inline auto parse( std::istream& stream)
            {
               detail::parser parser{ stream};

               node::array nrv;

               while( auto document = parser())
                  nrv.emplace_back( std::move( *document));
               
               return nrv;
            }

            inline auto parse( std::string_view json)
            {
               std::ispanstream stream{ json};
               return parse( stream);
            }
         } // all


         namespace detail
         {
            constexpr std::size_t spaces = 2;

            template< std::size_t spaces>
            struct writer : help::stream::buffer::iterator::writer
            {
               using base::base;

               void key( const auto& name)
               {
                  if( ! name.empty() && std::ranges::all_of( name, bare))
                     copy( name);
                  else
                     (*this)( node::string{ name});
               }

               void operator() ( const node::object& node)
               {
                  if constexpr( spaces)
                  {
                     for( const auto& [ name, data] : node)
                     {
                        fill();
                        key( name);
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
                     json::detail::writer< 0>{ mark}( node);
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
                     json::detail::writer< 0>{ mark}( node);
                  }
               }

               void operator() ( const node::nothing& node)
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

               void operator() ( const node::string& node)
               {
                  push( '"');
                  cast( node);
                  push( '"');
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
            inline auto write( const node& node, std::ostream& stream)
            {
               yaml::write< 0>( node, stream);
            }

            inline auto write( const node& node)
            {
               std::ostringstream stream;
               write( node, stream);
               return std::move( stream).str();
            }
         } // compact

      } // yaml

   } // version

} // poly
