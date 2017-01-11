'use strict';

var test_exception = require( "./build/Release/test_exception" );
var assert = require( "assert" );
var theError = new Error( "Some error" );

var returnedError = test_exception.returnException( function() {
	throw theError;
} );
assert.strictEqual( theError, returnedError,
	"Returned error is strictly equal to the thrown error" );

try {
	test_exception.allowException( function() {
		throw theError;
	} );
} catch ( anError ) {
	assert.strictEqual( anError, theError,
		"Thrown error was allowed to pass through unhindered" );
}
