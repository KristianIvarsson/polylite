//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "core/heap.hpp"
#include "core/help.hpp"
#include "core/lint.hpp"
#include "core/load.hpp"
#include "core/save.hpp"

#include <cmath>
#include <format>
#include <string>
#include <variant>
#include <optional>
#include <unordered_map>


namespace poly_version
{
   namespace yaml
   {
      namespace detail
      {
         constexpr auto bare = [] ( const auto sign)
         {
            return core::help::is::alnum( sign) || core::help::is::some< '_', '-', '.'>( sign);
         };

         namespace parser
         {
            template< core::type::byte unit, core::load::pipe::source origin>
            struct basic : core::load::parser< unit, origin>
            {
               using base = core::load::parser< unit, origin>;
               using base::base;
               using base::full;
               using base::half;
               using base::good;
               using base::look;
               using base::mark;
               using base::peek;
               using base::pull;
               using base::read;
               using base::rest;
               using base::skip;
               using base::take;
               using base::test;

            protected:

               core::heap< node::array>   item{};
               core::heap< node::object>  pair{};

               using nill = std::monostate;
               using name = node::object::key_type;

               struct dash {};
               struct begin {};
               struct cease {};

               using info = std::variant< nill, name, node, dash, begin, cease>;

               void tidy()
               {
                  skip();

                  if( peek() == '#')
                     rest(), tidy();
               }

               char peep() 
               { 
                  return tidy(), peek(); 
               }

               auto quoted()
               {
                  ++mark; // '"'

                  std::string nrv;

                  while( true)
                  {
                     read( core::help::till::quot, nrv);

                     if( pull() == '"')
                        return nrv;

                     nrv.append( core::help::transform::point( cast( pull())));
                  }
               }

               auto single()
               {
                  ++mark; // '\''

                  std::string nrv;

                  while( true)
                  {
                     read( core::help::till::apos, nrv);

                     pull(); // '\''

                     if( peek() != '\'')
                        return nrv;

                     nrv.push_back( take()); // '\''
                  }
               }

               auto cast( const auto sign) -> std::int32_t
               {
                  switch( sign)
                  {
                  case '0': return '\0';
                  case 'a': return '\a';
                  case 'e': return 0x1B; // \e
                  case 'v': return '\v';
                  case 'x': return half();
                  case 'U': return full();
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

                  if( auto result = core::help::transform::simple( data))
                     return std::move( *result);

                  if( data == ".nan" || data == ".inf" || data == "+.inf" || data == "-.inf")
                     std::erase( data, '.');

                  if( auto result = core::help::transform::number( data))
                     return std::move( *result);

                  return node{ std::move( data)};
               }

               bool cusp()
               {
                  return ! good() || core::help::is::white( look());
               }
            };

            template< core::type::byte unit, core::load::pipe::source origin>
            struct flow : basic< unit, origin>
            {
               using base = basic< unit, origin>;
               using base::base;
               using base::cusp;
               using base::good;
               using base::halt;
               using base::item;
               using base::mark;
               using base::look;
               using base::pair;
               using base::peep;
               using base::pull;
               using base::take;
               using base::test;
               using base::quoted;
               using base::single;
               using base::resolve;
               using typename base::info;
               using typename base::name;
               using typename base::nill;

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

                  const auto from = pair.base();

                  while( peep() != '}')
                  {
                     auto data = scan();

                     if( ! std::holds_alternative< name>( data))
                        [[unlikely]] halt( "expected key");

                     pair.push( std::get< name>( std::move( data)), spot());

                     if( peep() == '}') break;

                     test( ',');
                  }

                  ++mark; // '}'

                  return pair.seal( from);
               }

               auto array() -> node::array
               {
                  ++mark; // '['

                  const auto from = item.base();

                  while( peep() != ']')
                  {
                     item.push( spot());

                     if( peep() == ']') break;
                     
                     test( ',');
                  }

                  ++mark; // ']'

                  return item.seal( from);
               }

               bool more( const std::string& data)
               {
                  return good() and not ( core::help::is::some< ',', '}', ']'>( look()) || ( look() == '#' && data.ends_with( ' ')));
               }

               info absent()
               {
                  name data;

                  while( more( data))
                  {
                     data.push_back( take());

                     if( data.back() == ':' && cusp())
                        return data.pop_back(), core::help::trim( std::move( data));
                  }

                  return resolve( core::help::trim( std::move( data)));
               }

            };

            template< core::type::byte unit, core::load::pipe::source origin>
            struct block : basic< unit, origin>
            {
               using base = basic< unit, origin>;
               using base::base;
               using base::cusp;
               using base::deny;
               using base::good;
               using base::halt;
               using base::item;
               using base::last;
               using base::leap;
               using base::look;
               using base::mark;
               using base::pair;
               using base::peek;
               using base::peep;
               using base::pull;
               using base::read;
               using base::rest;
               using base::take;
               using base::test;
               using base::tidy;
               using base::quoted;
               using base::single;
               using base::resolve;
               using typename base::info;
               using typename base::name;
               using typename base::nill;
               using typename base::dash;
               using typename base::begin;
               using typename base::cease;

               using size = int;
               
               std::unordered_map< std::string, node> anchors{};
               size dent{};

               auto operator()() -> std::optional< node>
               {
                  directives();
                  anchors.clear();
                  return spot( 0);
               }

            private:

               void line()
               {
                  rest();
               }

               size step()
               {
                  auto span = 0;
                  leap( [&span] ( const auto sign) { return sign == ' ' ? ++span, true : false;});
                  return span;
               }

               size next()
               {
                  auto span = step();

                  if( idle()) 
                     return line(), next();

                  return span;
               }

               void directives()
               {
                  auto part = [this]
                  {
                     tidy();
                     return read( [] ( const auto sign) { return ! core::help::is::white( sign); });
                  };

                  while( peep() == '%')
                  {
                     ++mark; // '%'

                     const auto word = part();

                     if( word == "YAML")
                        // we ignore the version (but consumes it)
                        part();
                     else if( word == "TAG")
                        // we ignore custom tags (but consumes handle and prefix)
                        part(), part(); 
                     else [[unlikely]]
                        deny( "directive");
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
                     return info{ parser::flow< unit, origin&>{ this->feed}.spot()};
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
                  if( core::help::hold< begin, cease>( data))
                     return line(), dent = 0, true;

                  return false;
               }

               void wrap()
               {
                  step();
                  if( idle())
                     dent = next();
               }

               auto head() -> std::optional< info>
               {
                  while( good())
                  {
                     auto data = scan();

                     if( core::help::hold< begin, nill>( data))
                        dent = next();
                     else
                        return data;
                  }

                  return std::nullopt;
               }

               auto bind( const size root) -> node
               {
                  step();
                  auto data = scan();

                  if( core::help::hold< name>( data))
                     halt( "unexpected key");

                  wrap();

                  if( core::help::hold< node>( data))
                     return std::move( std::get< node>( std::move( data)));

                  if( root < dent || ( root == dent && peek() == '-'))
                     return *spot( dent);

                  return nullptr;
               }

               auto spot( const size root) -> std::optional< node>
               {
                  auto gist = head();

                  if( ! gist) return root ? std::optional< node>{ nullptr} : std::nullopt;

                  auto& data = *gist;

                  if( edge( data)) return node{ nullptr};

                  if( core::help::hold< node>( data))
                     return line(), dent = next(), std::get< node>( std::move( data));

                  if( core::help::hold< dash>( data))
                  {
                     const auto from = item.base();

                     do
                        item.push( *spot( dent + 1 + step()));
                     while( dent >= root && peek() == '-' && core::help::hold< dash>( scan()));

                     return item.seal( from);
                  }

                  const auto from = pair.base();
                  
                  while( good())
                  {
                     while( good() && core::help::hold< nill>( data))
                        line(), data = scan();

                     if( ! good())
                        break;

                     if( edge( data))
                        break;

                     pair.push( std::get< name>( std::move( data)), bind( dent));

                     if( dent < root)
                        break;

                     wrap();

                     if( dent < root)
                        break;

                     data = scan();
                  }

                  return pair.seal( from);
               }

               bool more( const std::string& data)
               {
                  return good() and not ( look() == '\n' || ( look() == '#' && data.ends_with( ' ')));                  
               }

               info absent()
               {
                  name data;

                  while( more( data))
                  {
                     data.push_back( take());

                     if( data.back() == ':' && cusp())
                     {
                        data.pop_back();
                        auto key = core::help::trim( std::move( data));
                        if( key == "<<") deny( "merge keys");
                        return key;
                     }

                     if( data.size() == 1)
                     {
                        if( data.back() == '-' && cusp())
                           return dash{};

                        if( data.back() == '?' && cusp())
                           deny( "explicit keys");
                     }
                  }

                  if( dent == 0)
                  {
                     // consuming potential values after start/end markers
                     if( data.starts_with( "---")) return begin{};
                     if( data.starts_with( "...")) return cease{};
                  }

                  return resolve( core::help::trim( std::move( data)));
               }

               bool idle()
               {
                  return core::help::is::some< '\n', '#'>( peek());
               }

               info alias()
               {
                  ++mark; // '*'

                  const auto word = read( bare);

                  if( anchors.contains( word))
                     return anchors.at( word);

                  [[unlikely]] halt( "invalid alias");
               }

               info anchor()
               {
                  ++mark; // '&'
                  
                  auto word = read( bare);
                  tidy();
                  auto data = scan();

                  if( std::holds_alternative< node>( data))
                     return anchors[ std::move( word)] = std::get< node>( std::move( data));

                  [[unlikely]] halt( "invalid anchor");                  
               }

               auto scalar()
               {
                  std::string nrv;

                  while( more( nrv))
                     nrv.push_back( take());

                  return nrv;
               }

               info tagged()
               {
                  ++mark; // '!'

                  // only support core tags
                  test( '!');

                  const auto tag = read( []( const auto sign) { return core::help::is::lower( sign); });

                  tidy();

                  if( tag == "str")
                     if( core::help::is::none< '"', '\''>( peek()))
                        return node{ core::help::trim( scalar())};
   
                  auto data = scan();

                  if( std::holds_alternative< nill>( data))
                     data = node{ nullptr};

                  if( auto what = std::get_if< node>( &data))
                  {
                     if( what->is_string() && tag == "str")
                        return data;

                     if( what->is_nothing() && tag == "null")  
                        return data;

                     if( what->is_boolean() && tag == "bool")  
                        return data;

                     if( what->is_integer() && tag == "int")  
                        return data;

                     if( what->is_decimal() && tag == "float")
                        return data;

                     if( what->is_integer() && tag == "float")
                        return node{ static_cast< node::decimal>( what->as_integer())};
                     
                     if( what->is_string() && tag == "timestamp")
                        if( auto result = core::help::transform::instant( core::help::trim( what->as_string())))
                           return *result;
                     
                     if( what->is_string() && tag == "binary")
                        if( auto result = core::help::transform::binary( core::help::trim( what->as_string())))
                           return *result;
                  }

                  [[unlikely]] halt( std::format( "!!{} is invalid", tag));
               }

               info styled()
               {
                  const auto style = take();
                  const auto chomp = core::help::is::some< '+', '-'>( peek()) ? take() : '\0';

                  std::string text;
                  size root{};
                  size nada{};

                  while( good())
                  {
                     rest();

                     dent = step();

                     if( dent < root && peek() != '\n')
                        break;

                     const auto line = read( core::help::till::line);

                     if( line.empty())
                     {
                        ++nada;
                        continue;
                     }

                     if( text.empty())
                        text.append( nada, '\n');
                     else
                        if( style == '>')
                           if( nada)
                              text.append( nada, '\n');
                           else
                              text.push_back( ' ');
                        else
                           text.append( nada + 1, '\n');

                     if( ! root) 
                        root = dent;

                     text.append( line);

                     nada = 0;
                  }

                  text.append( nada * (chomp == '+') + (chomp != '-'), '\n');

                  return node{ std::move( text)};
               }

            };

         } // parser

      } // detail

      inline namespace one
      {
         auto parse( auto&& source)
         {
            return core::load::make::source< detail::parser::block>( source)().value();
         }
      } // one

      namespace all
      {
         auto parse( auto&& source)
         {
            auto parser = core::load::make::source< detail::parser::block>( source);

            node::array nrv;

            while( auto document = parser())
               nrv.emplace_back( std::move( *document));

            return nrv;
         }
      } // all


      namespace detail
      {
         constexpr std::size_t spaces = 2;

         namespace writer
         {

            template< core::type::byte unit, core::save::pipe::target output>
            struct basic : core::save::writer< unit, output>
            {
               using base = core::save::writer< unit, output>;
               using base::base;
               using base::push;
               using base::copy;
               using base::cast;
               using base::time;
               using base::halt;

               void operator() ( const node::nothing)
               {
                  copy( "null");
               }

               void operator() ( const node::boolean node)
               {
                  copy( node ? "true" : "false");
               }

               void operator() ( const node::integer node)
               {
                  copy( std::format( "{}", node));
               }

               void operator() ( const node::decimal node)
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
                  
                  std::visit( [ this]( const auto& data) { this->time( data); }, node);
               }

               void operator() ( const node::string& node)
               {
                  auto plain = [&node]
                  {
                     if( node.empty())
                        return false;

                     switch( node.front())
                     case '{': case '[': case '*': case '&': case '|': 
                     case '!': case '>': case '"': case '~': case '-': 
                     case '.': case '\'':case '?': case '`': case '%':
                        return false;

                     if( core::help::is::space( node.front()) || core::help::is::space( node.back()))
                        return false;

                     if( node.find_first_of( ":#\n") != std::string::npos)
                        return false;

                     return ! core::help::transform::simple( node) && ! core::help::transform::number( node);
                  };

                  if( plain())
                     copy( node);
                  else
                     push( '"'), cast( node), push( '"');
               }
            };

            template< core::type::byte unit, core::save::pipe::target output>
            struct flow : basic< unit, output>
            {
               using base = basic< unit, output>;
               using base::base;
               using base::push;
               using base::copy;
               using base::cast;
               using base::flat;
               using base::operator();

               void operator() ( const node::object& node)
               {
                  push( '{');
                  auto comma = node.size();
                  for( const auto& [ name, data] : node)
                  {
                     (*this)( name);
                     copy( ": ");
                     std::visit( *this, data);
                     if( --comma) copy( ", ");
                  }
                  push( '}');
               }

               void operator() ( const node::array& node)
               {
                  push( '[');
                  auto comma = node.size();
                  for( const auto& data : node)
                  {
                     std::visit( *this, data);
                     if( --comma) copy( ", ");
                  }
                  push( ']');
               }

               void operator() ( const node::string& node)
               {
                  if( node.find_first_of( ",]}") != std::string::npos)
                     push( '"'), cast( node), push( '"');
                  else
                     base::operator()( node);
               }

               void operator() ( const node::binary& node)
               {
                  copy( "!!binary ");
                  flat( node);
               }
            };

            template< core::type::byte unit, core::save::pipe::target output, std::size_t spaces>
            struct block : basic< unit, output>
            {
               using base = basic< unit, output>;
               using base::base;
               using base::sink;
               using base::push;
               using base::copy;
               using base::wrap;
               using base::operator();

               char column{};
               bool indent{};

               void operator() ( const node::object& node)
               {
                  for( const auto& [ name, data] : node)
                  {
                     fill();
                     (*this)( name);
                     push( ':');

                     if constexpr( spaces)
                     {
                        if( data.is_trivial())
                        {
                           push( ' ');
                           std::visit( *this, data);
                        }
                        else
                        {
                           ++column;
                           std::visit( *this, data);
                           --column;
                        }
                     }
                     else
                     {
                        push( ' ');
                        std::visit( flow< unit, output&>{ sink}, data);
                     }
                  }
               }

               void operator() ( const node::array& node)
               {
                  if constexpr( spaces)
                     for( const auto& data : node)
                     {
                        fill();
                        copy( "- ");

                        if( data.is_trivial())
                        {
                           std::visit( *this, data);
                        }
                        else
                        {
                           indent = false;
                           ++column;
                           std::visit( *this, data);
                           --column;
                           indent = true;
                        }
                     }
                  else
                     flow< unit, output&>{ sink}( node);
               }

               void operator() ( const node::binary& node)
               {
                  copy( "!!binary ");
                  push( '|'), wrap( node, column * spaces);
               }

            private:

               void fill()
               {
                  if( indent)
                     base::fold( column * spaces);
                  else
                     indent = true;
               }

            };

         } // writer

         template< std::size_t spaces>
         auto write( const node& root, auto&& target)
         {
            auto sink = core::save::make::target< writer::block, spaces>( target);
            std::visit( sink, root);
         }

         // the default write function
         template< std::size_t spaces>
         auto write( const node& root)
         {
            std::string target;
            write< spaces>( root, target);
            return target;
         }

      } // detail


      inline namespace elegant
      {
         template< std::size_t spaces = detail::spaces>
         auto write( auto&&... parameters)
         {
            return detail::write< spaces>( std::forward< decltype( parameters)>( parameters)...);
         }
      } // elegant

      namespace compact
      {
         auto write( auto&&... parameters)
         {
            return detail::write< 0>( std::forward< decltype( parameters)>( parameters)...);
         }
      } // compact

      namespace detail
      {
         struct rule : core::lint::rule
         {
            template< typename type>
            static constexpr bool block = core::type::some< type, node::local_time>;
         };
      } // detail

      decltype( auto) check( auto&& data)
      {
         return core::lint::test< detail::rule>( std::forward< decltype( data)>( data));
      }
   } // yaml

} // poly_version
