//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "help.hpp"

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
            return help::is::alnum( sign) || sign == '_' || sign == '-' || sign == '.';
         };

         namespace parser
         {
            template< help::sign type, help::source_iterator< type> iterator>
            struct core : help::parser< type, iterator>
            {
               using base = help::parser< type, iterator>;
               using base::full;
               using base::half;
               using base::good;
               using base::look;
               using base::mark;
               using base::peek;
               using base::pull;
               using base::rest;
               using base::skip;

            protected:

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
                  return ! good() || help::is::space( look());
               }
            };

            template< help::sign type, help::source_iterator< type> iterator>
            struct flow : core< type, iterator>
            {
               using base = parser::core< type, iterator>;
               using base::cusp;
               using base::good;
               using base::halt;
               using base::mark;
               using base::look;
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

               flow( iterator& head, iterator tail) : base{ head, tail}, keep{ head} {}
               ~flow() { keep = mark; }

               auto spot() -> node
               {
                  auto data = scan();

                  if( auto* result = std::get_if< node>( &data))
                     return std::move( *result);

                  [[unlikely]] halt( "expected value");
               }

            private:

               iterator& keep;

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
                  return good() and not ( look() == ',' || look() == '}' || look() == ']' || ( look() == '#' && data.ends_with( ' ')));
               }

               info absent()
               {
                  name data;

                  while( more( data))
                  {
                     data.push_back( take());

                     if( data.back() == ':' && cusp())
                        return data.pop_back(), help::trim( std::move( data));
                  }

                  return resolve( help::trim( std::move( data)));
               }

            };

            template< help::sign type, help::source_iterator< type> iterator>
            struct block : core< type, iterator>
            {
               using base = parser::core< type, iterator>;
               using base::mark;
               using base::last;
               using base::good;
               using base::pull;
               using base::peek;
               using base::look;
               using base::take;
               using base::test;
               using base::read;
               using base::leap;
               using base::rest;
               using base::halt;
               using base::deny;
               using base::cusp;
               using base::peep;
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

               auto operator()() -> std::optional<node>
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
                     return read( [] ( const auto sign) { return ! help::is::space( sign); });
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
                     return info{ parser::flow< type, iterator>{ mark, last}.spot()};
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
                  if( help::hold< begin, cease>( data))
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
                     auto item = scan();

                     if( help::hold< begin, nill>( item))
                        dent = next();
                     else
                        return item;
                  }

                  return std::nullopt;
               }

               auto grab( const size root) -> node
               {
                  step();
                  auto item = scan();

                  if( help::hold< name>( item))
                     halt( "unexpected key");

                  wrap();

                  if( help::hold< node>( item))
                     return std::move( std::get< node>( std::move( item)));

                  if( root < dent || ( root == dent && peek() == '-'))
                     return *spot( dent);

                  return nullptr;
               }

               auto spot( const size root) -> std::optional< node>
               {
                  auto gist = head();

                  if( ! gist) return root ? std::optional< node>{ nullptr} : std::nullopt;

                  auto& item = *gist;

                  if( edge( item)) return node{ nullptr};

                  if( help::hold< node>( item))
                     return line(), dent = next(), std::get< node>( std::move( item));

                  if( help::hold< dash>( item))
                  {
                     node::array array;

                     do
                        array.emplace_back( *spot( dent + 1 + step()));
                     while( dent >= root && peek() == '-' && help::hold< dash>( scan()));

                     return { array};
                  }

                  node::object object;

                  while( good())
                  {
                     while( good() && help::hold< nill>( item))
                        line(), item = scan();

                     if( ! good())
                        break;

                     if( edge( item))
                        break;

                     object[ std::get< name>( std::move( item))] = grab( dent);

                     if( dent < root)
                        break;

                     wrap();

                     if( dent < root)
                        break;

                     item = scan();
                  }

                  return { object};
               }

               bool more( const std::string& data) const
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
                        auto key = help::trim( std::move( data));
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

                  return resolve( help::trim( std::move( data)));
               }

               bool idle() const
               {
                  return peek() == '\n' || peek() == '#';
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
                  auto item = scan();

                  if( std::holds_alternative< node>( item))
                     return anchors[ std::move( word)] = std::get< node>( std::move( item));

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
                  test( '!', pull());

                  const auto tag = read( []( const auto sign) { return help::is::lower( sign); });

                  tidy();

                  if( tag == "str")
                     if( peek() != '"' && peek() != '\'')
                        return node{ help::trim( scalar())};
   
                  auto item = scan();

                  if( std::holds_alternative< nill>( item))
                     item = node{ nullptr};

                  if( auto data = std::get_if< node>( &item))
                  {
                     if( data->is_string() && tag == "str")
                        return item;

                     if( data->is_nothing() && tag == "null")  
                        return item;

                     if( data->is_boolean() && tag == "bool")  
                        return item;

                     if( data->is_integer() && tag == "int")  
                        return item;

                     if( data->is_decimal() && tag == "float")
                        return item;

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
                  const auto style = take();
                  const auto chomp = peek() == '+' || peek() == '-' ? take() : '\0';

                  std::string text;
                  size root{};
                  size nada{};

                  while( good())
                  {
                     rest();

                     dent = step();

                     if( dent < root && peek() != '\n')
                        break;

                     const auto line = read( [] ( const auto sign) { return sign != '\n'; });

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
            return help::make::source< detail::parser::block>( source)().value();
         }
      } // one

      namespace all
      {
         auto parse( auto&& source)
         {
            auto parser = help::make::source< detail::parser::block>( source);

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

            template< help::sign type, help::target_iterator< type> iterator, bool strict>
            struct core : help::writer< type, iterator>
            {
               using base = help::writer< type, iterator>;
               using base::push;
               using base::copy;
               using base::cast;
               using base::time;
               using base::halt;

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
                     case '{': case '[': case '*': case '&': case '|': 
                     case '!': case '>': case '"': case '~': case '-': 
                     case '.': case '\'':case '?': case '`': case '%':
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
            };

            template< help::sign type, help::target_iterator< type> iterator, bool strict>
            struct flow : core< type, iterator, strict>
            {
               using base = writer::core< type, iterator, strict>;
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

            template< help::sign type, help::target_iterator< type> iterator, std::size_t spaces, bool strict>
            struct block : core< type, iterator, strict>
            {
               using base = writer::core< type, iterator, strict>;
               using base::mark;
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
                        std::visit( flow< type, iterator, strict>{ mark}, data);
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
                     flow< type, iterator, strict>{ mark}( node);
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

         template< std::size_t spaces, bool strict>
         auto write( const node& root, auto&& target)
         {
            auto sink = help::make::target< writer::block, spaces, strict>( target);
            std::visit( sink, root);
         }

         // the default write function
         template< std::size_t spaces, bool strict>
         auto write( const node& root)
         {
            std::string target;
            write< spaces, strict>( root, target);
            return target;
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

} // poly_version
