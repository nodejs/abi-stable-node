'use strict';
var common = require('../../common');
var assert = require('assert');
var addon = require(`./build/${common.buildType}/binding`);

var fn = addon();
assert.equal(fn(), 'hello world'); // 'hello world'
