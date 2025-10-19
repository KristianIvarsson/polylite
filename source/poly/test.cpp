#include "poly/json.hpp"

#include <stdexcept>
#include <cassert>
#include <fstream>
#include <print>

namespace poly
{
   namespace json
   {
      namespace test
      {
         namespace cases
         {
            void object()
            {
               node source;

               {
                  source[ "aaa"] = nullptr;
                  source[ "bbb"] = true;
                  source[ "ccc"] = -42;
                  source[ "ddd"] = 3.14;
                  source[ "eee"] = "qwerty";
                  source[ "fff"][ 0] = 123456;
                  source[ "ggg"][ "xxx"] = 123;
                  source[ "ggg"][ "yyy"] = 456;
                  source[ "ggg"][ "zzz"] = 789;
               }

               node target = parse( write( source));

               {
                  assert( target[ "aaa"].as_nothing() == nullptr);
                  assert( target[ "bbb"].as_boolean() == true);
                  assert( target[ "ccc"].as_integer() == -42);
                  assert( target[ "ddd"].as_decimal() == 3.14);
                  assert( target[ "eee"].as_string() == "qwerty");
                  assert( !target[ "fff"].as_array().empty());
                  assert( !target[ "ggg"].as_table().empty());

                  assert( target[ "ggg"][ "xxx"].is_numeric());
                  assert( target[ "ggg"][ "yyy"].is_numeric());
                  assert( target[ "ggg"][ "zzz"].is_numeric());

                  assert( target[ "aaa"].is_null());
                  assert( target[ "bbb"].is_true());
                  assert( !target[ "bbb"].is_false());
                  assert( target[ "ccc"].is_numeric());
                  assert( target[ "ddd"].is_numeric());
                  assert( target[ "eee"].is_scalar());
               }

               assert( target.at( "ggg").at( "yyy").as_integer() == 456);
               assert( target.at( "fff").at( 0).as_integer() == 123456);

               {
                  const node target = parse( write( source));

                  assert( target.at( "fff").at( 0).as_integer() == 123456);
                  assert( target.at( "ggg").at( "yyy").as_integer() == 456);

                  assert( target( "fff")( 0)->to_integer() != nullptr);
                  assert( target( "ggg")( "yyy")->to_integer() != nullptr);
               }
            }

            namespace detail
            {
               auto roundtrip( const node& node)
               {
                  const auto source = write( node);
                  const auto target = write( parse( source));
                  assert( source == target);
               }
            } // detail

            void scalar()
            {
               detail::roundtrip( { nullptr});
               detail::roundtrip( { true});
               detail::roundtrip( { false});
               detail::roundtrip( { 42});
               detail::roundtrip( { 3.14});
               detail::roundtrip( { " \\ hello \n\u0007\t world \\ "});
            }

            void string()
            {
               {
                  const auto s = json::parse( R"("\u0041")").as_string();
                  assert( s == "\x41");
               }

               {
                  const auto s = json::parse( R"("\u00A3")").as_string();
                  assert( s == "\xC2\xA3");
               }

               {
                  const auto s = json::parse( R"("\u20AC")").as_string();
                  assert( s == "\xE2\x82\xAC");
               }

               {
                  const auto s = json::parse( R"("\uD83D\uDE00")").as_string();
                  assert( s == "\xF0\x9F\x98\x80");
               }
            }

         } // cases

         void all()
         {
            cases::object();
            cases::scalar();
            cases::string();
         }
      } // test
   } // json
} // poly


int main( const int argc, const char* const argv[])
{
   try
   {
      if( argc > 1)
      {
         for( int idx = 1; idx < argc; ++idx)
         {
            if( std::ifstream file{ argv[ idx]})
               poly::json::parse( file);
            else
               throw std::runtime_error{ std::format( "failed to open file [{}]", argv[ idx])};
         }
      }
      else
      {
         poly::json::test::all();
      }

      return 0;
   }
   catch( const std::exception& e)
   {
      std::println( "{}", e.what());
      return 1;
   }
}
