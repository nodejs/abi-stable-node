'use strict';
var common = require('../../common');
var assert = require('assert');
var addon = require(`./build/${common.buildType}/binding`);

assert.equal(addon.hello(), 'world');
