'use strict';
require('../../common');
var assert = require('assert');

// testing api calls for symbol
var test_symbol = require('./build/Release/test_symbol');

var sym = test_symbol.New('test');
assert.equal(sym.toString(), 'test');


var myObj = {};
var fooSym = test_symbol.New('foo');
var otherSym = test_symbol.New('bar');
myObj['foo'] = 'bar';
myObj[fooSym] = 'baz';
myObj[otherSym] = 'bing';

assert(myObj.foo === 'baz');
assert(myObj[fooSym] === 'baz');
assert(myObj[otherSym] === 'bing');
