'use strict';
var common = require('../../common');
var assert = require('assert');
var createObject = require(`./build/${common.buildType}/binding`);

var obj = createObject(10);
assert.equal(obj.plusOne(), 11);
assert.equal(obj.plusOne(), 12);
assert.equal(obj.plusOne(), 13);

var obj2 = createObject(20);
assert.equal(obj2.plusOne(), 21);
assert.equal(obj2.plusOne(), 22);
assert.equal(obj2.plusOne(), 23);
