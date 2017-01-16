var addon = require( "./build/Release/test_instanceof" );
function F() {}
var x = 1;
addon.doInstanceOf( x, F );
