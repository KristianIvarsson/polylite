#include "poly/cbor.hpp"
#include "poly/json.hpp"
#include "poly/toml.hpp"
#include "poly/yaml.hpp"

#include <cassert>
#include <chrono>
#include <print>
#include <string>
#include <string_view>
#include <tuple>

namespace poly_version
{
   namespace pace
   {
      namespace detail
      {
         constexpr int default_iterations = 1000;

         struct result
         {
            std::string_view label;
            std::size_t      iterations;
            std::size_t      payload;
            std::chrono::nanoseconds write;
            std::chrono::nanoseconds parse;
         };

         template< typename fn>
         auto time( fn&& body)
         {
            const auto start = std::chrono::steady_clock::now();
            body();
            return std::chrono::steady_clock::now() - start;
         }

         void report( const result& r)
         {
            const auto write_ns = r.write.count() / r.iterations;
            const auto parse_ns = r.parse.count() / r.iterations;
            const auto round_ns = write_ns + parse_ns;
            const auto write_mb = r.payload * 1000.0 / static_cast< double>( write_ns);
            const auto parse_mb = r.payload * 1000.0 / static_cast< double>( parse_ns);

            std::println( " {:<14} {:>6} bytes  write {:>8} ns/op ({:>7.1f} MB/s)  parse {:>8} ns/op ({:>7.1f} MB/s)  round {:>8} ns/op",
                          r.label, r.payload, write_ns, write_mb, parse_ns, parse_mb, round_ns);
         }

         template< typename writer, typename parser>
         auto measure( std::string_view label, const node& source, std::size_t iterations, writer&& w, parser&& p)
         {
            // warm-up + correctness check
            const auto reference = w( source);
            std::ignore = p( reference);

            result r{ label, iterations, reference.size(), {}, {}};

            r.write = time( [&]
            {
               for( std::size_t i = 0; i < iterations; ++i)
               {
                  auto bytes = w( source);
                  // prevent the optimizer from discarding the call
                  asm volatile( "" :: "r"( bytes.data()) : "memory");
               }
            });

            r.parse = time( [&]
            {
               for( std::size_t i = 0; i < iterations; ++i)
               {
                  auto node = p( reference);
                  asm volatile( "" :: "r"( &node) : "memory");
               }
            });

            report( r);
            return r;
         }
      } // detail

      auto common()
      {
         // strict intersection supported by all four formats:
         // boolean, integer, decimal, string (plain/escaped/unicode), array, object, deep nesting
         // (no null, no binary, no instant — those are excluded for apples-to-apples comparison)

         node root;

         root[ "boolean"][ "t"] = true;
         root[ "boolean"][ "f"] = false;

         root[ "integer"][ "zero"]     = 0;
         root[ "integer"][ "positive"] = 1234567890L;
         root[ "integer"][ "negative"] = -1234567890L;

         root[ "decimal"][ "pi"]    = 3.14159265358979;
         root[ "decimal"][ "small"] = -0.0001234;
         root[ "decimal"][ "big"]   = 1.23e21;

         root[ "string"][ "plain"]   = "the quick brown fox jumps over the lazy dog";
         root[ "string"][ "escaped"] = "tab\there\nnew line\\slash\"quote";
         root[ "string"][ "unicode"] = "\xE2\x82\xAC \xF0\x9F\x98\x80 hello";
         root[ "string"][ "long"]    = std::string( 256, 'x');

         auto& mixed = root[ "array"][ "mixed"];
         mixed[ 0] = true;
         mixed[ 1] = 42;
         mixed[ 2] = 3.14;
         mixed[ 3] = "qwerty";

         for( int i = 0; i < 32; ++i)
            root[ "array"][ "numbers"][ i] = static_cast< node::integer>( i * i);

         for( int i = 0; i < 8; ++i)
         {
            auto& item = root[ "array"][ "objects"][ i];
            item[ "id"]     = static_cast< node::integer>( i);
            item[ "name"]   = "item";
            item[ "active"] = ( i % 2 == 0);
         }

         root[ "nested"][ "a"][ "b"][ "c"][ "d"][ "e"] = 42;

         return root;
      }

      namespace json
      {
         namespace cases
         {
            void all_constructs()
            {
               // strict json supports: null, bool, integer, decimal, string, array, object
               node source = pace::common();

               detail::measure( "json/elegant", source, detail::default_iterations,
                  []( const node& n) { return poly::json::elegant::write( n); },
                  []( const auto& s) { return poly::json::parse( s); });

               detail::measure( "json/compact", source, detail::default_iterations,
                  []( const node& n) { return poly::json::compact::write( n); },
                  []( const auto& s) { return poly::json::parse( s); });
            }
         } // cases

         void all()
         {
            cases::all_constructs();
         }
      } // json

      namespace toml
      {
         namespace cases
         {
            void all_constructs()
            {
               // strict toml supports common constructs except null at top level.
               node source = pace::common();

               detail::measure( "toml/elegant", source, detail::default_iterations,
                  []( const node& n) { return poly::toml::elegant::write( n); },
                  []( const auto& s) { return poly::toml::parse( s); });

               detail::measure( "toml/compact", source, detail::default_iterations,
                  []( const node& n) { return poly::toml::compact::write( n); },
                  []( const auto& s) { return poly::toml::parse( s); });
            }
         } // cases

         void all()
         {
            cases::all_constructs();
         }
      } // toml

      namespace yaml
      {
         namespace cases
         {
            void all_constructs()
            {
               // strict subset — only common constructs.
               node source = pace::common();

               detail::measure( "yaml/elegant", source, detail::default_iterations,
                  []( const node& n) { return poly::yaml::elegant::write( n); },
                  []( const auto& s) { return poly::yaml::parse( s); });

               detail::measure( "yaml/compact", source, detail::default_iterations,
                  []( const node& n) { return poly::yaml::compact::write( n); },
                  []( const auto& s) { return poly::yaml::parse( s); });
            }
         } // cases

         void all()
         {
            cases::all_constructs();
         }
      } // yaml

      namespace cbor
      {
         namespace cases
         {
            void all_constructs()
            {
               // cbor supports every node type natively
               node source = pace::common();

               detail::measure( "cbor/verbose", source, detail::default_iterations,
                  []( const node& n) { return poly::cbor::verbose::write( n); },
                  []( const auto& s) { return poly::cbor::parse( s); });

               detail::measure( "cbor/compact", source, detail::default_iterations,
                  []( const node& n) { return poly::cbor::compact::write( n); },
                  []( const auto& s) { return poly::cbor::parse( s); });
            }
         } // cases

         void all()
         {
            cases::all_constructs();
         }
      } // cbor

      void all()
      {
         std::println( "polylite pace -- {} iterations per measurement", detail::default_iterations);
         std::println( "");

         json::all();
         toml::all();
         yaml::all();
         cbor::all();
      }
   } // pace
} // poly_version


int main()
{
try
{
   poly::pace::all();
   return 0;
}
catch( const std::exception& e)
{
   std::println( stderr, "{}", e.what());
   return 1;
}
}
