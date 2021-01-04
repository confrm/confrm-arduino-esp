#include <string>

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#define CPP_STANDARD
#include "../src/simple_json.h"

TEST_CASE( "JSON Reading", "[simple_json]" ) {

  std::string myjson = 
    "{\n"
    "  \"Key1\": \"value      \",\n"
    "  \"Key2\" : 2,\n"
    "  \"Key3\" : true,\n"
    "  \"Key4\" : -42 ,\n"
    "  \"Key5\" : false,\n"
    "  \"Key6\" : true,\n"
    "  \"Key7\" : true ,\n"
    "  \"Key8\" : true , \n"
    "  \"Key9\" :true,\n"
    "\"Key10\"  :-42 ,\n"
    "  \"Key11\" :42\n"
    "}";

  std::vector<SimpleJSONElement> result = simple_json(myjson);

  REQUIRE( result.size() == 11 );

  REQUIRE( result[0].key == "Key1" );
  REQUIRE( result[0].type == STRING );
  REQUIRE( result[0].value_string == "value      " );

  REQUIRE( result[1].key == "Key2" );
  REQUIRE( result[1].type == NUMBER );
  REQUIRE( result[1].value_number == 2 );

  REQUIRE( result[2].key == "Key3" );
  REQUIRE( result[2].type == BOOLEAN );
  REQUIRE( result[2].value_boolean == true );

  REQUIRE( result[3].key == "Key4" );
  REQUIRE( result[3].type == NUMBER );
  REQUIRE( result[3].value_number == -42 );

  REQUIRE( result[4].key == "Key5" );
  REQUIRE( result[4].type == BOOLEAN );
  REQUIRE( result[4].value_boolean == false );

  REQUIRE( result[5].key == "Key6" );
  REQUIRE( result[5].type == BOOLEAN );
  REQUIRE( result[5].value_boolean == true );

  REQUIRE( result[6].key == "Key7" );
  REQUIRE( result[6].type == BOOLEAN );
  REQUIRE( result[6].value_boolean == true );

  REQUIRE( result[7].key == "Key8" );
  REQUIRE( result[7].type == BOOLEAN );
  REQUIRE( result[7].value_boolean == true );

  REQUIRE( result[8].key == "Key9" );
  REQUIRE( result[8].type == BOOLEAN );
  REQUIRE( result[8].value_boolean == true );

  REQUIRE( result[9].key == "Key10" );
  REQUIRE( result[9].type == NUMBER );
  REQUIRE( result[9].value_number == -42 );

  REQUIRE( result[10].key == "Key11" );
  REQUIRE( result[10].type == NUMBER );
  REQUIRE( result[10].value_number == 42 );

  std::string real_test = "{\"current_version\":\"0.5.1\",\"blob\":\"9656f840e8d94170b9d99ead29bb3d78\",\"hash\":\"4ac64de7167d6b21d1bf4370d27987e3812b556048c9641584fa3ea66a7a6e4c\",\"force\":true}";
  std::vector<SimpleJSONElement> result2 = simple_json(real_test);

  REQUIRE( result2.size() == 4 );

  REQUIRE( result2[0].key == "current_version" );
  REQUIRE( result2[0].type == STRING );
  REQUIRE( result2[0].value_string == "0.5.1" );

  REQUIRE( result2[3].key == "force" );
  REQUIRE( result2[3].type == BOOLEAN );
  REQUIRE( result2[3].value_string == "true" );
  REQUIRE( result2[3].value_boolean == true );

  
  REQUIRE (get_simple_json_bool(result2, "force") == true);

}
