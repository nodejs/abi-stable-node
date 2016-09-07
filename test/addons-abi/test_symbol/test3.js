'use strict';
require('../../common');
var assert = require('assert');

// testing api calls for symbol
var test_symbol = require('./build/Release/test_symbol');


var foo1 = test_symbol.New('foo');
var foo2 = test_symbol.New('foo');
var object = {
  foo1: 1,
  foo2: 2,
};

assert(object.foo1 === 1);
assert(object.foo2 === 2);
