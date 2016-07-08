'use strict';
require('../../common');
var assert = require('assert');
var addon = require('./build/Release/binding');

var fn = addon();
assert.equal(fn(), 'hello world'); // 'hello world'
