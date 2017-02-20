'use strict';
var common = require('../../common');
var assert = require('assert');
var addon = require(`./build/${common.buildType}/binding`);

addon.RunCallback(function(msg) {
  assert.equal(msg, 'hello world');
});

var global = ( function() { return this; } ).apply();

function testRecv(desiredRecv) {
  addon.RunCallbackWithRecv(function() {
    assert.equal(this,
      ( desiredRecv === undefined || desiredRecv === null ) ? global : desiredRecv );
  }, desiredRecv);
}

testRecv(undefined);
testRecv(null);
testRecv(5);
testRecv(true);
testRecv("Hello");
testRecv([]);
testRecv({});
