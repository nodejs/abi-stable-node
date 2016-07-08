'use strict';
require('../../common');
var assert = require('assert');
var addon = require('./build/Release/binding');

var obj1 = addon.createObject(10);
var obj2 = addon.createObject(20);
var result = addon.add(obj1, obj2);
assert.equal(result, 30);
