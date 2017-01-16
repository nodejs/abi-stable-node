'use strict';

var test_exception = require( "./build/Release/test_exception" );
var assert = require( "assert" );
var theError = new Error( "Some error" );
var throwTheError = function() {
	throw theError;
};

// Test that the native side successfully captures the exception
var returnedError = test_exception.returnException( throwTheError );
assert.strictEqual( theError, returnedError,
	"Returned error is strictly equal to the thrown error" );

// Test that the native side passes the exception through
try {
	test_exception.allowException( throwTheError );
} catch ( anError ) {
	assert.strictEqual( anError, theError,
		"Thrown error was allowed to pass through unhindered" );
}

// Test that the exception thrown above was marked as pending before it was handled on the JS side
assert.strictEqual( test_exception.wasPending(), true,
	"VM was marked as having an exception pending when it was allowed through" );
