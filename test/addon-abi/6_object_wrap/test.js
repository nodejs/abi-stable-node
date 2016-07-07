'use strict';
require('../../common');
var assert = require('assert');
var addon = require('./build/Release/binding');

var obj = new addon.MyObject(10);
assert.equal(obj.plusOne(), 11);
assert.equal(obj.plusOne(), 12);
assert.equal(obj.plusOne(), 13);

assert.equal(obj.multiply().napi_value() , 13);
assert.equal(obj.multiply(10).napi_value(), 130);

var newobj = obj.multiply(-1);
assert.equal(newobj.napi_value(), -13);
assert(obj !== newobj);
