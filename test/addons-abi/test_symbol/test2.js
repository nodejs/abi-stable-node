'use strict';
require('../../common');
var assert = require('assert');

// testing api calls for symbol
var test_symbol = require('./build/Release/test_symbol');

var fooSym = test_symbol.New('foo');
var myObj = {};
myObj['foo'] = 'bar';
myObj[fooSym] = 'baz';
Object.keys(myObj); // -> [ 'foo' ]
Object.getOwnPropertyNames(myObj); // -> [ 'foo' ]
Object.getOwnPropertySymbols(myObj); // -> [ Symbol(foo) ]
assert(Object.getOwnPropertySymbols(myObj)[0] === fooSym);
