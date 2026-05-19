//
// Copyright (c) 2025 Kristian Ivarsson
//
// Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
//

#pragma once

#include "help.hpp"

#include <format>
#include <ranges>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <spanstream>


namespace poly_version
{
   namespace toml
   {
      namespace detail
      {
         constexpr auto bare = [] ( const auto sign)
         {
            return help::is::alnum( sign) || sign == '_' || sign == '-';
         };

         struct parser : help::stream::buffer::iterator::parser
         {
            using base::base;

            auto operator()() -> node
            {
               node nrv = node::object{};

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

                     for( auto&& name : keys | std::views::take( keys.size() - 1) | std::views::as_rvalue)
                        if( where = &(*where)[ std::move( name)]; where->is_array() && ! where->as_array().empty())
                           where = &where->as_array().back();

                     where = &(*where)[ std::move( keys.back())];

                     if( peek() == ']')
                     {
                        ++mark; // ']'

                        if( ! where->is_array())
                           *where = node::array{};
                        
                        where = &where->as_array().emplace_back( node::object{});
                     }
                     else
                     {
                        if( ! where->is_object())
                           *where = node::object{};
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
                  return scalar();
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

            auto keys() -> std::vector< node::object::key_type>
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

               if( peek() == '#')
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

            auto cast( const auto sign) -> std::int32_t
            {
               switch( sign)
               {
               case 'U': return unit< 8>();
               default:  return base::cast( sign);
               }
            }

            auto table() -> node::object
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

               return std::move( nrv).as_object();
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
                     if( same || ( peek() != '"' && [&nrv] { return nrv.ends_with( R"("")") ? nrv.erase( nrv.size() - 2), true : false; }()))
                        return nrv;

                  if( sign != '\\') [[likely]]
                     nrv.push_back( sign);
                  else  
                     if( peek() != '\n' && same)
                        nrv.append( help::transform::point( cast( pull())));
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
                     if( same || ( peek() != '\'' && [&nrv] { return nrv.ends_with( R"('')") ? nrv.erase( nrv.size() - 2), true : false; }()))
                        return nrv;

                  nrv.push_back( sign);
               }
            }

            node simple()
            {
               const auto data = read( []( const auto sign) 
                  { 
                     return help::is::lower( sign); 
                  });
            
               if( auto result = help::transform::simple( data))
                  return std::move( *result);

               [[unlikely]] this->halt( "unexpected data");
            }

            node scalar()
            {
               auto data = read( []( const auto sign)
                  {
                     switch( sign)
                     case '.': case '+': case '-': case '_': case ':': return true;
                     return help::is::alnum( sign);
                  });

               if( auto result = help::transform::instant( data))
                  return std::move( *result);

               std::erase( data, '_');

               if( auto result = help::transform::number( data))
                  return std::move( *result);

               [[unlikely]] this->halt( "unexpected data");
            }
         };

      } // detail

      inline auto parse( std::istream& stream)
      {
         return detail::parser{ stream}();
      }

      inline auto parse( std::string_view data)
      {
         std::ispanstream stream{ data};
         return parse( stream);
      }

      namespace detail
      {
         template< typename type>
         concept number = std::same_as< type, node::integer> || std::same_as< type, node::decimal>;

         struct numeric : std::numpunct< char>
         {
            char do_thousands_sep()   const override { return '_'; }
            std::string do_grouping() const override { return "\3"; }
         };

         template< bool posh, bool firm>
         struct writer : help::stream::buffer::iterator::writer
         {
            using base::base;

            void operator()( const node& node)
            {
               if( node.is_object())
                  (*this)( node.as_object());
               else
                  if constexpr( firm)
                     halt( "not node::object");
                  else
                     (*this)( node::object{ { {}, node}});
            }

            void operator()( const node::object& node)
            {
               if( node.empty() || ! std::ranges::all_of( node, section, &node::object::value_type::second))
                  table();

               for( const auto& [ name, data] : node)
                  if( ! section( data))
                  {
                     key( name);
                     copy( " = ");
                     std::visit( *this, data);
                     push( '\n');
                  }

               for( const auto& [ name, data] : node)
                  if( section( data))
                  {
                     stack.emplace_back( name);
                     std::visit( *this, data);
                     stack.pop_back();
                  }
            }

            void operator()( const node::array& node)
            {
               auto comma = [this] ( auto& tail)
               {
                  if( --tail) copy( ", ");
               };


               if( node.empty() || trivial( node) || ! std::ranges::all_of( node, &node::is_object))
               {
                  push( '[');
                  auto commas = node.size();
                  for( const auto& data : node)
                  {
                     place( data);
                     comma( commas);
                  }
                  push( ']');
               }
               else
               {
                  auto section = [&]
                  {
                     for( const auto& data : node)
                     {
                        array();

                        const auto& table = data.as_object();

                        for( const auto& [ name, data] : table)
                           if( trivial( data))
                           {
                              key( name);
                              copy( " = ");
                              std::visit( *this, data);
                              push( '\n');
                           }

                        for( const auto& [ name, data] : table)
                           if( complex( data))
                           {
                              stack.emplace_back( name);
                              std::visit( *this, data);
                              stack.pop_back();
                           }
                     }
                  };

                  if constexpr( posh)
                  {
                     section();
                  }
                  else
                  {
                     if( shallow( node))
                     {
                        push( '[');
                        auto tail = node.size();
                        for( const auto& data : node)
                        {
                           copy( "{ ");
                           auto commas = data.as_object().size();
                           for( const auto& [ name, val] : data.as_object())
                           {
                              key( name);
                              copy( " = ");
                              std::visit( *this, val);
                              comma( commas);
                           }
                           copy( " }");
                           comma( tail);
                        }
                        push( ']');
                     }
                     else
                     {
                        section();
                     }
                  }
               }
            }

            void operator()( const node::nothing& )
            {
               if constexpr( firm)
                  halt( "node::nothing");
               else
                  copy( R"("null")");
            }

            void operator()( const node::boolean& node)
            {
               copy( node ? "true" : "false");
            }

            void operator()( const number auto& node)
            {
               if constexpr( posh)
               {
                  static const std::locale locale{ std::locale::classic(), new numeric{}};
                  copy( std::format( locale, "{:L}", node));
               }
               else
               {
                  copy( std::format( "{}", node));
               }
            }

            void operator()( const node::instant& node)
            {
               std::visit( [ this]( const auto& data) { time( data); }, node);
            }

            void operator()( const node::string& node)
            {
               if constexpr( posh)
                  if( ! node.contains( '\'') && std::ranges::none_of( node, []( const auto sign){ return help::is::cntrl( sign); }))
                     return push( '\''), copy( node), push( '\'');

               push( '"'), cast( node), push( '"');
            }

            void operator()( const node::binary& node)
            {
               if constexpr( firm)
                  halt( "node::binary");

               if constexpr( posh)
               {
                  copy( "'''");
                  data( node);
                  copy( "'''");
               }
               else
               {
                  push( '"');
                  data< 0>( node);
                  push( '"');
               }
            }

         private:

            void key( const auto& name)
            {
               if( ! name.empty() && std::ranges::all_of( name, bare))
                  copy( name); // bare key
               else
                  (*this)( node::string{ name}); // quoted key
            }

            void place( const node& node)
            {
               if( node.is_trivial())
                  return std::visit( *this, node);

               if( node.is_object())
               {
                  copy( "{ ");
                  auto tail = node.as_object().size();
                  for( const auto& [ name, data] : node.as_object())
                  {
                     key( name);
                     copy( " = ");
                     place( data);
                     if( --tail) copy( ", ");
                  }
                  copy( " }");
               }

               if( node.is_array())
               {
                  push( '[');
                  auto tail = node.as_array().size();
                  for( const auto& item : node.as_array())
                  {
                     place( item);
                     if( --tail) copy( ", ");
                  }
                  push( ']');
               }
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

            static bool complex( const node& item) noexcept
            {
               if( item.is_trivial()) return false;
               if( item.is_object()) return true;
               return std::ranges::any_of( item.as_array(), complex);
            }

            static bool trivial( const node& item) noexcept
            {
               if( item.is_trivial()) return true;
               if( item.is_object()) return false;
               return std::ranges::any_of( item.as_array(), trivial);
            }

            static bool shallow( const node::array& array) noexcept
            {
               return std::ranges::all_of( array, [] ( const node& item)
                  {
                     return item.is_object() && std::ranges::all_of( item.as_object(), trivial, &node::object::value_type::second);
                  });
            }

            static bool section( const node& item) noexcept
            {
               if( item.is_object()) return true;
               if( ! item.is_array()) return false;
               if( ! std::ranges::all_of( item.as_array(), &node::is_object)) return false;
               if constexpr( posh) return true;
               return ! shallow( item.as_array());
            }

         private:

            std::vector< std::string_view> stack;
         };


         template< bool posh, bool firm>
         auto write( const node& node, std::ostream& stream)
         {
            writer< posh, firm>{ stream}( node);
         }

         template< bool posh, bool firm>
         auto write( const node& node)
         {
            std::ostringstream stream;
            write< posh, firm>( node, stream);
            return std::move( stream).str();
         }

      } // detail

      inline namespace elegant
      {
         inline namespace strict
         {
            auto write( auto&&... parameters)
            {
               return detail::write< true, true>( std::forward< decltype( parameters)>( parameters)...);
            }
         } // strict

         namespace gentle
         {
            auto write( auto&&... parameters)
            {
               return detail::write< true, false>( std::forward< decltype( parameters)>( parameters)...);
            }
         } // gentle
      } // elegant

      namespace compact
      {
         inline namespace strict
         {
            auto write( auto&&... parameters)
            {
               return detail::write< false, true>( std::forward< decltype( parameters)>( parameters)...);
            }
         } // strict

         namespace gentle
         {
            auto write( auto&&... parameters)
            {
               return detail::write< false, false>( std::forward< decltype( parameters)>( parameters)...);
            }
         } // gentle
      } // compact

   } // toml

} // version
