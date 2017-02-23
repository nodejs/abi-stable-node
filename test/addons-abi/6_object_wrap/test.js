'use strict';
var common = require('../../common');
var assert = require('assert');
var addon = require(`./build/${common.buildType}/binding`);

var obj = new addon.MyObject(9);
assert.ok(obj instanceof addon.MyObject);
assert.equal(obj.value, 9);
obj.value = 10;
assert.equal(obj.value, 10);
assert.equal(obj.plusOne(), 11);
assert.equal(obj.plusOne(), 12);
assert.equal(obj.plusOne(), 13);

assert.equal(obj.multiply().value, 13);
assert.equal(obj.multiply(10).value, 130);

var newobj = obj.multiply(-1);
assert.equal(newobj.value, -13);
assert(obj !== newobj);
