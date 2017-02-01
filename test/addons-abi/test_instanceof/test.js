var fs = require( 'fs' );

require('../../common');
var assert = require('assert');
var addon = require('./build/Release/test_instanceof');
var path = require( 'path' );

function assertTrue(assertion) {
  return assert.strictEqual(true, assertion);
}

function assertFalse(assertion) {
  assert.strictEqual(false, assertion);
}

function assertEquals(leftHandSide, rightHandSide) {
  assert.equal(leftHandSide, rightHandSide);
}

function assertThrows(statement) {
  assert.throws(function() {
    eval(statement);
  }, Error);
}

function testFile( fileName ) {
  eval( fs.readFileSync( fileName, { encoding: 'utf8' } )
    .replace( /[(]([^\s(]+)\s+instanceof\s+([^)]+)[)]/g,
      '( addon.doInstanceOf( $1, $2 ) )' ) );
}

testFile(
  path.join( path.resolve( __dirname, '..', '..', '..', 'deps', 'v8', 'test', 'mjsunit' ),
    'instanceof.js' ) );
testFile(
  path.join( path.resolve( __dirname, '..', '..', '..', 'deps', 'v8', 'test', 'mjsunit' ),
    'instanceof-2.js' ) );
