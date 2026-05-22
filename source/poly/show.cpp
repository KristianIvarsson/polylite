#include <poly/json.hpp>
#include <poly/tool.hpp>
#include <print>

namespace local
{
   namespace
   {
      constexpr std::string_view json = R"json(
      {
         "user": {
            "id": 42,
            "name": "Kristian Ivarsson",
            "active": true,
            "profile": {
               "title": "Senior Software Engineer",
               "location": "Järvsö, Gävleborg County, Sweden",
               "skills": ["C++", "Python", "Biking", "Photography", "Skiing", "Hiking"],
               "preferences": {
               "remote": true,
               "notifications": false,
               "theme": null
               }
            }
         },
         "metadata": {
            "created_at": "2025-11-05T21:55:00Z",
            "valid": true
         }
      })json";
   } //
} // local

int main()
{
   auto root = poly::json::parse( poly::tool::bom::skip( local::json));

   // access with bounds checking
   const auto& user_name = root.at( "user").at( "name").as_string();
   std::println( "{}", user_name);

   // access by lookup
   if( const auto user_profile_skills = root( "user")( "profile")( "skills")(0))
   {
      const auto& first_skill = user_profile_skills->as_string();
      std::println( "{}", first_skill);              
   }

   // access and adding new data
   auto& user_profile_members = root[ "user"][ "profile"][ "members"];
   user_profile_members[ 0] = "Tony Iommi";
   user_profile_members[ 1] = "Geezer Butler";
   user_profile_members[ 2] = "Ozzy Osbourne";
   user_profile_members[ 3] = "Bill Ward";

   // remove some data
   root[ "user"][ "profile"].as_object().erase( "preferences");
   root.as_object().erase( "metadata");
   root[ "user"][ "profile"][ "skills"].as_array().pop_back();

   auto create = []
   {
      poly::node result;

      result[ "numbers"][ 0] = 123;
      result[ "numbers"][ 1] = false;
      result[ "numbers"][ 2] = true;
      result[ "numbers"][ 3] = nullptr;
      result[ "numbers"][ 4] = poly::node::object{ { "pi", 3.14}};

      return result;
   };

   // add child created wherever
   root[ "data"] = create();

   std::println();

   // print modified json compact
   //std::println( "{}", poly::json::compact::write( root));
   // print modified json elegant
   //std::println( "{}", poly::json::elegant::write<3>( root));
   // 3 is default so don't bother specifying it
   //std::println( "{}", poly::json::elegant::write( root));
   // and for confusion this will be the same as compact
   //std::println( "{}", poly::json::elegant::write< 0>( root));
   // and due to inline namespace this also works
   //std::println( "{}", poly::json::write( root));
   // or to an std::ostream
   //poly::json::write( root, std::cout);
   // but just go with the default elegant version
   //std::println( "{}", poly::json::write( root));
   // write whatever in the tree
   std::println( "{}", poly::json::write( root[ "user"]));

   return 0;
}