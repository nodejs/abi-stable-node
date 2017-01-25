'use strict';
require('../../common');
var assert = require('assert');
var addon = require('./build/Release/binding');

assert.equal(addon.hello(), 'world');
